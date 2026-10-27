/* Production context journal with persistent host storage, real SHA, and
 * failures before/after writes (including partial raw erase/write effects).
 * Does not emulate NVS pages or execute ESP32 application machine code. */
#include <assert.h>
#include <stdio.h>
#include <openssl/evp.h>
#include "../main/compat.c"

typedef struct {
    bool context_present, saved_present[3], mode_present[3];
    context_t context;
    passport_mode_t mode[3];
    saved_t saved[3];
    uint8_t raw[NVS_BYTES];
} disk_t;
static disk_t disk, baseline;
static passport_registry_t apps;
static esp_partition_t nvs={1,2,0x9000,NVS_BYTES};
static esp_partition_t private[]={
    {1,2,0x7a0000,0x20000},{1,2,0x7c0000,0x20000},{1,2,0x7e0000,0x20000}};
static int io, fail_at;
static bool after;
static bool fault(void){return ++io==fail_at;}
const esp_partition_t *esp_partition_find_first(unsigned type,unsigned subtype,const char *label)
{
    assert(type==1&&subtype==2);
    if(!strcmp(label,"nvs"))return &nvs;
    assert(strlen(label)==8&&label[7]>='0'&&label[7]<='2');return &private[label[7]-'0'];
}
esp_err_t nvs_flash_init_partition(const char *label){(void)label;return fault()?ESP_FAIL:ESP_OK;}
esp_err_t nvs_flash_deinit_partition(const char *label){assert(!strcmp(label,"nvs"));return fault()?ESP_FAIL:ESP_OK;}
esp_err_t nvs_open_from_partition(const char *part,const char *ns,int mode,nvs_handle_t *h)
{
    (void)mode;if(fault())return ESP_FAIL;
    if(!strcmp(part,"pos_meta")){assert(!strcmp(ns,"compat"));*h=1;}
    else{assert(!strcmp(ns,"ppl_legacy"));*h=10+part[7]-'0';assert(*h>=10&&*h<=12);}
    return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size)
{
    if(fault())return ESP_FAIL;
    const void *value;size_t n;
    if(h==1 && !strncmp(key,"mode",4)) {
        unsigned slot=key[4]-'0';assert(slot<3);if(!disk.mode_present[slot])return ESP_ERR_NVS_NOT_FOUND;
        value=&disk.mode[slot];n=sizeof(passport_mode_t);
    }
    else if(h==1){assert(!strcmp(key,"context_v1"));if(!disk.context_present)return ESP_ERR_NVS_NOT_FOUND;value=&disk.context;n=sizeof(disk.context);}
    else{unsigned slot=h-10;assert(slot<3&&!strcmp(key,"nvs_v1"));if(!disk.saved_present[slot])return ESP_ERR_NVS_NOT_FOUND;value=&disk.saved[slot];n=sizeof(saved_t);}
    assert(*size>=n);*size=n;memcpy(out,value,n);return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *value,size_t size)
{
    bool f=fault();if(f&&!after)return ESP_FAIL;
    if(h==1 && !strncmp(key,"mode",4)) {
        unsigned slot=key[4]-'0';assert(slot<3&&size==sizeof(passport_mode_t));disk.mode_present[slot]=true;memcpy(&disk.mode[slot],value,size);
    }
    else if(h==1){assert(size==sizeof(context_t)&&!strcmp(key,"context_v1"));disk.context_present=true;memcpy(&disk.context,value,size);}
    else{unsigned slot=h-10;assert(slot<3&&size==sizeof(saved_t)&&!strcmp(key,"nvs_v1"));disk.saved_present[slot]=true;memcpy(&disk.saved[slot],value,size);}
    return f?ESP_FAIL:ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;return fault()?ESP_FAIL:ESP_OK;}
void nvs_close(nvs_handle_t h){(void)h;}
esp_err_t esp_partition_read(const esp_partition_t *p,size_t offset,void *out,size_t size)
{assert(p==&nvs&&offset+size<=NVS_BYTES);if(fault())return ESP_FAIL;memcpy(out,disk.raw+offset,size);return ESP_OK;}
esp_err_t esp_partition_write(const esp_partition_t *p,size_t offset,const void *data,size_t size)
{
    assert(p==&nvs&&offset==0&&size==NVS_BYTES&&disk.context.phase==RESTORING);
    bool f=fault();if(f&&!after)return ESP_FAIL;
    memcpy(disk.raw,data,f?size/2:size);return f?ESP_FAIL:ESP_OK;
}
esp_err_t esp_partition_erase_range(const esp_partition_t *p,size_t offset,size_t size)
{
    assert(p==&nvs&&offset==0&&size==NVS_BYTES&&(disk.context.phase==RESTORING||disk.context.phase==SAVED));
    bool f=fault();if(f&&!after)return ESP_FAIL;
    memset(disk.raw,255,f?size/2:size);return f?ESP_FAIL:ESP_OK;
}
int mbedtls_sha256(const unsigned char *data,size_t size,unsigned char out[32],int is224)
{assert(!is224);unsigned n;return EVP_Digest(data,size,out,&n,EVP_sha256(),NULL)==1&&n==32?0:-1;}

