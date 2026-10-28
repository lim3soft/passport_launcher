/* Executes the production installer/deleter with faults at every I/O boundary.
 * Mock image/SHA validation is intentional; real binary validation is separately
 * exercised in test_package.py and the ESP-IDF build. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../main/manager.c"
#include "../main/storage.c"

static passport_registry_t durable;
static char stored_startup[64];
static bool unlocked;
static int fail_at, operation, destructive, ota_slot;
static size_t received, written;
static int64_t clock_us, clock_step;
static unsigned char upload[1056], code[PASSPORT_SLOTS][0x10000], private[PASSPORT_SLOTS][0x20000];
static unsigned staged;
static rtc_retain_mem_t rtc;
static esp_partition_t apps[PASSPORT_SLOTS],data[PASSPORT_SLOTS];
static bool app_active[PASSPORT_SLOTS],data_active[PASSPORT_SLOTS];
static unsigned label_slot(const char *label){return (unsigned)strtoul(label+7,NULL,16);}
esp_err_t esp_partition_register_external(void *chip,size_t address,size_t size,const char *label,unsigned type,unsigned subtype,const esp_partition_t **out)
{
    assert(!chip);unsigned slot=type==0?subtype-16:label_slot(label);assert(slot<PASSPORT_SLOTS);
    esp_partition_t *p=type==0?&apps[slot]:&data[slot];*p=(esp_partition_t){type,subtype,address,size};
    if(type==0)app_active[slot]=true;else data_active[slot]=true;*out=p;return ESP_OK;
}
esp_err_t esp_partition_deregister_external(const esp_partition_t *p)
{for(unsigned i=0;i<PASSPORT_SLOTS;i++){if(p==&apps[i])app_active[i]=false;if(p==&data[i])data_active[i]=false;}return ESP_OK;}
esp_err_t nvs_flash_deinit_partition(const char *label){(void)label;return ESP_OK;}
esp_err_t esp_flash_read(void *chip,void *out,uint32_t address,uint32_t size)
{(void)chip;(void)address;memset(out,255,size);return ESP_OK;}
static bool fault(void) {return ++operation == fail_at;}
SemaphoreHandle_t xSemaphoreCreateMutex(void) {unlocked=true;return &unlocked;}
int xSemaphoreTake(SemaphoreHandle_t m, int wait) {(void)wait; if(!*m)return 0;*m=false;return 1;}
void xSemaphoreGive(SemaphoreHandle_t m) {*m=true;}
esp_err_t passport_registry_load(passport_registry_t *out) {*out=durable;return ESP_OK;}
esp_err_t passport_registry_save(const passport_registry_t *r)
{if(fault())return ESP_FAIL; assert(passport_registry_valid(r));durable=*r;return ESP_OK;}
const esp_partition_t *esp_partition_find_first(unsigned type, unsigned subtype, const char *label)
{
    if(type==0 && subtype>=16 && subtype<16+PASSPORT_SLOTS && app_active[subtype-16])return &apps[subtype-16];
    if(type==1 && label && !strncmp(label,"appdata",7) && label_slot(label)<PASSPORT_SLOTS && data_active[label_slot(label)])return &data[label_slot(label)];
    return NULL;
}
esp_err_t esp_partition_read(const esp_partition_t *p, size_t offset, void *out, size_t n)
{
    if(fault())return ESP_FAIL;
    assert(offset+n<=p->size);
    unsigned slot=p->type==0 ? (unsigned)(p->subtype-16) : (unsigned)(p-data);
    memcpy(out, p->type==0 ? code[slot]+offset : private[slot]+offset,n);return ESP_OK;
}
esp_err_t esp_partition_erase_range(const esp_partition_t *p, size_t offset, size_t n)
{
    unsigned slot=p->subtype-16;
    assert(durable.slots[slot].state==PASSPORT_RESERVED || durable.slots[slot].state==PASSPORT_INSTALLING);
    if(fault())return ESP_FAIL;
    destructive++;memset(code[slot]+offset,255,n);return ESP_OK;
}
esp_err_t nvs_flash_erase_partition(const char *label)
{
    unsigned slot=label_slot(label);assert(slot<PASSPORT_SLOTS);
    assert(durable.slots[slot].state==PASSPORT_RESERVED || durable.slots[slot].state==PASSPORT_INSTALLING);
    if(fault())return ESP_FAIL;
    destructive++;memset(private[slot],255,sizeof(private[slot]));return ESP_OK;
}
int httpd_req_recv(httpd_req_t *req, char *out, size_t n)
{
    clock_us += clock_step;
    if(fault())return -1;
    assert(received+n<=req->content_len);
    /* Small partial network reads exercise receive_exact(). */
    if(n>71)n=71;
    memcpy(out,upload+received,n);received+=n;return (int)n;
}
esp_err_t esp_ota_begin(const esp_partition_t *p, size_t size, esp_ota_handle_t *handle)
{
    (void)size;ota_slot=p->subtype-16;
    assert(durable.slots[ota_slot].state==PASSPORT_INSTALLING);
    if(fault())return ESP_FAIL;
    *handle=1;destructive++;memset(code[ota_slot],255,sizeof(code[ota_slot]));return ESP_OK;
}
esp_err_t esp_ota_write(esp_ota_handle_t h,const void *buf,size_t n)
{
    assert(h==1 && durable.slots[ota_slot].state==PASSPORT_INSTALLING);
    if(fault())return ESP_FAIL;
    memcpy(code[ota_slot]+written,buf,n);written+=n;return ESP_OK;
}
esp_err_t esp_ota_end(esp_ota_handle_t h) {assert(h==1);return fault()?ESP_FAIL:ESP_OK;}
esp_err_t esp_ota_abort(esp_ota_handle_t h) {(void)h;return ESP_OK;}
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p) {(void)p;return ESP_OK;}
rtc_retain_mem_t *bootloader_common_get_rtc_retain_mem(void) {return &rtc;}
void esp_restart(void) {abort();}
int64_t esp_timer_get_time(void) {return clock_us;}
void mbedtls_sha256_init(mbedtls_sha256_context *c) {(void)c;}
void mbedtls_sha256_free(mbedtls_sha256_context *c) {(void)c;}
int mbedtls_sha256_starts(mbedtls_sha256_context *c,int type) {(void)c;(void)type;return 0;}
int mbedtls_sha256_update(mbedtls_sha256_context *c,const unsigned char *buf,size_t n) {(void)c;(void)buf;(void)n;return 0;}
int mbedtls_sha256_finish(mbedtls_sha256_context *c,unsigned char out[32]) {(void)c;memset(out,0,32);return 0;}
esp_err_t passport_qso_stage(unsigned slot, const passport_qso_data_t *p)
{assert(slot==0 && passport_qso_data_valid(p));staged++;return ESP_OK;}

