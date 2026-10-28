/* Production manager + SDK + boot hook, with real SHA-256 and a persistent
 * host flash/NVS backend. Does not execute ESP32 machine code or emulate NVS
 * page recovery, radio, display, electrical timing or physical torn writes. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../main/manager.c"
#include "../main/storage.c"
#include "../main/autostart.c"
#include "../components/passport_sdk/passport_sdk.c"
#include "../main/compat.c"
#include "../bootloader_components/passport_boot/hooks.c"

enum {FLASH_BYTES=0x800000, MAX_RECORDS=32, MAX_HANDLES=32};
typedef struct {bool used; char part[16], ns[16], key[16]; unsigned type; size_t size; uint8_t value[17000];} record_t;
typedef struct {bool used; char part[16], ns[16]; int mode;} handle_t;
typedef struct {uint8_t flash[FLASH_BYTES]; record_t records[MAX_RECORDS]; int selected;} persistent_t;
static persistent_t disk, baseline;
static handle_t handles[MAX_HANDLES];
static bool initialized[18], lock_available;
static const char *labels[]={"pos_meta","appdata0","appdata1","appdata2","nvs","appdata3","appdata4","appdata5","appdata6","appdata7","appdata8","appdata9","appdataa","appdatab","appdatac","appdatad","appdatae","appdataf"};
static esp_partition_t parts[18]={{1,2,PASSPORT_META_ADDRESS,0x10000},{1,2,0x7a0000,0x20000},
    {1,2,0x7c0000,0x20000},{1,2,0x7e0000,0x20000},{1,2,0x9000,0x4000}};
static esp_partition_t apps[PASSPORT_SLOTS]={{0,16,0x190000,PASSPORT_SLOT_SIZE},
    {0,17,0x390000,PASSPORT_SLOT_SIZE},{0,18,0x590000,PASSPORT_SLOT_SIZE}};
static const esp_partition_t factory={0,0,0x10000,0x180000};
static const esp_partition_t *running;
static esp_partition_t executing;
static rtc_retain_mem_t rtc;
static jmp_buf restart_jump, boot_jump;
static int reset_reason, loaded, io_count, fail_at;
static unsigned boot_slot;
static bool fail_after_write;
static const uint8_t *stream;
static size_t stream_size, stream_offset, ota_offset, ota_size;
static unsigned ota_slot;
static bool ota_active;
typedef struct {uint8_t bytes[PASSPORT_SLOT_SIZE+512];size_t size;} fixture_t;
static fixture_t fixture[10];

static bool fault(void) {return ++io_count==fail_at;}
static int partition_index(const char *label)
{for(unsigned i=0;i<18;i++)if(!strcmp(label,labels[i]))return (int)i;assert(false);return -1;}
static record_t *find_record(const char *part,const char *ns,const char *key)
{
    for(unsigned i=0;i<MAX_RECORDS;i++) {
        if(disk.records[i].used&&!strcmp(disk.records[i].part,part)&&
           !strcmp(disk.records[i].ns,ns)&&!strcmp(disk.records[i].key,key))return &disk.records[i];
    }
    return NULL;
}
static record_t *new_record(const char *part,const char *ns,const char *key)
{
    record_t *r=find_record(part,ns,key);if(r)return r;
    for(unsigned i=0;i<MAX_RECORDS;i++)if(!disk.records[i].used){r=&disk.records[i];*r=(record_t){.used=true};
        strcpy(r->part,part);strcpy(r->ns,ns);strcpy(r->key,key);return r;}
    assert(false);return NULL;
}
static handle_t *get_handle(nvs_handle_t h)
{assert(h>0&&h<MAX_HANDLES&&handles[h].used);return &handles[h];}
static passport_registry_t stored_registry(void)
{
    record_t *r=find_record("pos_meta","passport","registry_v2");passport_registry_t out;
    if(!r){passport_registry_init(&out);return out;}
    assert(r->size==sizeof(out));memcpy(&out,r->value,sizeof(out));return out;
}
static void raw_sha(const void *data,size_t size,uint8_t output[32])
{unsigned length=0;assert(EVP_Digest(data,size,output,&length,EVP_sha256(),NULL)==1&&length==32);}
void mbedtls_sha256_init(mbedtls_sha256_context *c) {c->context=EVP_MD_CTX_new();assert(c->context);}
void mbedtls_sha256_free(mbedtls_sha256_context *c) {EVP_MD_CTX_free(c->context);c->context=NULL;}
int mbedtls_sha256_starts(mbedtls_sha256_context *c,int is224)
{assert(!is224);return EVP_DigestInit_ex(c->context,EVP_sha256(),NULL)==1?0:-1;}
int mbedtls_sha256_update(mbedtls_sha256_context *c,const unsigned char *p,size_t n)
{return EVP_DigestUpdate(c->context,p,n)==1?0:-1;}
int mbedtls_sha256_finish(mbedtls_sha256_context *c,unsigned char out[32])
{unsigned n=0;return EVP_DigestFinal_ex(c->context,out,&n)==1&&n==32?0:-1;}
int mbedtls_sha256(const unsigned char *p,size_t n,unsigned char out[32],int is224)
{assert(!is224);raw_sha(p,n,out);return 0;}

SemaphoreHandle_t xSemaphoreCreateMutex(void) {lock_available=true;return &lock_available;}
int xSemaphoreTake(SemaphoreHandle_t m,int timeout) {(void)timeout;if(!*m)return 0;*m=false;return pdTRUE;}
void xSemaphoreGive(SemaphoreHandle_t m) {*m=true;}
esp_err_t esp_partition_register_external(void *chip,size_t address,size_t size,const char *label,unsigned type,unsigned subtype,const esp_partition_t **out)
{
    assert(!chip);esp_partition_t *p;
    if(type==0){assert(subtype>=16&&subtype<16+PASSPORT_SLOTS);p=&apps[subtype-16];}
    else p=&parts[partition_index(label)];
    *p=(esp_partition_t){type,subtype,address,size};*out=p;return ESP_OK;
}
esp_err_t esp_partition_deregister_external(const esp_partition_t *p){(void)p;return ESP_OK;}
esp_err_t esp_flash_read(void *chip,void *out,uint32_t address,uint32_t size)
{assert(!chip&&address+size<=FLASH_BYTES);if(fault())return ESP_FAIL;memcpy(out,disk.flash+address,size);return ESP_OK;}
const esp_partition_t *esp_partition_find_first(unsigned type,unsigned subtype,const char *label)
{
    if(type==0){if(subtype==0)return &factory;if(subtype>=16&&subtype<16+PASSPORT_SLOTS)return &apps[subtype-16];return NULL;}
    assert(type==1&&subtype==2&&label);return &parts[partition_index(label)];
}
const esp_partition_t *esp_ota_get_running_partition(void) {return running;}
esp_err_t esp_partition_read(const esp_partition_t *p,size_t offset,void *out,size_t n)
{assert(offset+n<=p->size);if(fault())return ESP_FAIL;memcpy(out,disk.flash+p->address+offset,n);return ESP_OK;}
esp_err_t esp_partition_erase_range(const esp_partition_t *p,size_t offset,size_t n)
{
    assert(offset+n<=p->size);
    if(p->type==0) {
        assert(p->subtype>=16&&p->subtype<16+PASSPORT_SLOTS);
        passport_registry_t r=stored_registry();assert(r.slots[p->subtype-16].state==PASSPORT_RESERVED);
    } else assert(p==&parts[4]);
    bool fail=fault();if(fail&&!fail_after_write)return ESP_FAIL;
    memset(disk.flash+p->address+offset,255,n);return fail?ESP_FAIL:ESP_OK;
}
esp_err_t esp_partition_write(const esp_partition_t *p,size_t offset,const void *data,size_t n)
{
    assert(p==&parts[4]&&offset+n<=p->size);
    bool fail=fault();if(fail&&!fail_after_write)return ESP_FAIL;
    memcpy(disk.flash+p->address+offset,data,n);return fail?ESP_FAIL:ESP_OK;
}
esp_err_t nvs_flash_init_partition(const char *label)
{if(fault())return ESP_FAIL;initialized[partition_index(label)]=true;return ESP_OK;}
esp_err_t nvs_flash_deinit_partition(const char *label)
{if(fault())return ESP_FAIL;initialized[partition_index(label)]=false;return ESP_OK;}
esp_err_t nvs_open_from_partition(const char *part,const char *ns,int mode,nvs_handle_t *h)
{
    assert(initialized[partition_index(part)]&&strlen(ns)<16);if(fault())return ESP_FAIL;
    bool exists=false;for(unsigned i=0;i<MAX_RECORDS;i++)if(disk.records[i].used&&
        !strcmp(disk.records[i].part,part)&&!strcmp(disk.records[i].ns,ns))exists=true;
    if(!exists&&mode==NVS_READONLY)return ESP_ERR_NVS_NOT_FOUND;
    for(int i=1;i<MAX_HANDLES;i++)if(!handles[i].used){handles[i]=(handle_t){.used=true,.mode=mode};
        strcpy(handles[i].part,part);strcpy(handles[i].ns,ns);*h=i;return ESP_OK;}
    assert(false);return ESP_FAIL;
}
static esp_err_t get_value(nvs_handle_t h,const char *key,void *out,size_t *size,unsigned type)
{
    handle_t *handle=get_handle(h);if(fault())return ESP_FAIL;
    record_t *r=find_record(handle->part,handle->ns,key);if(!r)return ESP_ERR_NVS_NOT_FOUND;
    assert(r->type==type);if(*size<r->size)return ESP_ERR_INVALID_SIZE;
    memcpy(out,r->value,r->size);*size=r->size;return ESP_OK;
}
static esp_err_t set_value(nvs_handle_t h,const char *key,const void *value,size_t size,unsigned type)
{
    handle_t *handle=get_handle(h);assert(handle->mode==NVS_READWRITE&&strlen(key)<16&&size<=17000);
    bool fail=fault();if(fail&&!fail_after_write)return ESP_FAIL;
    record_t *r=new_record(handle->part,handle->ns,key);r->size=size;r->type=type;memcpy(r->value,value,size);
    disk.flash[parts[partition_index(handle->part)].address]=0x5a;
    return fail?ESP_FAIL:ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size) {return get_value(h,key,out,size,1);}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *value,size_t size) {return set_value(h,key,value,size,1);}
esp_err_t nvs_set_i32(nvs_handle_t h,const char *key,int32_t value) {return set_value(h,key,&value,4,2);}
esp_err_t nvs_get_i32(nvs_handle_t h,const char *key,int32_t *value) {size_t n=4;return get_value(h,key,value,&n,2);}
esp_err_t nvs_set_str(nvs_handle_t h,const char *key,const char *value) {return set_value(h,key,value,strlen(value)+1,3);}
esp_err_t nvs_commit(nvs_handle_t h) {get_handle(h);return fault()?ESP_FAIL:ESP_OK;}
void nvs_close(nvs_handle_t h) {get_handle(h)->used=false;}
esp_err_t nvs_flash_erase_partition(const char *label)
{
    int i=partition_index(label);assert(i>0);passport_registry_t r=stored_registry();assert(r.slots[i<4?i-1:i-2].state==PASSPORT_RESERVED || r.slots[i<4?i-1:i-2].state==PASSPORT_INSTALLING);
    bool fail=fault();if(fail&&!fail_after_write)return ESP_FAIL;
    for(unsigned j=0;j<MAX_RECORDS;j++)if(disk.records[j].used&&!strcmp(disk.records[j].part,label))disk.records[j].used=false;
    memset(disk.flash+parts[i].address,255,parts[i].size);initialized[i]=false;return fail?ESP_FAIL:ESP_OK;
}
esp_err_t nvs_entry_find(const char *part,const char *ns,unsigned type,nvs_iterator_t *it)
{
    (void)ns;assert(type==NVS_TYPE_ANY);*it=NULL;if(fault())return ESP_FAIL;
    for(unsigned i=0;i<MAX_RECORDS;i++) {
        if(disk.records[i].used&&!strcmp(disk.records[i].part,part))return ESP_OK;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}
void nvs_release_iterator(nvs_iterator_t it) {(void)it;}

static bool image_ok(const esp_partition_t *p,size_t size)
{
    const uint8_t *data=disk.flash+p->address;if(size<288||size>p->size||data[0]!=0xe9||data[1]<1||data[1]>16)return false;
    size_t cursor=24;uint8_t sum=0xef;
    for(unsigned i=0;i<data[1];i++) {
        if(cursor+8>size)return false;
        uint32_t n;memcpy(&n,data+cursor+4,4);cursor+=8;
        if(n>size-cursor)return false;
        for(unsigned j=0;j<n;j++)sum^=data[cursor+j];
        cursor+=n;
    }
    size_t end=(cursor+16)&~(size_t)15;if(end+32!=size||data[end-1]!=sum)return false;
    uint8_t sha[32];raw_sha(data,end,sha);return !memcmp(sha,data+end,32);
}
int httpd_req_recv(httpd_req_t *req,char *out,size_t n)
{assert(req->content_len==stream_size);if(fault())return -1;if(n>97)n=97;
 assert(stream_offset+n<=stream_size);memcpy(out,stream+stream_offset,n);stream_offset+=n;return (int)n;}
esp_err_t esp_ota_begin(const esp_partition_t *p,size_t size,esp_ota_handle_t *h)
{
    assert(!ota_active&&p->subtype>=16&&p->subtype<16+PASSPORT_SLOTS&&size<=p->size);
    passport_registry_t r=stored_registry();assert(r.slots[p->subtype-16].state==PASSPORT_INSTALLING);
    bool fail=fault();if(fail&&!fail_after_write)return ESP_FAIL;
    memset(disk.flash+p->address,255,(size+4095)&~(size_t)4095);
    if(fail)return ESP_FAIL;
    ota_slot=p->subtype-16;ota_size=size;ota_offset=0;ota_active=true;*h=1;return ESP_OK;
}
esp_err_t esp_ota_write(esp_ota_handle_t h,const void *data,size_t n)
{
    assert(h==1&&ota_active&&ota_offset+n<=ota_size);bool fail=fault();if(fail&&!fail_after_write)return ESP_FAIL;
    uint8_t *dest=disk.flash+apps[ota_slot].address+ota_offset;const uint8_t *bytes=data;
    for(size_t i=0;i<n;i++){assert((dest[i]&bytes[i])==bytes[i]);dest[i]=bytes[i];}
    ota_offset+=n;return fail?ESP_FAIL:ESP_OK;
}
esp_err_t esp_ota_end(esp_ota_handle_t h)
{assert(h==1&&ota_active);ota_active=false;if(fault())return ESP_FAIL;return ota_offset==ota_size&&image_ok(&apps[ota_slot],ota_size)?ESP_OK:ESP_ERR_OTA_VALIDATE_FAILED;}
esp_err_t esp_ota_abort(esp_ota_handle_t h) {assert(h==1);ota_active=false;return ESP_OK;}
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p)
{
    passport_registry_t r=stored_registry();unsigned slot=p->subtype-16;
    assert(slot<PASSPORT_SLOTS&&image_ok(p,r.slots[slot].package.image_size));bool fail=fault();if(fail&&!fail_after_write)return ESP_FAIL;
    disk.selected=(int)slot;return fail?ESP_FAIL:ESP_OK;
}
rtc_retain_mem_t *bootloader_common_get_rtc_retain_mem(void) {return &rtc;}
void esp_restart(void) {longjmp(restart_jump,1);}
int64_t esp_timer_get_time(void) {return 1000000;}
esp_err_t bootloader_flash_unlock(void){return ESP_OK;}
esp_err_t bootloader_flash_read(size_t address,void *out,size_t size,bool decrypt)
{assert(address+size<=FLASH_BYTES&&!decrypt);memcpy(out,disk.flash+address,size);return ESP_OK;}
esp_err_t bootloader_flash_write(size_t address,const void *in,size_t size,bool encrypt)
{assert(address==0x8000&&size==0xc00&&!encrypt);memcpy(disk.flash+address,in,size);return ESP_OK;}
esp_err_t bootloader_flash_erase_sector(size_t sector)
{assert(sector==8);memset(disk.flash+0x8000,255,4096);return ESP_OK;}
int esp_rom_get_reset_reason(int cpu) {assert(!cpu);return reset_reason;}
bool bootloader_utility_load_partition_table(bootloader_state_t *state) {(void)state;return true;}
int bootloader_utility_get_selected_boot_partition(const bootloader_state_t *state) {(void)state;return disk.selected;}
void bootloader_reset(void) {assert(false);abort();}
void bootloader_utility_load_boot_image(const bootloader_state_t *state,int index)
{
    (void)state;for(unsigned i=0;i<sizeof(rtc.custom);i++)assert(rtc.custom[i]==0);
    if(index<0){loaded=index;running=&factory;}
    else {loaded=(int)boot_slot;executing=apps[boot_slot];executing.subtype=16;running=&executing;}
    longjmp(boot_jump,1);
}
esp_err_t passport_qso_stage(unsigned slot,const passport_qso_data_t *data)
{(void)slot;(void)data;assert(false);return ESP_FAIL;} /* generic app lifecycle only */

