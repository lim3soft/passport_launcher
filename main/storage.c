#include "storage.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>
static const esp_partition_t *storage_code[PASSPORT_SLOTS],*storage_data[PASSPORT_SLOTS];
static esp_err_t update(const esp_partition_t **p,uint32_t address,uint32_t size,
                        const char *label,bool app,unsigned slot)
{
    if(*p && (*p)->address==address && (*p)->size==size)return ESP_OK;
    if(*p) {
        if(!app) {
            esp_err_t e=nvs_flash_deinit_partition(label);
            if(e!=ESP_OK && e!=ESP_ERR_NVS_NOT_INITIALIZED)return e;
        }
        esp_err_t e=esp_partition_deregister_external(*p);
        if(e!=ESP_OK)return e;
        *p=NULL;
    }
    if(!size)return ESP_OK;
    return esp_partition_register_external(NULL,address,size,label,
        app?ESP_PARTITION_TYPE_APP:ESP_PARTITION_TYPE_DATA,
        app?ESP_PARTITION_SUBTYPE_APP_OTA_0+slot:ESP_PARTITION_SUBTYPE_DATA_NVS,p);
}
esp_err_t passport_storage_sync(const passport_registry_t *r)
{
    if(!passport_registry_valid(r))return ESP_ERR_INVALID_STATE;
    /* Remove released/moved storage_code before adding any allocation that may reuse it. */
    for(unsigned i=0;i<PASSPORT_SLOTS;i++) {
        const passport_record_t *a=&r->slots[i];char label[]="appdata0";label[7]="0123456789abcdef"[i];
        if(storage_code[i] && (a->state==PASSPORT_EMPTY || storage_code[i]->address!=a->image_offset || storage_code[i]->size!=a->image_capacity)) {
            esp_err_t e=update(&storage_code[i],0,0,"",true,i);if(e!=ESP_OK)return e;
        }
        if(storage_data[i] && (a->state==PASSPORT_EMPTY || storage_data[i]->address!=a->data_offset)) {
            esp_err_t e=update(&storage_data[i],0,0,label,false,i);if(e!=ESP_OK)return e;
        }
    }
    for(unsigned i=0;i<PASSPORT_SLOTS;i++)if(r->slots[i].state!=PASSPORT_EMPTY) {
        const passport_record_t *a=&r->slots[i];char app[16],label[]="appdata0";
        snprintf(app,sizeof(app),"app%u",i);label[7]="0123456789abcdef"[i];
        esp_err_t e=update(&storage_code[i],a->image_offset,a->image_capacity,app,true,i);
        if(e==ESP_OK)e=update(&storage_data[i],a->data_offset,PASSPORT_DATA_SIZE,label,false,i);
        if(e!=ESP_OK)return e;
    }
    return ESP_OK;
}