static void filled(const uint8_t *p,uint8_t value,size_t n)
{for(size_t i=0;i<n;i++)assert(p[i]==value);}
static void reset_fault(void){io=fail_at=0;after=false;}
static void prepare_baseline(void)
{
    memset(&disk,0,sizeof(disk));memset(disk.raw,255,NVS_BYTES);reset_fault();
    passport_registry_init(&apps);
    for(unsigned i=0;i<2;i++) {
        passport_package_t p={.format=1,.abi=0,.image_size=512};memcpy(p.magic,PASSPORT_MAGIC,8);
        snprintf(p.app_id,sizeof(p.app_id),"community.app%u",i);strcpy(p.name,"Community");strcpy(p.version,"1");
        assert(passport_install_prepare(&apps,&p,i)&&passport_install_finish(&apps,i));
        private[i].address=apps.slots[i].data_offset;
        assert(apps.slots[i].package.abi==PASSPORT_ABI); /* Older SDK registry contract. */
        assert(passport_compat_set_mode(i,p.app_id,true)==ESP_OK);
        assert(passport_compat_begin(&apps,i)==ESP_OK);filled(disk.raw,255,NVS_BYTES);
        memset(disk.raw,i?0xb2:0xa1,NVS_BYTES);assert(passport_compat_recover(&apps)==ESP_OK);
    }
    filled(disk.saved[0].data,0xa1,NVS_BYTES);filled(disk.saved[1].data,0xb2,NVS_BYTES);
    baseline=disk;
}
static void restore_failure_checks(void)
{
    disk=baseline;reset_fault();assert(passport_compat_begin(&apps,0)==ESP_OK);int count=io;
    for(int when=0;when<2;when++)for(int fail=1;fail<=count;fail++) {
        disk=baseline;reset_fault();fail_at=fail;after=when;
        assert(passport_compat_begin(&apps,0)!=ESP_OK);reset_fault();
        assert(passport_compat_recover(&apps)==ESP_OK);
        filled(disk.saved[0].data,0xa1,NVS_BYTES);filled(disk.saved[1].data,0xb2,NVS_BYTES);
        assert(passport_compat_begin(&apps,0)==ESP_OK);filled(disk.raw,0xa1,NVS_BYTES);
        assert(passport_compat_recover(&apps)==ESP_OK);
    }
    printf("Community NVS restore: %d I/O boundaries, before/after faults and partial raw writes: PASS\n",count);
}
static void save_failure_checks(void)
{
    disk=baseline;reset_fault();assert(passport_compat_begin(&apps,0)==ESP_OK);
    memset(disk.raw,0xc3,NVS_BYTES);disk_t active=disk;
    reset_fault();assert(passport_compat_recover(&apps)==ESP_OK);int count=io;
    for(int when=0;when<2;when++)for(int fail=1;fail<=count;fail++) {
        disk=active;reset_fault();fail_at=fail;after=when;
        assert(passport_compat_recover(&apps)!=ESP_OK);reset_fault();
        assert(passport_compat_recover(&apps)==ESP_OK);
        filled(disk.raw,255,NVS_BYTES);filled(disk.saved[0].data,0xc3,NVS_BYTES);filled(disk.saved[1].data,0xb2,NVS_BYTES);
        assert(passport_compat_begin(&apps,0)==ESP_OK);filled(disk.raw,0xc3,NVS_BYTES);
    }
    printf("Community NVS capture: %d I/O boundaries, before/after faults and partial raw erases: PASS\n",count);
}
int main(void)
{
    prepare_baseline();restore_failure_checks();save_failure_checks();
    disk=baseline;reset_fault();disk.saved[0].data[0]^=1;
    assert(passport_compat_begin(&apps,0)==ESP_ERR_INVALID_CRC);filled(disk.raw,255,NVS_BYTES);
    disk=baseline;strcpy(disk.saved[0].id,"community.other");
    assert(passport_compat_begin(&apps,0)==ESP_ERR_INVALID_CRC);
    disk=baseline;reset_fault();assert(passport_compat_begin(&apps,0)==ESP_OK);
    disk.mode_present[0]=false;
    assert(passport_compat_recover(&apps)==ESP_ERR_INVALID_STATE);filled(disk.raw,0xa1,NVS_BYTES);
    disk=baseline;reset_fault();assert(passport_compat_begin(&apps,0)==ESP_OK);
    strcpy(disk.context.id,"community.other");
    assert(passport_compat_recover(&apps)==ESP_ERR_INVALID_STATE);filled(disk.raw,0xa1,NVS_BYTES);
    disk=baseline;nvs.address=0xa000;assert(passport_compat_begin(&apps,0)==ESP_ERR_INVALID_STATE);nvs.address=0x9000;
    disk=baseline;assert(passport_compat_begin(&apps,2)==ESP_ERR_INVALID_ARG);
    puts("Community NVS identity, integrity, bounds and two independent default-NVS contexts: PASS");
}