static void runtime_reset(void)
{
    memset(handles,0,sizeof(handles));memset(initialized,0,sizeof(initialized));
    memset(storage_code,0,sizeof(storage_code));memset(storage_data,0,sizeof(storage_data));
    storage_blocked=false;memset(&registry,0,sizeof(registry));mutex=NULL;passport_install_enable(false);ota_active=false;
}
static int reboot(int reason)
{
    runtime_reset();reset_reason=reason;
    if(reason!=RESET_REASON_CORE_SW&&reason!=RESET_REASON_CPU0_SW)memset(&rtc,0,sizeof(rtc));
    passport_ticket_t t;memcpy(&t,rtc.custom,sizeof(t));boot_slot=t.slot;
    if(!setjmp(boot_jump))bootloader_after_init();
    return loaded;
}
static esp_err_t menu(void)
{assert(reboot(1)==FACTORY_INDEX);esp_err_t e=passport_manager_init();if(e==ESP_OK)passport_install_enable(true);return e;}
static void fresh(void)
{memset(&disk,0,sizeof(disk));memset(disk.flash,255,sizeof(disk.flash));disk.selected=-1;
 memset(disk.flash+factory.address,0xa6,factory.size);io_count=fail_at=0;fail_after_write=false;assert(menu()==ESP_OK);}