static void reset(int fail)
{
    memset(code,255,sizeof(code));memset(private,0x5a,sizeof(private));
    memset(code[1],0x77,sizeof(code[1]));
    passport_registry_init(&durable);
    passport_package_t p={.format=1,.abi=PASSPORT_ABI,.image_size=512};
    memcpy(p.magic,PASSPORT_MAGIC,8);strcpy(p.app_id,"org.test.app");strcpy(p.name,"Test");strcpy(p.version,"1");
    memset(upload,0,sizeof(upload));memcpy(upload,&p,sizeof(p));
    esp_image_header_t image={.magic=0xe9,.segment_count=1,.spi_mode=2,.spi_size=3,.spi_speed=15,.chip_id=5,.hash_appended=1};
    memcpy(upload+512,&image,sizeof(image));
    uint32_t magic=ESP_APP_DESC_MAGIC_WORD;memcpy(upload+512+32,&magic,4);
    memcpy(upload+512+144,"v5.5.3",6);
    memcpy(upload+512+288,"PASSPORTOS_ABI2:org.test.app",29);
    fail_at=fail;operation=destructive=0;written=received=0;
    clock_us=0;clock_step=0;passport_install_enable(true);
    registry=durable;mutex=xSemaphoreCreateMutex();storage_blocked=false;
    memset(startup_id,0,sizeof(startup_id));memset(stored_startup,0,sizeof(stored_startup));
    memset(storage_code,0,sizeof(storage_code));memset(storage_data,0,sizeof(storage_data));
    memset(app_active,0,sizeof(app_active));memset(data_active,0,sizeof(data_active));
}

