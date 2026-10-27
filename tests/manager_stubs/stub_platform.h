#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "passport_model.h"
typedef int esp_err_t;
enum {ESP_OK=0, ESP_FAIL=-1, ESP_ERR_NO_MEM=1, ESP_ERR_INVALID_STATE,
      ESP_ERR_INVALID_ARG, ESP_ERR_NOT_FOUND, ESP_ERR_OTA_VALIDATE_FAILED, ESP_ERR_INVALID_VERSION,
      ESP_ERR_INVALID_CRC, ESP_ERR_INVALID_SIZE, ESP_ERR_NVS_NOT_FOUND, ESP_ERR_NVS_NOT_INITIALIZED};
enum {ESP_PARTITION_TYPE_APP=0, ESP_PARTITION_TYPE_DATA=1,
      ESP_PARTITION_SUBTYPE_APP_OTA_0=16, ESP_PARTITION_SUBTYPE_DATA_NVS=2};
enum {ESP_IMAGE_HEADER_MAGIC=0xe9, ESP_CHIP_ID_ESP32C3=5,
      ESP_IMAGE_SPI_MODE_DIO=2, ESP_IMAGE_FLASH_SIZE_8MB=3, ESP_IMAGE_SPI_SPEED_DIV_1=15};
#define ESP_APP_DESC_MAGIC_WORD 0xabcd5432u
typedef struct {
    uint8_t magic, segment_count, spi_mode;
    uint8_t spi_speed:4, spi_size:4;
    uint32_t entry_addr;
    uint8_t unused[4];
    uint16_t chip_id;
    uint8_t revisions[9], hash_appended;
} esp_image_header_t;
_Static_assert(sizeof(esp_image_header_t)==24,"image test header");
typedef struct {size_t content_len;} httpd_req_t;
typedef struct {uint8_t type, subtype; uint32_t address, size;} esp_partition_t;
typedef uint32_t esp_ota_handle_t;
typedef int nvs_open_mode_t;
typedef int nvs_handle_t;
typedef void *nvs_iterator_t;
enum {NVS_READONLY=0, NVS_READWRITE=1, NVS_TYPE_ANY=255};
typedef bool *SemaphoreHandle_t;
typedef struct {uint8_t custom[32];} rtc_retain_mem_t;
#ifdef PASSPORT_TEST_REAL_SHA
#include <openssl/evp.h>
typedef struct {EVP_MD_CTX *context;} mbedtls_sha256_context;
#else
typedef struct {unsigned unused;} mbedtls_sha256_context;
#endif
#define pdTRUE 1
#define taskYIELD() ((void)0)
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t mutex, int wait);
void xSemaphoreGive(SemaphoreHandle_t mutex);
const esp_partition_t *esp_partition_find_first(unsigned type, unsigned subtype, const char *label);
esp_err_t esp_partition_read(const esp_partition_t *p, size_t offset, void *out, size_t size);
esp_err_t esp_partition_erase_range(const esp_partition_t *p, size_t offset, size_t size);
esp_err_t nvs_flash_erase_partition(const char *label);
int httpd_req_recv(httpd_req_t *req, char *out, size_t size);
esp_err_t esp_ota_begin(const esp_partition_t *p, size_t size, esp_ota_handle_t *handle);
esp_err_t esp_ota_write(esp_ota_handle_t h, const void *data, size_t n);
esp_err_t esp_ota_end(esp_ota_handle_t h);
esp_err_t esp_ota_abort(esp_ota_handle_t h);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p);
const esp_partition_t *esp_ota_get_running_partition(void);
rtc_retain_mem_t *bootloader_common_get_rtc_retain_mem(void);
void esp_restart(void);
int64_t esp_timer_get_time(void);
void mbedtls_sha256_init(mbedtls_sha256_context *c);
void mbedtls_sha256_free(mbedtls_sha256_context *c);
int mbedtls_sha256_starts(mbedtls_sha256_context *c, int is224);
int mbedtls_sha256_update(mbedtls_sha256_context *c, const unsigned char *data, size_t n);
int mbedtls_sha256_finish(mbedtls_sha256_context *c, unsigned char out[32]);
int mbedtls_sha256(const unsigned char *data, size_t n, unsigned char out[32], int is224);
esp_err_t nvs_flash_init_partition(const char *label);
esp_err_t nvs_open_from_partition(const char *part, const char *ns, int mode, nvs_handle_t *h);
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *size);
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *data, size_t size);
esp_err_t nvs_set_i32(nvs_handle_t h, const char *key, int32_t value);
esp_err_t nvs_get_i32(nvs_handle_t h, const char *key, int32_t *value);
esp_err_t nvs_set_str(nvs_handle_t h, const char *key, const char *value);
esp_err_t nvs_commit(nvs_handle_t h);
void nvs_close(nvs_handle_t h);
esp_err_t nvs_entry_find(const char *part, const char *ns, unsigned type, nvs_iterator_t *it);
void nvs_release_iterator(nvs_iterator_t it);

esp_err_t nvs_flash_deinit_partition(const char *label);
esp_err_t esp_partition_write(const esp_partition_t *p,size_t offset,const void *data,size_t size);

esp_err_t esp_partition_register_external(void *chip,size_t address,size_t size,const char *label,unsigned type,unsigned subtype,const esp_partition_t **out);
esp_err_t esp_partition_deregister_external(const esp_partition_t *p);
esp_err_t esp_flash_read(void *chip,void *out,uint32_t address,uint32_t size);