static esp_err_t install(unsigned f)
{stream=fixture[f].bytes;stream_size=fixture[f].size;stream_offset=0;httpd_req_t req={stream_size};return passport_install_http(&req);}
static void launch(unsigned slot)
{
    if(!setjmp(restart_jump)){esp_err_t e=passport_launch(slot);fprintf(stderr,"Launch returned %d\n",e);abort();}
    assert(reboot(RESET_REASON_CORE_SW)==(int)slot);
}
static void return_to_menu(void)
{
    if(!setjmp(restart_jump)){passport_return_to_launcher();assert(false);}
    assert(reboot(RESET_REASON_CORE_SW)==FACTORY_INDEX);assert(passport_manager_init()==ESP_OK);passport_install_enable(true);
}
static const char *app_id(unsigned f) {return (const char*)fixture[f].bytes+offsetof(passport_package_t,app_id);}
static void set_counter(unsigned f,int32_t value)
{nvs_handle_t h;assert(passport_app_open(app_id(f),"counter",NVS_READWRITE,&h)==ESP_OK);
 assert(nvs_set_i32(h,"value",value)==ESP_OK&&nvs_commit(h)==ESP_OK);nvs_close(h);}
static int32_t counter(unsigned f)
{nvs_handle_t h;int32_t value;assert(passport_app_open(app_id(f),"counter",NVS_READONLY,&h)==ESP_OK);
 assert(nvs_get_i32(h,"value",&value)==ESP_OK);nvs_close(h);return value;}