int main(void)
{
    reset(0);httpd_req_t req={1024};
    httpd_req_t check={512};char message[96];
    assert(passport_preflight_http(&check)==ESP_OK&&destructive==0&&durable.slots[0].state==PASSPORT_EMPTY);
    assert(passport_import_message(message,sizeof(message))&&strstr(message,"Space checked"));
    reset(0);((passport_package_t *)upload)->image_size=PASSPORT_SLOT_SIZE+1;
    assert(passport_preflight_http(&check)==ESP_ERR_INVALID_SIZE&&destructive==0);
    assert(passport_import_message(message,sizeof(message))&&strstr(message,"Not enough usable flash"));
    reset(0);
    passport_package_t full=*(passport_package_t *)upload;full.image_size=PASSPORT_SLOT_SIZE;
    assert(passport_install_prepare(&durable,&full,0)&&passport_install_finish(&durable,0));
    registry=durable;
    strcpy(((passport_package_t *)upload)->app_id,"community.other");
    assert(passport_preflight_http(&check)==ESP_ERR_NO_MEM&&destructive==0);
    assert(passport_import_message(message,sizeof(message))&&strstr(message,"Uninstall an app"));
    received=0;memset(((passport_package_t *)upload)->app_id,0,64);strcpy(((passport_package_t *)upload)->app_id,"org.test.app");
    assert(passport_preflight_http(&check)==ESP_OK&&destructive==0);
    reset(0);passport_import_note(IMPORT_LAYOUT,0);
    assert(passport_import_message(message,sizeof(message))&&strstr(message,"Unsupported ROM layout"));
    reset(0);
    assert(passport_install_http(&req)==ESP_OK);
    int install_ops=operation;
    assert(durable.slots[0].state==PASSPORT_READY);
    for(int fail=1;fail<=install_ops;fail++) {
        reset(fail);
        assert(passport_install_http(&req)!=ESP_OK);
        assert(durable.slots[0].state!=PASSPORT_READY);
        if(destructive)assert(durable.slots[0].state==PASSPORT_INSTALLING);
        for(unsigned s=1;s<3;s++)for(size_t j=0;j<sizeof(private[s]);j++)assert(private[s][j]==0x5a);
        for(size_t j=0;j<sizeof(code[1]);j++)assert(code[1][j]==0x77);
        assert(unlocked);
    }
    reset(0);upload[512+12]=9;
    assert(passport_install_http(&req)!=ESP_OK && destructive==0);
    reset(0);passport_install_enable(false);
    assert(passport_install_http(&req)!=ESP_OK && destructive==0);
    reset(0);clock_step=60000000;
    assert(passport_install_http(&req)!=ESP_OK && destructive==0);
    reset(0);memset(upload+512+288,0,29);
    assert(passport_install_http(&req)!=ESP_OK && durable.slots[0].state==PASSPORT_INSTALLING);
    reset(0);upload[148]=1;
    assert(passport_install_http(&req)!=ESP_OK && durable.slots[0].state==PASSPORT_INSTALLING);
    for(int fail=0;fail<=4;fail++) {
        reset(0);assert(passport_install_http(&req)==ESP_OK);
        fail_at=fail;operation=0;
        esp_err_t result=passport_delete(0,true);
        if(fail)assert(result!=ESP_OK);else assert(result==ESP_OK);
        if(durable.slots[0].state==PASSPORT_EMPTY) {
            for(size_t j=0;j<sizeof(private[0]);j++)assert(private[0][j]==255);
            for(size_t j=0;j<sizeof(code[0]);j++)assert(code[0][j]==255);
        } else assert(durable.slots[0].state==PASSPORT_RESERVED || !destructive || fail==1);
    }
    reset(0);assert(passport_install_http(&req)==ESP_OK);
    int writes_before=destructive;
    assert(passport_uninstall(0,"org.someone.else")==ESP_ERR_INVALID_STATE&&destructive==writes_before);
    assert(passport_uninstall(0,"org.test.app")==ESP_OK&&durable.slots[0].state==PASSPORT_EMPTY);
    for(size_t j=0;j<sizeof(private[0]);j++)assert(private[0][j]==255);
    reset(0);assert(passport_install_http(&req)==ESP_OK);
    assert(passport_delete(0,false)==ESP_OK && durable.slots[0].state==PASSPORT_RESERVED);
    for(size_t j=0;j<sizeof(private[0]);j++)assert(private[0][j]==255);
    /* Restore route must require the right app, marker, file size and session. */
    reset(0);assert(passport_install_http(&req)==ESP_OK);
    httpd_req_t restore={sizeof(passport_qso_data_t)};
    assert(passport_qso_import_http(&restore,0)!=ESP_OK && staged==0);
    strcpy(registry.slots[0].package.app_id,PASSPORT_QSO_ID);
    assert(passport_qso_import_http(&restore,0)!=ESP_OK && staged==0);
    memcpy(code[0]+400,PASSPORT_QSO_MIGRATION_MARKER,sizeof(PASSPORT_QSO_MIGRATION_MARKER));
    passport_qso_data_t migration={0};memcpy(migration.data.magic,"PPTQSO1",8);
    migration.data.format=1;migration.data.present=1;migration.data.numbers[0]=91;
    memcpy(upload,&migration,sizeof(migration));received=0;
    assert(passport_qso_import_http(&restore,0)==ESP_OK && staged==1);
    passport_install_enable(false);
    assert(passport_qso_import_http(&restore,0)!=ESP_OK && staged==1);
    printf("Production installer: %d I/O failure boundaries; wrong chip, hash, ABI, delete ownership: PASS\n",install_ops);
}

/* Compatibility storage has its own production fault-injection test. */
esp_err_t passport_compat_recover(const passport_registry_t *r) {(void)r;return ESP_OK;}
esp_err_t passport_compat_begin(const passport_registry_t *r,unsigned slot) {(void)r;(void)slot;return ESP_OK;}
esp_err_t passport_compat_mode(const passport_registry_t *r,unsigned slot,bool *legacy) {(void)r;(void)slot;*legacy=false;return ESP_OK;}
esp_err_t passport_compat_set_mode(unsigned slot,const char *id,bool legacy) {(void)slot;(void)id;assert(!legacy);return ESP_OK;}

esp_err_t passport_autostart_load(char id[64]){memcpy(id,stored_startup,64);return ESP_OK;}
esp_err_t passport_autostart_save(const char *id){memset(stored_startup,0,64);strcpy(stored_startup,id);return ESP_OK;}

esp_err_t esp_flash_write(void *chip,const void *data,uint32_t address,uint32_t size)
{(void)chip;(void)data;(void)address;(void)size;assert(false);return ESP_FAIL;}
esp_err_t esp_flash_erase_region(void *chip,uint32_t address,uint32_t size)
{(void)chip;(void)address;(void)size;assert(false);return ESP_FAIL;}
