#include "passport_qso_migration.h"
#include "passport_sdk.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "mbedtls/sha256.h"
#include <string.h>
#include <stdlib.h>

static esp_err_t validate(const passport_qso_data_t *data)
{
    uint8_t digest[32];
    if (!passport_qso_data_valid(data)) return ESP_ERR_INVALID_ARG;
    if (mbedtls_sha256((const uint8_t *)&data->data, sizeof(data->data), digest, 0) != 0 ||
        memcmp(digest, data->sha256, 32)) return ESP_ERR_INVALID_CRC;
    return ESP_OK;
}

static esp_err_t read_blob(nvs_handle_t h, const char *key, void *out, size_t size)
{
    size_t actual = size;
    esp_err_t err = nvs_get_blob(h, key, out, &actual);
    return err == ESP_OK && actual != size ? ESP_ERR_INVALID_SIZE : err;
}

static esp_err_t stage(unsigned slot, const passport_qso_data_t *data,const passport_registry_t *r)
{
    esp_err_t err = validate(data);
    if (err != ESP_OK) return err;
    if (slot >= PASSPORT_SLOTS || r->slots[slot].state != PASSPORT_READY ||
        strcmp(r->slots[slot].package.app_id, PASSPORT_QSO_ID)) return ESP_ERR_INVALID_STATE;
    char label[] = "appdata0"; label[7] = "0123456789abcdef"[slot];
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_NVS, label);
    if (!part || part->address != r->slots[slot].data_offset || part->size != PASSPORT_DATA_SIZE) return ESP_ERR_INVALID_STATE;
    err = nvs_flash_init_partition(label);
    if (err != ESP_OK) return err;
    nvs_handle_t h;
    err = nvs_open_from_partition(label, "pos_migrate", NVS_READONLY, &h);
    if (err == ESP_OK) {
        passport_qso_data_t existing;
        err = read_blob(h, "qso_v1", &existing, sizeof(existing));
        nvs_close(h);
        if (err == ESP_OK) return !memcmp(&existing, data, sizeof(existing)) ? ESP_OK : ESP_ERR_INVALID_STATE;
        if (err != ESP_ERR_NVS_NOT_FOUND) return err;
    } else if (err != ESP_ERR_NVS_NOT_FOUND) return err;
    /* Refuse ALL existing private records, including namespaces unknown to us. */
    nvs_iterator_t it = NULL;
    err = nvs_entry_find(label, NULL, NVS_TYPE_ANY, &it);
    nvs_release_iterator(it);
    if (err == ESP_OK) return ESP_ERR_INVALID_STATE;
    if (err != ESP_ERR_NVS_NOT_FOUND) return err;
    err = nvs_open_from_partition(label, "pos_migrate", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, "qso_v1", data, sizeof(*data));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t passport_qso_stage(unsigned slot,const passport_qso_data_t *data)
{
    passport_registry_t *r=malloc(sizeof(*r));if(!r)return ESP_ERR_NO_MEM;
    esp_err_t err=passport_registry_load(r);
    if(err==ESP_OK)err=stage(slot,data,r);
    free(r);return err;
}

esp_err_t passport_qso_apply_migration(void)
{
    nvs_handle_t stage;
    esp_err_t err = passport_app_open(PASSPORT_QSO_ID, "pos_migrate", NVS_READONLY, &stage);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    passport_qso_data_t data;
    err = read_blob(stage, "qso_v1", &data, sizeof(data));
    if (err == ESP_ERR_NVS_NOT_FOUND) { nvs_close(stage); return ESP_OK; }
    if (err == ESP_OK) err = validate(&data);
    if (err != ESP_OK) { nvs_close(stage); return err; }
    uint8_t applied[32];
    err = read_blob(stage, "applied_v1", applied, sizeof(applied));
    nvs_close(stage);
    if (err == ESP_OK) return !memcmp(applied, data.sha256, 32) ? ESP_OK : ESP_ERR_INVALID_STATE;
    if (err != ESP_ERR_NVS_NOT_FOUND) return err;
    nvs_handle_t settings;
    err = passport_app_open(PASSPORT_QSO_ID, "monitor", NVS_READWRITE, &settings);
    if (err != ESP_OK) return err;
    for (unsigned i = 0; i < PASSPORT_QSO_FIELDS && err == ESP_OK; ++i) {
        if (!(data.data.present & (1u << i))) continue;
        const char *key = passport_qso_field_key(i);
        err = i < 4 ? nvs_set_i32(settings, key, data.data.numbers[i]) :
            nvs_set_str(settings, key, passport_qso_field_string(&data.data, i));
    }
    if (err == ESP_OK) err = nvs_commit(settings);
    nvs_close(settings);
    if (err != ESP_OK) return err;
    err = passport_app_open(PASSPORT_QSO_ID, "pos_migrate", NVS_READWRITE, &stage);
    if (err != ESP_OK) return err;
    /* A durable completion digest is written only after all setting writes. */
    err = nvs_set_blob(stage, "applied_v1", data.sha256, 32);
    if (err == ESP_OK) err = nvs_commit(stage);
    nvs_close(stage);
    return err;
}