static int32_t persisted_counter(unsigned slot)
{char label[]="appdata0";label[7]+=slot;record_t *r=find_record(label,"counter","value");assert(r&&r->size==4);int32_t value;memcpy(&value,r->value,4);return value;}
static void factory_unchanged(void)
{for(size_t i=0;i<factory.size;i++)assert(disk.flash[factory.address+i]==0xa6);}
static void snapshot_baseline(void)
{
    fresh();assert(install(0)==ESP_OK&&install(2)==ESP_OK);
    launch(0);set_counter(0,101);assert(passport_app_mark_healthy(app_id(0))==ESP_OK);return_to_menu();
    launch(1);set_counter(2,202);assert(passport_app_mark_healthy(app_id(2))==ESP_OK);return_to_menu();baseline=disk;
}
static void restore(void) {disk=baseline;fail_at=0;assert(menu()==ESP_OK);io_count=0;}
static void reset_failure_checks(bool updating)
{
    restore();assert((updating?install(1):passport_delete(0,true))==ESP_OK);int boundaries=io_count;
    for(int after=0;after<2;after++)for(int fail=1;fail<=boundaries;fail++) {
        restore();fail_at=fail;fail_after_write=after;
        esp_err_t e=updating?install(1):passport_delete(0,true);assert(e!=ESP_OK);fail_at=0;
        assert(menu()==ESP_OK);passport_registry_t r=stored_registry();assert(passport_registry_valid(&r));
        assert(persisted_counter(1)==202);factory_unchanged();
        if(updating) {
            assert(persisted_counter(0)==101);
            if(r.slots[0].state==PASSPORT_READY) {
                uint8_t hash[32];raw_sha(disk.flash+apps[0].address,r.slots[0].package.image_size,hash);
                assert(!memcmp(hash,r.slots[0].package.sha256,32));
            } else assert(r.slots[0].state==PASSPORT_INSTALLING&&passport_launch(0)!=ESP_OK);
            assert(install(1)==ESP_OK);
        } else {
            assert(r.slots[0].state==PASSPORT_READY||r.slots[0].state==PASSPORT_RESERVED||r.slots[0].state==PASSPORT_EMPTY);
            if(r.slots[0].state==PASSPORT_EMPTY)assert(!find_record("appdata0","counter","value"));
            else assert(passport_delete(0,true)==ESP_OK);
            assert(install(4)==ESP_OK);
            assert(!find_record("appdata0","counter","value"));
        }
    }
    printf("Lifecycle %s: %d I/O boundaries before/after writes plus registry reload: PASS\n",updating?"update":"delete",boundaries);
}

