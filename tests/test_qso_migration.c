/* Runs production staging/application code. NVS and SHA are fault stubs; the
 * Python tests independently check real SHA and SDK-generated NVS fixtures. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../components/passport_sdk/passport_qso_migration.c"

static passport_registry_t registry;
static passport_qso_data_t stored;
static bool has_stage, has_done, stage_ns, settings_ns;
static uint8_t done[32];
static uint32_t saved_mask;
static int32_t saved_numbers[4];
static char saved_strings[19][65];
static int operation, fail_at;
static bool after_write;
static unsigned setting_writes;
static const esp_partition_t part={1,2,0x7a0000,0x20000};

static bool fault(void) {return ++operation==fail_at;}
static int field(const char *key)
{for(unsigned i=0;i<PASSPORT_QSO_FIELDS;i++)if(!strcmp(key,passport_qso_field_key(i)))return (int)i;assert(false);return -1;}
esp_err_t passport_registry_load(passport_registry_t *r) {if(fault())return ESP_FAIL;*r=registry;return ESP_OK;}
const esp_partition_t *esp_partition_find_first(unsigned type,unsigned subtype,const char *label)
{assert(type==1 && subtype==2 && !strcmp(label,"appdata0"));return &part;}
esp_err_t nvs_flash_init_partition(const char *label) {assert(!strcmp(label,"appdata0"));return fault()?ESP_FAIL:ESP_OK;}
esp_err_t nvs_open_from_partition(const char *label,const char *ns,int mode,nvs_handle_t *h)
{
    assert(!strcmp(label,"appdata0"));if(fault())return ESP_FAIL;
    bool *exists=!strcmp(ns,"pos_migrate")?&stage_ns:&settings_ns;
    assert(!strcmp(ns,"pos_migrate")||!strcmp(ns,"monitor"));
    if(!*exists && mode==NVS_READONLY)return ESP_ERR_NVS_NOT_FOUND;
    *exists=true;*h=exists==&stage_ns?1:2;return ESP_OK;
}
esp_err_t passport_app_open(const char *id,const char *ns,nvs_open_mode_t mode,nvs_handle_t *h)
{assert(!strcmp(id,PASSPORT_QSO_ID));return nvs_open_from_partition("appdata0",ns,mode,h);}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size)
{
    assert(h==1);if(fault())return ESP_FAIL;
    bool file=!strcmp(key,"qso_v1");assert(file||!strcmp(key,"applied_v1"));
    if(file?!has_stage:!has_done)return ESP_ERR_NVS_NOT_FOUND;
    size_t n=file?sizeof(stored):sizeof(done);assert(*size>=n);
    memcpy(out,file?(const void*)&stored:(const void*)done,n);*size=n;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size)
{
    assert(h==1);bool fail=fault();if(fail&&!after_write)return ESP_FAIL;
    if(!strcmp(key,"qso_v1")){assert(size==sizeof(stored));memcpy(&stored,data,size);has_stage=true;}
    else {assert(!strcmp(key,"applied_v1")&&size==32);memcpy(done,data,32);has_done=true;}
    return fail?ESP_FAIL:ESP_OK;
}
esp_err_t nvs_set_i32(nvs_handle_t h,const char *key,int32_t value)
{
    assert(h==2);int i=field(key);assert(i<4);bool fail=fault();if(fail&&!after_write)return ESP_FAIL;
    saved_numbers[i]=value;saved_mask|=1u<<i;setting_writes++;return fail?ESP_FAIL:ESP_OK;
}
esp_err_t nvs_set_str(nvs_handle_t h,const char *key,const char *value)
{
    assert(h==2);int i=field(key);assert(i>=4);bool fail=fault();if(fail&&!after_write)return ESP_FAIL;
    assert(strlen(value)<65);strcpy(saved_strings[i-4],value);saved_mask|=1u<<i;setting_writes++;
    return fail?ESP_FAIL:ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) {assert(h==1||h==2);return fault()?ESP_FAIL:ESP_OK;}
void nvs_close(nvs_handle_t h) {assert(h==1||h==2);}
esp_err_t nvs_entry_find(const char *label,const char *ns,unsigned type,nvs_iterator_t *it)
{assert(!strcmp(label,"appdata0")&&!ns&&type==NVS_TYPE_ANY);*it=NULL;if(fault())return ESP_FAIL;return saved_mask||has_stage||has_done?ESP_OK:ESP_ERR_NVS_NOT_FOUND;}
void nvs_release_iterator(nvs_iterator_t it) {assert(!it);}
int mbedtls_sha256(const unsigned char *data,size_t size,unsigned char out[32],int is224)
{(void)data;assert(size==1024&&!is224);memset(out,0,32);return 0;}

static passport_qso_data_t sample(void)
{
    passport_qso_data_t p={0};memcpy(p.data.magic,"PPTQSO1",8);p.data.format=1;
    p.data.present=0x7fffff;p.data.numbers[0]=91;p.data.numbers[1]=75;p.data.numbers[2]=3;p.data.numbers[3]=60;
    for(unsigned i=0;i<8;i++){strcpy(p.data.profile_ssids[i],"TEST-NETWORK");strcpy(p.data.profile_passwords[i],"NOT-A-REAL-PASSWORD");}
    strcpy(p.data.legacy_ssid,"OLD-TEST");strcpy(p.data.legacy_password,"OLD-TEST-PASSWORD");strcpy(p.data.last_ssid,"TEST-NETWORK");
    return p;
}
static void reset(void)
{
    passport_registry_init(&registry);strcpy(registry.slots[0].package.app_id,PASSPORT_QSO_ID);registry.slots[0].state=PASSPORT_READY;registry.slots[0].data_offset=part.address;
    has_stage=has_done=stage_ns=settings_ns=false;saved_mask=0;setting_writes=0;operation=fail_at=0;after_write=false;
    memset(&stored,0,sizeof(stored));memset(done,0,sizeof(done));memset(saved_numbers,0,sizeof(saved_numbers));memset(saved_strings,0,sizeof(saved_strings));
}
static void verify(const passport_qso_data_t *p)
{
    assert(saved_mask==p->data.present && has_done);
    for(unsigned i=0;i<4;i++)assert(saved_numbers[i]==p->data.numbers[i]);
    for(unsigned i=4;i<PASSPORT_QSO_FIELDS;i++)assert(!strcmp(saved_strings[i-4],passport_qso_field_string(&p->data,i)));
}
int main(int argc, char **argv)
{
    if(argc==2) {
        passport_qso_data_t fixture;FILE *file=fopen(argv[1],"rb");assert(file);
        assert(fread(&fixture,1,sizeof(fixture),file)==sizeof(fixture)&&fgetc(file)==EOF);fclose(file);
        assert(passport_qso_data_valid(&fixture)&&fixture.data.present==0x7fffff);
        assert(fixture.data.numbers[1]==75&&!strcmp(fixture.data.profile_ssids[7],"TEST-NETWORK-7"));
    }
    passport_qso_data_t p=sample();assert(passport_qso_data_valid(&p));reset();
    assert(passport_qso_stage(0,&p)==ESP_OK);int stage_ops=operation;operation=0;
    assert(passport_qso_apply_migration()==ESP_OK);int apply_ops=operation;verify(&p);
    for(int mode=0;mode<2;mode++)for(int fail=1;fail<=stage_ops;fail++) {
        reset();after_write=mode;fail_at=fail;
        assert(passport_qso_stage(0,&p)!=ESP_OK);fail_at=0;
        assert(passport_qso_stage(0,&p)==ESP_OK);assert(passport_qso_apply_migration()==ESP_OK);verify(&p);
    }
    for(int mode=0;mode<2;mode++)for(int fail=1;fail<=apply_ops;fail++) {
        reset();assert(passport_qso_stage(0,&p)==ESP_OK);operation=0;fail_at=fail;after_write=mode;
        assert(passport_qso_apply_migration()!=ESP_OK);fail_at=0;
        assert(passport_qso_apply_migration()==ESP_OK);verify(&p);
    }
    reset();saved_mask=2;saved_numbers[1]=19;
    assert(passport_qso_stage(0,&p)==ESP_ERR_INVALID_STATE && saved_numbers[1]==19 && !has_stage);
    reset();assert(passport_qso_stage(0,&p)==ESP_OK);passport_qso_data_t changed=p;changed.data.numbers[1]=20;
    assert(passport_qso_stage(0,&changed)==ESP_ERR_INVALID_STATE && stored.data.numbers[1]==75);
    assert(passport_qso_apply_migration()==ESP_OK);saved_numbers[1]=12;unsigned writes=setting_writes;
    assert(passport_qso_stage(0,&p)==ESP_OK && passport_qso_apply_migration()==ESP_OK);
    assert(saved_numbers[1]==12 && setting_writes==writes);
    reset();assert(passport_qso_apply_migration()==ESP_OK && !has_stage && !setting_writes);
    p.sha256[0]=1;assert(passport_qso_stage(0,&p)==ESP_ERR_INVALID_CRC && !has_stage);
    p=sample();p.data.present|=1u<<31;assert(!passport_qso_data_valid(&p));
    p=sample();p.data.reserved[0]=1;assert(!passport_qso_data_valid(&p));
    p=sample();memset(p.data.profile_passwords[0],'x',65);assert(!passport_qso_data_valid(&p));
    p=sample();p.data.present&=~(1u<<4);assert(!passport_qso_data_valid(&p));
    p=sample();p.data.numbers[1]=101;assert(!passport_qso_data_valid(&p));
    printf("Production QSO migration: %d staging + %d application boundaries, before/after-write failures, retry and no overwrite: PASS\n",stage_ops,apply_ops);
}
