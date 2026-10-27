#include "passport_sdk.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include <string.h>
#include <stdlib.h>

esp_err_t passport_registry_load(passport_registry_t *r)
{
    if (!r) return ESP_ERR_INVALID_ARG;
    const esp_partition_t *meta = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_NVS, "pos_meta");
    if (!meta || meta->address != PASSPORT_META_ADDRESS || meta->size != 0x10000) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_flash_init_partition("pos_meta");
    if (err != ESP_OK) return err;
    nvs_handle_t h;
    err = nvs_open_from_partition("pos_meta", "passport", NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return err;
    if (err != ESP_OK) return err;
    size_t size = sizeof(*r);
    err = nvs_get_blob(h, "registry_v2", r, &size);
    nvs_close(h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return err;
    if (err != ESP_OK) return err;
    return size == sizeof(*r) && passport_registry_valid(r) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t passport_registry_save(const passport_registry_t *r)
{
    if (!passport_registry_valid(r)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open_from_partition("pos_meta", "passport", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, "registry_v2", r, sizeof(*r));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static int app_slot(const passport_registry_t *r, const char *id)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if(!id || !running || running->type!=ESP_PARTITION_TYPE_APP || running->subtype!=ESP_PARTITION_SUBTYPE_APP_OTA_0)return -1;
    for(unsigned slot=0;slot<PASSPORT_SLOTS;slot++) {
        const passport_record_t *a=&r->slots[slot];
        if(a->state==PASSPORT_READY && !strcmp(a->package.app_id,id) &&
            running->address==a->image_offset && running->size==a->image_capacity)return (int)slot;
    }
    return -1;
}

esp_err_t passport_app_open(const char *id, const char *ns, nvs_open_mode_t mode, nvs_handle_t *handle)
{
    passport_registry_t *r=malloc(sizeof(*r));if(!r)return ESP_ERR_NO_MEM;
    esp_err_t err=passport_registry_load(r);
    int slot=err==ESP_OK?app_slot(r,id):-1;
    if(err==ESP_OK && slot<0)err=ESP_ERR_INVALID_STATE;
    if(err==ESP_OK) {
        char label[]="appdata0";label[7]="0123456789abcdef"[slot];
        const esp_partition_t *data=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,label);
        if(!data || data->address!=r->slots[slot].data_offset || data->size!=PASSPORT_DATA_SIZE)err=ESP_ERR_INVALID_STATE;
        else {
            err=nvs_flash_init_partition(label);
            if(err==ESP_OK)err=nvs_open_from_partition(label,ns,mode,handle);
        }
    }
    free(r);return err;
}
esp_err_t passport_app_mark_healthy(const char *id)
{
    passport_registry_t *r=malloc(sizeof(*r));if(!r)return ESP_ERR_NO_MEM;
    esp_err_t err=passport_registry_load(r);
    if(err==ESP_OK) {
        if(app_slot(r,id)<0)err=ESP_ERR_INVALID_STATE;
        else {r->launch_pending=0;err=passport_registry_save(r);}
    }
    free(r);return err;
}

void passport_return_to_launcher(void) { esp_restart(); }