static void launch_failure_checks(void)
{
    restore();io_count=0;launch(0);int boundaries=io_count;
    for(int after=0;after<2;after++)for(int fail=1;fail<=boundaries;fail++) {
        restore();fail_at=fail;fail_after_write=after;
        assert(passport_launch(0)!=ESP_OK);fail_at=0;
        /* A selector write that persisted despite an error is insufficient:
         * no launch ticket was issued, so a software reset stays in factory. */
        assert(reboot(RESET_REASON_CORE_SW)==FACTORY_INDEX);
        assert(passport_manager_init()==ESP_OK);passport_install_enable(true);
        assert(persisted_counter(0)==101&&persisted_counter(1)==202);
        factory_unchanged();launch(0);assert(counter(0)==101);
        assert(passport_app_mark_healthy(app_id(0))==ESP_OK);return_to_menu();
    }
    printf("Lifecycle launch: %d I/O boundaries before/after writes, no ticket on error: PASS\n",boundaries);
}

static void immediate_retry_checks(void)
{
    restore();assert(install(1)==ESP_OK);int boundaries=io_count;
    for(int after=0;after<2;after++)for(int fail=1;fail<=boundaries;fail++) {
        restore();fail_at=fail;fail_after_write=after;assert(install(1)!=ESP_OK);fail_at=0;
        /* Retry through the same manager instance without a reset first. */
        assert(install(1)==ESP_OK);launch(0);assert(counter(1)==101);return_to_menu();
        assert(persisted_counter(1)==202);factory_unchanged();
    }
    puts("Lifecycle same-session retries after every failed update boundary: PASS");
}

