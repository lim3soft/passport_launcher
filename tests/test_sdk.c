#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../components/passport_sdk/passport_sdk.c"

static passport_registry_t stored;
static esp_partition_t meta, data, running;
static int meta_inits, data_inits, private_opens, saves;
static bool bad_read, init_failure, missing_blob;
static const char *id="org.passportos.test";
const esp_partition_t *esp_partition_find_first(unsigned type,unsigned subtype,const char *label)
{
    assert(type==1&&subtype==2);
    if(!strcmp(label,"pos_meta"))return &meta;
    assert(!strcmp(label,"appdata1"));return &data;
}
const esp_partition_t *esp_ota_get_running_partition(void) {return &running;}
esp_err_t nvs_flash_init_partition(const char *label)
{
    if(!strcmp(label,"pos_meta")){meta_inits++;return init_failure?ESP_FAIL:ESP_OK;}
    assert(!strcmp(label,"appdata1"));data_inits++;return ESP_OK;
}
esp_err_t nvs_open_from_partition(const char *label,const char *ns,int mode,nvs_handle_t *h)
{
    (void)mode;
    if(!strcmp(label,"pos_meta")){assert(!strcmp(ns,"passport"));*h=1;}
    else {assert(!strcmp(label,"appdata1")&&!strcmp(ns,"same-key-space"));private_opens++;*h=2;}
    return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size)
{
    assert(h==1&&!strcmp(key,"registry_v2"));
    if(missing_blob)return ESP_ERR_NVS_NOT_FOUND;
    memcpy(out,&stored,sizeof(stored));*size=sizeof(stored)-(bad_read?1:0);return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *p,size_t size)
{assert(h==1&&!strcmp(key,"registry_v2")&&size==sizeof(stored));stored=*(const passport_registry_t*)p;saves++;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h) {assert(h==1);return ESP_OK;}
void nvs_close(nvs_handle_t h) {assert(h==1||h==2);}
void esp_restart(void) {abort();}
static void reset(void)
{
    passport_registry_init(&stored);
    passport_package_t p={.format=1,.abi=PASSPORT_ABI,.image_size=1000};
    memcpy(p.magic,PASSPORT_MAGIC,8);strcpy(p.app_id,"org.placeholder");strcpy(p.name,"Test");strcpy(p.version,"1");
    assert(passport_install_prepare(&stored,&p,0)&&passport_install_finish(&stored,0));
    strcpy(p.app_id,id);assert(passport_install_prepare(&stored,&p,1)&&passport_install_finish(&stored,1));
    meta=(esp_partition_t){1,2,PASSPORT_META_ADDRESS,0x10000};data=(esp_partition_t){1,2,stored.slots[1].data_offset,PASSPORT_DATA_SIZE};
    running=(esp_partition_t){0,16,stored.slots[1].image_offset,stored.slots[1].image_capacity};
    meta_inits=data_inits=private_opens=saves=0;bad_read=init_failure=missing_blob=false;
}
int main(void)
{
    nvs_handle_t h;reset();
    assert(passport_app_open(id,"same-key-space",NVS_READWRITE,&h)==ESP_OK&&h==2&&data_inits==1&&private_opens==1);
    reset();assert(passport_app_open("org.other","same-key-space",NVS_READWRITE,&h)!=ESP_OK&&!data_inits&&!private_opens);
    reset();data.address=0x7a0000;
    assert(passport_app_open(id,"same-key-space",NVS_READWRITE,&h)!=ESP_OK&&!data_inits&&!private_opens);
    reset();data.size=0x40000;
    assert(passport_app_open(id,"same-key-space",NVS_READWRITE,&h)!=ESP_OK&&!data_inits);
    reset();meta.address=0x7c0000;
    assert(passport_app_open(id,"same-key-space",NVS_READWRITE,&h)!=ESP_OK&&!meta_inits&&!data_inits);
    reset();running.address=0x190000;
    assert(passport_app_open(id,"same-key-space",NVS_READWRITE,&h)!=ESP_OK&&!data_inits);
    reset();running.subtype=0;
    assert(passport_app_open(id,"same-key-space",NVS_READWRITE,&h)!=ESP_OK&&!data_inits);
    reset();stored.slots[1].state=PASSPORT_INSTALLING;
    assert(passport_app_open(id,"same-key-space",NVS_READWRITE,&h)!=ESP_OK&&!data_inits);
    reset();bad_read=true;
    assert(passport_app_open(id,"same-key-space",NVS_READWRITE,&h)!=ESP_OK&&!data_inits&&!saves);
    reset();stored.schema=3;
    assert(passport_app_open(id,"same-key-space",NVS_READWRITE,&h)!=ESP_OK&&!data_inits&&!saves);
    reset();init_failure=true;
    assert(passport_app_open(id,"same-key-space",NVS_READWRITE,&h)==ESP_FAIL&&!data_inits&&!saves);
    reset();missing_blob=true;
    assert(passport_app_open(id,"same-key-space",NVS_READWRITE,&h)!=ESP_OK&&!data_inits&&!saves);
    reset();stored.launch_pending=1;
    assert(passport_app_mark_healthy(id)==ESP_OK&&!stored.launch_pending&&saves==1&&!private_opens);
    puts("Production SDK: stable ID, actual running partition, private bounds, metadata failures and health acknowledgement: PASS");
}