int main(int argc,char **argv)
{
    assert(argc==2);const char *names[]={"a1","a2","b1","c1","d1","x1","y1"};
    for(unsigned i=0;i<7;i++){char path[512];snprintf(path,sizeof(path),"%s/%s.passport",argv[1],names[i]);
        FILE *f=fopen(path,"rb");assert(f);fixture[i].size=fread(fixture[i].bytes,1,sizeof(fixture[i].bytes),f);
        assert(fixture[i].size>512&&fgetc(f)==EOF);fclose(f);}
    snapshot_baseline();assert(passport_autostart_toggle()==ESP_OK);launch(0);
    assert(counter(0)==101);nvs_handle_t h;assert(passport_app_open(app_id(2),"counter",NVS_READONLY,&h)!=ESP_OK);
    passport_registry_t r=stored_registry();assert(!passport_should_autostart(&r,true));
    assert(passport_app_mark_healthy(app_id(0))==ESP_OK);return_to_menu();
    r=stored_registry();assert(passport_should_autostart(&r,true)&&!passport_should_autostart(&r,false));
    assert(install(3)==ESP_OK); /* More app capacity is covered by the allocator model. */
    assert(passport_delete(0,false)==ESP_OK);assert(menu()==ESP_OK);
    assert(install(0)==ESP_OK);launch(0);assert(counter(0)==101);return_to_menu();
    assert(install(1)==ESP_OK);launch(0);assert(counter(1)==101);assert(passport_app_mark_healthy(app_id(1))==ESP_OK);return_to_menu();
    assert(passport_delete(0,true)==ESP_OK&&install(4)==ESP_OK);launch(0);
    assert(passport_app_open(app_id(4),"counter",NVS_READONLY,&h)==ESP_ERR_NVS_NOT_FOUND);
    set_counter(4,303);return_to_menu();launch(1);assert(counter(2)==202);return_to_menu();
    factory_unchanged();reset_failure_checks(true);reset_failure_checks(false);
    launch_failure_checks();immediate_retry_checks();
    restore();disk.flash[apps[0].address+7000]^=1;assert(passport_launch(0)==ESP_ERR_OTA_VALIDATE_FAILED);
    r=stored_registry();assert(r.last_slot==1&&persisted_counter(0)==101&&persisted_counter(1)==202);
    restore();record_t *meta=find_record("pos_meta","passport","registry_v2");assert(meta);meta->used=false;
    assert(menu()==ESP_ERR_INVALID_STATE&&persisted_counter(0)==101&&persisted_counter(1)==202);
    puts("Production manager/SDK/boot lifecycle: private values, return, capacity, reinstall, corruption and missing registry: PASS");
    fresh();assert(install(5)==ESP_OK&&install(6)==ESP_OK&&install(0)==ESP_OK);
    launch(0);for(size_t i=0;i<NVS_BYTES;i++)assert(disk.flash[0x9000+i]==255);
    memset(disk.flash+0x9000,0x31,NVS_BYTES);return_to_menu();
    launch(1);for(size_t i=0;i<NVS_BYTES;i++)assert(disk.flash[0x9000+i]==255);
    memset(disk.flash+0x9000,0x72,NVS_BYTES);return_to_menu();
    launch(2);set_counter(0,444);return_to_menu();
    launch(0);for(size_t i=0;i<NVS_BYTES;i++)assert(disk.flash[0x9000+i]==0x31);
    memset(disk.flash+0x9000,0x42,NVS_BYTES);assert(menu()==ESP_OK); /* power-cycle return */
    launch(1);for(size_t i=0;i<NVS_BYTES;i++)assert(disk.flash[0x9000+i]==0x72);return_to_menu();
    assert(passport_delete(0,false)==ESP_OK&&install(5)==ESP_OK);launch(0);
    for(size_t i=0;i<NVS_BYTES;i++)assert(disk.flash[0x9000+i]==0x42);
    return_to_menu();
    launch(2);assert(counter(0)==444);return_to_menu();factory_unchanged();
    /* Changing storage mode under a retained ID must fail before erasing. */
    passport_package_t *transport=(passport_package_t *)fixture[5].bytes;
    transport->abi=PASSPORT_ABI;assert(install(5)==ESP_ERR_INVALID_VERSION);transport->abi=0;
    launch(0);for(size_t i=0;i<NVS_BYTES;i++)assert(disk.flash[0x9000+i]==0x42);return_to_menu();
    puts("Community production manager + actual NVS context + boot hook: two default stores, SDK coexistence, power-cycle, reinstall and mode mismatch: PASS");
    assert(passport_autostart_toggle()==ESP_OK); /* On: normal community BINs can resume on cold power-on. */
    assert(passport_autostart_ready(true)&&!passport_autostart_ready(false));
    assert(passport_autostart_toggle()==ESP_OK&&!passport_autostart_ready(true));
    assert(passport_autostart_toggle()==ESP_OK);
    launch(2);return_to_menu();assert(!passport_autostart_ready(true)); /* Native health still required. */
    launch(2);assert(passport_app_mark_healthy(app_id(0))==ESP_OK);return_to_menu();
    assert(passport_autostart_ready(true)&&!passport_autostart_ready(false));
    launch(0);return_to_menu();assert(passport_autostart_ready(true));
    puts("Auto start: explicit On/Off, community cold boot, warm reset exclusion and native SDK health gate: PASS");
    record_t *mode=find_record("pos_meta","compat","mode0");assert(mode);mode->used=false;
    assert(passport_launch(0)==ESP_ERR_INVALID_VERSION); /* Never run a legacy app as native if its mode is lost. */
    /* Default follows last boot; a pinned stable ID survives other app boots. */
    fresh();assert(install(5)==ESP_OK&&install(6)==ESP_OK);
    char choice[64];assert(passport_autostart_configuration(choice)&&!choice[0]);
    assert(passport_autostart_boot_slot()==PASSPORT_NO_SLOT);
    launch(0);return_to_menu();assert(passport_autostart_boot_slot()==0);
    assert(passport_autostart_toggle()==ESP_OK);
    assert(passport_autostart_select("community.y1")==ESP_OK);
    assert(passport_autostart_boot_slot()==1&&passport_autostart_ready(true)&&!passport_autostart_ready(false));
    launch(0);return_to_menu();assert(stored_registry().last_slot==0&&passport_autostart_boot_slot()==1);
    assert(passport_autostart_configuration(choice)&&!strcmp(choice,"community.y1"));
    assert(passport_autostart_select("community.missing")==ESP_ERR_INVALID_ARG&&passport_autostart_boot_slot()==1);
    assert(passport_autostart_toggle()==ESP_OK&&!passport_autostart_ready(true));
    assert(passport_autostart_toggle()==ESP_OK);
    /* Each NVS I/O can fail before or after persistence. Reboot resolves to a
     * complete old/new preference; both point to the same untouched app data. */
    io_count=0;assert(passport_autostart_select("community.x1")==ESP_OK);int preference_io=io_count;
    for(int when=0;when<2;when++)for(int f=1;f<=preference_io;f++) {
        fail_at=0;assert(menu()==ESP_OK);assert(passport_autostart_select("community.y1")==ESP_OK);
        io_count=0;fail_at=f;fail_after_write=when;(void)passport_autostart_select("community.x1");
        fail_at=0;assert(menu()==ESP_OK);unsigned selected=passport_autostart_boot_slot();assert(selected==0||selected==1);
    }
    assert(passport_autostart_select("community.y1")==ESP_OK);
    assert(passport_uninstall(1,"community.y1")==ESP_OK);
    assert(passport_autostart_configuration(choice)&&!choice[0]&&passport_autostart_boot_slot()==0);
    assert(passport_autostart_select("")==ESP_OK);
    puts("Auto start target: default-last, fixed stable ID, other manual boots, persistence, On/Off, NVS faults and uninstall fallback: PASS");
    char real_path[512];snprintf(real_path,sizeof(real_path),"%s/real-qso.transport",argv[1]);
    FILE *qso=fopen(real_path,"rb");
    if(qso) {
        fixture[7].size=fread(fixture[7].bytes,1,sizeof(fixture[7].bytes),qso);assert(fgetc(qso)==EOF);fclose(qso);
        snprintf(real_path,sizeof(real_path),"%s/real-radio.transport",argv[1]);
        FILE *radio=fopen(real_path,"rb");assert(radio);
        fixture[8].size=fread(fixture[8].bytes,1,sizeof(fixture[8].bytes),radio);assert(fgetc(radio)==EOF);fclose(radio);
        fresh();assert(install(7)==ESP_OK&&install(8)==ESP_OK);
        launch(0);memset(disk.flash+0x9000,0x91,NVS_BYTES);return_to_menu();
        launch(1);for(size_t i=0;i<NVS_BYTES;i++)assert(disk.flash[0x9000+i]==255);
        memset(disk.flash+0x9000,0x63,NVS_BYTES);return_to_menu();
        launch(0);for(size_t i=0;i<NVS_BYTES;i++)assert(disk.flash[0x9000+i]==0x91);
        return_to_menu();factory_unchanged();
        assert(passport_uninstall(0,"community.wrong")==ESP_ERR_INVALID_STATE);
        assert(passport_uninstall(0,"community.qso-vision")==ESP_OK);
        assert(!find_record("appdata0","ppl_legacy","nvs_v1"));
        assert(stored_registry().slots[0].state==PASSPORT_EMPTY);
        assert(install(7)==ESP_OK);launch(0);
        for(size_t i=0;i<NVS_BYTES;i++)assert(disk.flash[0x9000+i]==255);
        return_to_menu();
        puts("Owner QSO + Internet Radio real image bytes: production upload/hash/selection/context switching PASS in host model; ESP32 code NOT executed");
    }
    fresh();assert(install(0)==ESP_OK&&install(2)==ESP_OK&&install(3)==ESP_OK&&install(4)==ESP_OK);
    launch(3);set_counter(4,987);return_to_menu();launch(3);assert(counter(4)==987);return_to_menu();
    puts("Production shared manager + SDK: fourth app installs, boots and retains private data: PASS");
    snprintf(real_path,sizeof(real_path),"%s/large.transport",argv[1]);FILE *large=fopen(real_path,"rb");assert(large);
    fixture[9].size=fread(fixture[9].bytes,1,sizeof(fixture[9].bytes),large);assert(fgetc(large)==EOF);fclose(large);
    fresh();assert(install(9)==ESP_OK);assert(stored_registry().slots[0].package.image_size>0x200000);
    launch(0);memset(disk.flash+0x9000,0x57,NVS_BYTES);return_to_menu();
    assert(install(0)==ESP_OK);launch(1);set_counter(0,567);return_to_menu();
    launch(0);for(size_t i=0;i<NVS_BYTES;i++)assert(disk.flash[0x9000+i]==0x57);return_to_menu();
    assert(passport_uninstall(0,"community.large")==ESP_OK);launch(1);assert(counter(0)==567);return_to_menu();
    puts("Production 2.5 MiB BIN: streamed install, full SHA, boot selection, context retention and uninstall alongside SDK app: PASS (host model)");
    return 0;
}

esp_err_t esp_flash_write(void *chip,const void *data,uint32_t address,uint32_t size)
{assert(!chip&&address>=PASSPORT_POOL_START&&address+size<=PASSPORT_POOL_END);bool f=fault();if(f&&!fail_after_write)return ESP_FAIL;
 const uint8_t *p=data;for(uint32_t i=0;i<size;i++){assert((disk.flash[address+i]&p[i])==p[i]);disk.flash[address+i]=p[i];}return f?ESP_FAIL:ESP_OK;}
esp_err_t esp_flash_erase_region(void *chip,uint32_t address,uint32_t size)
{assert(!chip&&!(address&4095)&&!(size&4095)&&address>=PASSPORT_POOL_START&&address+size<=PASSPORT_POOL_END);bool f=fault();if(f&&!fail_after_write)return ESP_FAIL;
 memset(disk.flash+address,255,size);return f?ESP_FAIL:ESP_OK;}
