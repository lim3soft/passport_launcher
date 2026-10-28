#include "manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_flash.h"
#include "esp_app_format.h"
#include "esp_app_desc.h"
#include "esp_image_format.h"
#include "bootloader_common.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "mbedtls/sha256.h"
#include "passport_qso_migration.h"
#include "compat.h"
#include "storage.h"
#include "autostart.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>

static SemaphoreHandle_t mutex;
static passport_registry_t registry,next;
static char startup_id[64];
static atomic_bool install_enabled;
static bool storage_blocked;
static atomic_uint import_state;

void passport_install_enable(bool enabled)
{
    atomic_store(&install_enabled, enabled);
    if (enabled) passport_import_note(IMPORT_IDLE, 0);
}

void passport_import_note(passport_import_notice_t notice, unsigned bytes)
{
    if (notice > IMPORT_FAILED) return;
    /* One atomic word keeps the reason and displayed size coherent. */
    unsigned kib=bytes/1024u+(bytes%1024u!=0);
    atomic_store(&import_state, ((unsigned)notice<<24)|kib);
}

bool passport_import_message(char *out, size_t size)
{
    unsigned state=atomic_load(&import_state),notice=state>>24,kib=state&0xffffffu;
    const char *text = notice == IMPORT_READY ? "Space checked. Ready to install." :
        notice == IMPORT_NO_SPACE ? "Not enough usable space.\nUninstall an app first." :
        notice == IMPORT_LAYOUT ? "Unsupported ROM layout.\nCheck firmware compatibility." :
        notice == IMPORT_INVALID ? "ROM validation failed.\nChoose a compatible BIN." :
        notice == IMPORT_WRITING ? "Installing... keep power on." :
        notice == IMPORT_DONE ? "ROM installed. Hold OK to return." :
        notice == IMPORT_FAILED ? "Installation failed.\nRetry the same BIN file." : "";
    if (notice == IMPORT_TOO_LARGE) snprintf(out, size, "ROM needs %u KiB.\nNot enough usable flash.", kib);
    else snprintf(out, size, "%s", text);
    return notice != IMPORT_IDLE;
}

static const esp_partition_t *app_partition(unsigned slot)
{
    if (slot >= PASSPORT_SLOTS) return NULL;
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_APP_OTA_0 + slot, NULL);
}

esp_err_t passport_manager_init(void)
{
    mutex = xSemaphoreCreateMutex();
    if (!mutex) return ESP_ERR_NO_MEM;
    esp_err_t err = passport_registry_load(&registry);
    bool fresh=err==ESP_ERR_NVS_NOT_FOUND;
    if(fresh)passport_registry_init(&registry);
    else if(err!=ESP_OK)return err;
    err=passport_autostart_load(startup_id);
    if(err!=ESP_OK)return err;
    err = passport_storage_sync(&registry);
    if(err==ESP_OK)err = passport_compat_recover(&registry);
    storage_blocked = err != ESP_OK;
    if (err != ESP_OK) return err;
    if(fresh) {
            /* A missing v2 registry is never permission to reuse old live flash. */
            uint8_t probe[1024];
            for(uint32_t address=PASSPORT_POOL_START;address<PASSPORT_POOL_END;address+=sizeof(probe)) {
                if(esp_flash_read(NULL,probe,address,sizeof(probe))!=ESP_OK)return ESP_FAIL;
                for(unsigned j=0;j<sizeof(probe);j++)if(probe[j]!=255)return ESP_ERR_INVALID_STATE;
                taskYIELD();
            }
        err=passport_registry_save(&registry);
        if(err!=ESP_OK)return err;
    }
    return ESP_OK;
}

bool passport_manager_busy(void)
{
    if(!mutex || xSemaphoreTake(mutex,0)!=pdTRUE)return true;
    xSemaphoreGive(mutex);return false;
}

bool passport_snapshot(passport_registry_t *out)
{
    if (!mutex || xSemaphoreTake(mutex, 0) != pdTRUE) return false;
    *out = registry;
    xSemaphoreGive(mutex);
    return true;
}

static esp_err_t publish(const passport_registry_t *candidate)
{
    esp_err_t err = passport_registry_save(candidate);
    if (err == ESP_OK) registry = *candidate;
    else {
        /* A failed NVS commit may already be durable. Re-read ownership before
         * admitting another upload into variable extents. Fail closed if unsure. */
        esp_err_t reload=passport_registry_load(&next);
        if(reload==ESP_OK && passport_registry_valid(&next)) {
            registry=next;
            if(passport_storage_sync(&registry)!=ESP_OK)storage_blocked=true;
        } else storage_blocked=true;
    }
    return err;
}

esp_err_t passport_prepare_web(void)
{
    if (!mutex || xSemaphoreTake(mutex, 0) != pdTRUE) return ESP_ERR_INVALID_STATE;
    esp_err_t err = passport_compat_recover(&registry);
    storage_blocked = err != ESP_OK;
    xSemaphoreGive(mutex);
    return err;
}

static esp_err_t receive_exact(httpd_req_t *req, void *data, size_t count, int64_t deadline)
{
    uint8_t *p = data;
    while (count) {
        if (!atomic_load(&install_enabled) || esp_timer_get_time() >= deadline) return ESP_FAIL;
        int received = httpd_req_recv(req, (char *)p, count);
        if (received <= 0) return ESP_FAIL;
        p += received; count -= received;
    }
    return ESP_OK;
}

static esp_err_t check_image_marker(const esp_partition_t *app, size_t image_size,
                                   const char *marker, size_t marker_len)
{
    uint8_t *buffer = malloc(4096 + marker_len);
    if (!buffer) return ESP_ERR_NO_MEM;
    size_t carry = 0;
    esp_err_t err = ESP_ERR_INVALID_VERSION;
    for (size_t offset = 0; offset < image_size; offset += 4096) {
        size_t n = image_size - offset < 4096 ? image_size - offset : 4096;
        esp_err_t read_err = esp_partition_read(app, offset, buffer + carry, n);
        if (read_err != ESP_OK) { err = read_err; break; }
        size_t total = carry + n;
        for (size_t i = 0; i + marker_len <= total; ++i) {
            if (!memcmp(buffer + i, marker, marker_len)) { err = ESP_OK; break; }
        }
        if (err == ESP_OK) break;
        carry = total < marker_len - 1 ? total : marker_len - 1;
        memmove(buffer, buffer + total - carry, carry);
    }
    free(buffer);
    return err;
}

static esp_err_t check_app_declaration(const esp_partition_t *app, const passport_package_t *p)
{
    char marker[81];
    size_t length = 16 + strlen(p->app_id) + 1;
    memcpy(marker, "PASSPORTOS_ABI2:", 16);
    memcpy(marker + 16, p->app_id, length - 16);
    return check_image_marker(app, p->image_size, marker, length);
}

static bool image_idf_supported(const uint8_t *buffer, bool native)
{
    const char *idf = (const char *)buffer + 144;
    if (!memchr(idf, 0, 32)) return false;
    if (native) return !memcmp(idf, "v5.5.3", 6) && (!idf[6] || idf[6] == '-');
    if (memcmp(idf, "v5.5", 4)) return false;
    const char *p = idf + 4;
    if (*p == '.') {
        ++p;
        if (*p < '0' || *p > '9') return false;
        while (*p >= '0' && *p <= '9') ++p;
    }
    return !*p || *p == '-';
}

esp_err_t passport_preflight_http(httpd_req_t *req)
{
    if (storage_blocked || !atomic_load(&install_enabled) || xSemaphoreTake(mutex, 0) != pdTRUE)
        return ESP_ERR_INVALID_STATE;
    esp_err_t err = ESP_ERR_INVALID_ARG;
    passport_import_notice_t note = IMPORT_INVALID;
    passport_package_t p;
    if (req->content_len != sizeof(p) || receive_exact(req, &p, sizeof(p), esp_timer_get_time()+10000000) != ESP_OK)
        goto done;
    if (p.image_size > PASSPORT_SLOT_SIZE) { note = IMPORT_TOO_LARGE; err = ESP_ERR_INVALID_SIZE; goto done; }
    if (!passport_package_valid(&p)) goto done;
    int slot = passport_install_slot(&registry, &p);
    if(slot>=0 && registry.slots[slot].state!=PASSPORT_EMPTY &&
       !passport_package_layout_equal(&p,&registry.slots[slot].package)) {
        note=IMPORT_LAYOUT;err=ESP_ERR_INVALID_VERSION;goto done;
    }
    passport_record_t planned;
    if(slot>=0 && !passport_install_plan(&registry,&p,slot,&planned))slot=-1;
    if (slot < 0) { note = IMPORT_NO_SPACE; err = ESP_ERR_NO_MEM; goto done; }
    if (registry.slots[slot].state != PASSPORT_EMPTY) {
        bool legacy = false;
        err = passport_compat_mode(&registry, slot, &legacy);
        if (err != ESP_OK) goto done;
        if (legacy != (p.abi == 0)) { err = ESP_ERR_INVALID_VERSION; goto done; }
    }
    err = ESP_OK; note = IMPORT_READY;
done:
    passport_import_note(note, note == IMPORT_TOO_LARGE ? p.image_size : 0);
    xSemaphoreGive(mutex);
    return err; /* Read-only metadata check. Installer rechecks to prevent races. */
}

esp_err_t passport_qso_import_http(httpd_req_t *req, unsigned slot)
{
    if (storage_blocked || !atomic_load(&install_enabled) || slot >= PASSPORT_SLOTS ||
        xSemaphoreTake(mutex, 0) != pdTRUE) return ESP_ERR_INVALID_STATE;
    esp_err_t err = ESP_ERR_INVALID_ARG;
    passport_qso_data_t *data = NULL;
    if (req->content_len != sizeof(*data) || registry.slots[slot].state != PASSPORT_READY ||
        strcmp(registry.slots[slot].package.app_id, PASSPORT_QSO_ID)) goto done;
    const esp_partition_t *app = app_partition(slot);
    if (!app) { err = ESP_ERR_NOT_FOUND; goto done; }
    err = check_image_marker(app, registry.slots[slot].package.image_size,
        PASSPORT_QSO_MIGRATION_MARKER, sizeof(PASSPORT_QSO_MIGRATION_MARKER));
    if (err != ESP_OK) goto done;
    data = malloc(sizeof(*data));
    if (!data) { err = ESP_ERR_NO_MEM; goto done; }
    err = receive_exact(req, data, sizeof(*data), esp_timer_get_time() + 30000000);
    if (err == ESP_OK && atomic_load(&install_enabled)) err = passport_qso_stage(slot, data);
    else if (err == ESP_OK) err = ESP_FAIL;
done:
    free(data);
    xSemaphoreGive(mutex);
    return err;
}

esp_err_t passport_install_http(httpd_req_t *req)
{
    if (storage_blocked || !atomic_load(&install_enabled)) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(mutex, 0) != pdTRUE) return ESP_ERR_INVALID_STATE;
    const int64_t deadline = esp_timer_get_time() + 300000000;
    esp_err_t err = ESP_ERR_INVALID_ARG;
    passport_package_t package;
    passport_import_notice_t note = IMPORT_FAILED;
    esp_ota_handle_t ota = 0;
    uint8_t *buffer = NULL;
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    if (req->content_len < sizeof(package) + 288 || req->content_len > sizeof(package) + PASSPORT_POOL_END-PASSPORT_POOL_START) {
        note = req->content_len > sizeof(package) + PASSPORT_POOL_END-PASSPORT_POOL_START ? IMPORT_TOO_LARGE : IMPORT_INVALID;
        goto done;
    }
    if (receive_exact(req, &package, sizeof(package), deadline) != ESP_OK || !passport_package_valid(&package) ||
        req->content_len != sizeof(package) + package.image_size + passport_package_data_size(&package) - (passport_package_data_size(&package)>PASSPORT_DATA_SIZE?PASSPORT_RESOURCE_BASE:PASSPORT_DATA_SIZE)) goto done;
    int slot = passport_install_slot(&registry, &package);
    if (slot < 0) { err = ESP_ERR_NO_MEM; note = IMPORT_NO_SPACE; goto done; }
    if(registry.slots[slot].state!=PASSPORT_EMPTY &&
       !passport_package_layout_equal(&package,&registry.slots[slot].package)) {
        err=ESP_ERR_INVALID_VERSION;note=IMPORT_LAYOUT;goto done;
    }
    if (registry.slots[slot].state != PASSPORT_EMPTY) {
        bool legacy = false;
        err = passport_compat_mode(&registry, slot, &legacy);
        if (err != ESP_OK) goto done;
        if (legacy != (package.abi == 0)) { err = ESP_ERR_INVALID_VERSION; goto done; }
    }
    buffer = malloc(4096);
    if (!buffer) { err = ESP_ERR_NO_MEM; goto done; }
    /* Read image + first segment + descriptor before touching flash. */
    err = receive_exact(req, buffer, 288, deadline);
    if (err != ESP_OK) goto done;
    err = ESP_ERR_INVALID_ARG;
    esp_image_header_t image;
    memcpy(&image, buffer, sizeof(image));
    uint32_t desc_magic;
    memcpy(&desc_magic, buffer + 32, sizeof(desc_magic));
    if (image.magic != ESP_IMAGE_HEADER_MAGIC || image.chip_id != ESP_CHIP_ID_ESP32C3 ||
        image.spi_mode != ESP_IMAGE_SPI_MODE_DIO || image.spi_size != ESP_IMAGE_FLASH_SIZE_8MB ||
        image.spi_speed != ESP_IMAGE_SPI_SPEED_DIV_1 || image.hash_appended != 1 ||
        desc_magic != ESP_APP_DESC_MAGIC_WORD || !image_idf_supported(buffer, package.abi != 0)) goto done;

    if (registry.slots[slot].state == PASSPORT_EMPTY) {
        /* Metadata only: establish mode before reserving the ID or erasing.
         * A stale mode for an EMPTY slot is ignored until that ID is assigned. */
        err = passport_compat_set_mode(slot, package.app_id, package.abi == 0);
        if (err != ESP_OK) goto done;
    }
    next = registry;
    if (!passport_install_prepare(&next, &package, slot)) { err = ESP_ERR_NO_MEM; note=IMPORT_NO_SPACE; goto done; }
    /* Durable INSTALLING MUST precede every destructive operation. */
    err = publish(&next);
    if (err != ESP_OK) goto done;
    err=passport_storage_sync(&registry);
    if(err!=ESP_OK){storage_blocked=true;goto done;}
    const esp_partition_t *target=app_partition(slot);
    if(!target){err=ESP_ERR_NOT_FOUND;goto done;}
    if(!registry.slots[slot].data_initialized) {
        char label[]="appdata0";label[7]="0123456789abcdef"[slot];
        err=nvs_flash_erase_partition(label);if(err!=ESP_OK)goto done;
    }
    passport_import_note(IMPORT_WRITING, package.image_size);
    err = esp_ota_begin(target, package.image_size, &ota);
    if (err != ESP_OK) goto done;
    if (mbedtls_sha256_starts(&sha, 0) != 0) { err = ESP_FAIL; goto done; }
    size_t remaining = package.image_size;
    size_t chunk = 288;
    while (remaining) {
        if (!atomic_load(&install_enabled) || esp_timer_get_time() >= deadline) { err = ESP_FAIL; goto done; }
        if (mbedtls_sha256_update(&sha, buffer, chunk) != 0) { err = ESP_FAIL; goto done; }
        err = esp_ota_write(ota, buffer, chunk);
        if (err != ESP_OK) goto done;
        remaining -= chunk;
        if (!remaining) break;
        chunk = remaining < 4096 ? remaining : 4096;
        err = receive_exact(req, buffer, chunk, deadline);
        if (err != ESP_OK) goto done;
        taskYIELD();
    }
    uint8_t digest[32];
    if (mbedtls_sha256_finish(&sha, digest) != 0 || memcmp(digest, package.sha256, 32)) {
        err = ESP_ERR_OTA_VALIDATE_FAILED; goto done;
    }
    err = esp_ota_end(ota); /* SDK checksum, image, chip and revision checks. */
    ota = 0;
    if (err != ESP_OK) goto done;
    if (package.abi == PASSPORT_ABI) err = check_app_declaration(target, &package);
    else {
        /* A native SDK declaration cannot be relabelled as a legacy app. */
        err = check_image_marker(target, package.image_size, "PASSPORTOS_ABI2:", 16);
        if (err == ESP_OK) err = ESP_ERR_INVALID_VERSION;
        else if (err == ESP_ERR_INVALID_VERSION) {
            err=check_image_marker(target,package.image_size,"PASSPORTOS_ABI1:",16);
            if(err==ESP_OK)err=ESP_ERR_INVALID_VERSION;
            else if(err==ESP_ERR_INVALID_VERSION)err=ESP_OK;
        }
    }
    if (err != ESP_OK) goto done;
    passport_layout_t layout;
    if(passport_package_layout(&package,&layout)) {
        const passport_record_t *record=&registry.slots[slot];
        bool initialize=!record->data_initialized;
        uint32_t address=record->data_offset+PASSPORT_RESOURCE_BASE;
        if(initialize) {
            err=esp_flash_erase_region(NULL,record->data_offset+PASSPORT_DATA_SIZE,0x1000+layout.payload_size);
            if(err!=ESP_OK)goto done;
        }
        if(mbedtls_sha256_starts(&sha,0)!=0){err=ESP_FAIL;goto done;}
        for(uint32_t offset=0;offset<layout.payload_size;) {
            size_t n=layout.payload_size-offset<4096?layout.payload_size-offset:4096;
            err=receive_exact(req,buffer,n,deadline);if(err!=ESP_OK)goto done;
            if(mbedtls_sha256_update(&sha,buffer,n)!=0){err=ESP_FAIL;goto done;}
            if(initialize) {
                err=esp_flash_write(NULL,buffer,address+offset,n);if(err!=ESP_OK)goto done;
                uint8_t check[256];
                for(size_t i=0;i<n;i+=sizeof(check)) {
                    size_t bytes=n-i<sizeof(check)?n-i:sizeof(check);
                    err=esp_flash_read(NULL,check,address+offset+i,bytes);
                    if(err!=ESP_OK || memcmp(check,buffer+i,bytes)){err=ESP_ERR_INVALID_CRC;goto done;}
                }
            }
            offset+=n;taskYIELD();
        }
        if(mbedtls_sha256_finish(&sha,digest)!=0 || memcmp(digest,layout.sha256,32)){err=ESP_ERR_INVALID_CRC;goto done;}
        /* Persist the bounded boot manifest only after complete payload validation.
         * Updates retain existing private bytes; changing their layout is refused. */
        uint32_t manifest=record->data_offset+PASSPORT_DATA_SIZE;
        err=esp_flash_erase_region(NULL,manifest,0x1000);if(err!=ESP_OK)goto done;
        err=esp_flash_write(NULL,&package,manifest,sizeof(package));if(err!=ESP_OK)goto done;
        passport_package_t check;
        err=esp_flash_read(NULL,&check,manifest,sizeof(check));
        if(err!=ESP_OK || memcmp(&check,&package,sizeof(check))){err=ESP_ERR_INVALID_CRC;goto done;}
    }
    next = registry;
    if (!passport_install_finish(&next, slot)) { err = ESP_ERR_INVALID_STATE; goto done; }
    err = publish(&next); /* Only READY images can be selected. No automatic boot. */
done:
    if (ota) esp_ota_abort(ota);
    passport_import_note(err == ESP_OK ? IMPORT_DONE : note,
        note == IMPORT_TOO_LARGE && req->content_len > sizeof(package) ? req->content_len-sizeof(package) : 0);
    mbedtls_sha256_free(&sha);
    free(buffer);
    xSemaphoreGive(mutex);
    return err;
}

static esp_err_t delete_app(unsigned slot, bool erase_private_data, const char *expected_id)
{
    if (storage_blocked || slot >= PASSPORT_SLOTS || xSemaphoreTake(mutex, 0) != pdTRUE) return ESP_ERR_INVALID_ARG;
    next = registry;
    esp_err_t err = ESP_ERR_NOT_FOUND;
    if (expected_id && strcmp(expected_id, registry.slots[slot].package.app_id)) {
        err = ESP_ERR_INVALID_STATE; goto done;
    }
    if (!passport_remove(&next, slot)) goto done;
    if(erase_private_data)next.slots[slot].data_initialized=0;
    err = publish(&next); /* Persist tombstone before erasing any code. */
    if (err != ESP_OK) goto done;
    if(erase_private_data && !strcmp(startup_id,registry.slots[slot].package.app_id)) {
        err=passport_autostart_save("");
        if(err!=ESP_OK)goto done;
        startup_id[0]=0;
    }
    const esp_partition_t *app = app_partition(slot);
    err = app ? esp_partition_erase_range(app, 0, app->size) : ESP_ERR_NOT_FOUND;
    if (err != ESP_OK || !erase_private_data) goto done;
    char label[] = "appdata0"; label[7] = "0123456789abcdef"[slot];
    err = nvs_flash_erase_partition(label);
    if (err != ESP_OK) goto done;
    uint32_t data_bytes=passport_package_data_size(&registry.slots[slot].package);
    if(data_bytes>PASSPORT_DATA_SIZE) {
        err=esp_flash_erase_region(NULL,registry.slots[slot].data_offset+PASSPORT_DATA_SIZE,data_bytes-PASSPORT_DATA_SIZE);
        if(err!=ESP_OK)goto done;
    }
    next = registry;
    memset(&next.slots[slot], 0, sizeof(next.slots[slot]));
    err = publish(&next); /* Reassignment allowed only AFTER successful data erase. */
    if(err==ESP_OK)err=passport_storage_sync(&registry);
done:
    xSemaphoreGive(mutex);
    return err;
}

esp_err_t passport_delete(unsigned slot, bool erase_private_data)
{ return delete_app(slot, erase_private_data, NULL); }

esp_err_t passport_uninstall(unsigned slot, const char *expected_id)
{
    if (!expected_id || !expected_id[0]) return ESP_ERR_INVALID_ARG;
    size_t length = 0;
    while (length < 64 && expected_id[length]) ++length;
    if (length == 64) return ESP_ERR_INVALID_ARG;
    return delete_app(slot, true, expected_id);
}

bool passport_autostart_configuration(char id[64])
{
    if(!mutex || xSemaphoreTake(mutex,0)!=pdTRUE)return false;
    memcpy(id,startup_id,64);xSemaphoreGive(mutex);return true;
}
unsigned passport_autostart_boot_slot(void)
{
    if(storage_blocked || !mutex || xSemaphoreTake(mutex,0)!=pdTRUE)return PASSPORT_NO_SLOT;
    unsigned slot=passport_autostart_target(&registry,startup_id);xSemaphoreGive(mutex);return slot;
}
esp_err_t passport_autostart_select(const char *id)
{
    if(!id)return ESP_ERR_INVALID_ARG;
    size_t length=0;while(length<64 && id[length])length++;if(length==64)return ESP_ERR_INVALID_ARG;
    if(storage_blocked || !mutex || xSemaphoreTake(mutex,0)!=pdTRUE)return ESP_ERR_INVALID_STATE;
    esp_err_t err=ESP_ERR_INVALID_ARG;
    if(!id[0] || passport_autostart_target(&registry,id)<PASSPORT_SLOTS) {
        err=passport_autostart_save(id);
        /* A commit error can still have persisted; keep RAM in step with NVS. */
        char stored[64];esp_err_t read=passport_autostart_load(stored);
        if(read==ESP_OK)memcpy(startup_id,stored,sizeof(startup_id));
        else {storage_blocked=true;if(err==ESP_OK)err=read;}
    }
    xSemaphoreGive(mutex);return err;
}

bool passport_autostart_ready(bool cold_boot)
{
    if (storage_blocked || !mutex || xSemaphoreTake(mutex, 0) != pdTRUE) return false;
    next = registry;
    next.last_slot=passport_autostart_target(&registry,startup_id);
    bool legacy = false, ready = false;
    if (next.last_slot < PASSPORT_SLOTS && passport_compat_mode(&next, next.last_slot, &legacy) == ESP_OK) {
        /* Community binaries cannot acknowledge SDK health. They may auto-start
         * only on clean power-on, after the cancellable countdown. All other
         * reset causes still open the launcher. Native SDK health stays intact. */
        if (legacy) next.launch_pending = 0;
        ready = passport_should_autostart(&next, cold_boot);
    }
    xSemaphoreGive(mutex);
    return ready;
}

esp_err_t passport_autostart_toggle(void)
{
    if (storage_blocked || xSemaphoreTake(mutex, 0) != pdTRUE) return ESP_ERR_INVALID_STATE;
    next = registry;
    next.autostart = !next.autostart;
    esp_err_t err = publish(&next);
    xSemaphoreGive(mutex);
    return err;
}

esp_err_t passport_launch(unsigned slot)
{
    if (storage_blocked || slot >= PASSPORT_SLOTS || xSemaphoreTake(mutex, 0) != pdTRUE) return ESP_ERR_INVALID_ARG;
    esp_err_t err = ESP_ERR_INVALID_STATE;
    if (registry.slots[slot].state != PASSPORT_READY) goto done;
    const esp_partition_t *app = app_partition(slot);
    if (!app) goto done;
    /* Recheck exact installed bytes as well as SDK's full-image validation. */
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    uint8_t *buffer = malloc(4096), digest[32];
    if (!buffer) { mbedtls_sha256_free(&sha); err = ESP_ERR_NO_MEM; goto done; }
    err = mbedtls_sha256_starts(&sha, 0) == 0 ? ESP_OK : ESP_FAIL;
    size_t size = registry.slots[slot].package.image_size;
    for (size_t offset = 0; err == ESP_OK && offset < size; offset += 4096) {
        size_t n = size - offset < 4096 ? size - offset : 4096;
        err = esp_partition_read(app, offset, buffer, n);
        if (err == ESP_OK && mbedtls_sha256_update(&sha, buffer, n) != 0) err = ESP_FAIL;
    }
    if (err == ESP_OK && (mbedtls_sha256_finish(&sha, digest) != 0 ||
        memcmp(digest, registry.slots[slot].package.sha256, 32))) err = ESP_ERR_OTA_VALIDATE_FAILED;
    free(buffer);
    mbedtls_sha256_free(&sha);
    if (err != ESP_OK) goto done;
    bool legacy = false;
    err = passport_compat_mode(&registry, slot, &legacy);
    if (err != ESP_OK) goto done;
    if (!legacy) {
        err = check_app_declaration(app, &registry.slots[slot].package);
        if (err != ESP_OK) goto done; /* Lost legacy mode must fail closed. */
    }
    next = registry;
    next.last_slot = slot;
    next.launch_pending = 1;
    err = publish(&next);
    if (err != ESP_OK) goto done;

    passport_layout_t layout;
    bool private_layout=passport_package_layout(&registry.slots[slot].package,&layout);
    if(private_layout) {
        passport_package_t check;
        err=esp_flash_read(NULL,&check,registry.slots[slot].data_offset+PASSPORT_DATA_SIZE,sizeof(check));
        if(err!=ESP_OK || memcmp(&check.reserved,&registry.slots[slot].package.reserved,sizeof(check.reserved)) ||
           check.image_size!=registry.slots[slot].package.image_size || memcmp(check.sha256,registry.slots[slot].package.sha256,32)) {
            err=ESP_ERR_INVALID_CRC;goto done;
        }
    }
    if (legacy && !private_layout) {
        err = passport_compat_begin(&registry, slot);
        if (err != ESP_OK) goto done;
    }
    passport_ticket_t ticket = passport_make_launch_ticket(slot,&registry.slots[slot]);
    memcpy(bootloader_common_get_rtc_retain_mem()->custom, &ticket, sizeof(ticket));
    esp_restart();
done:
    if (err != ESP_OK) {
        esp_err_t recovery = passport_compat_recover(&registry);
        if (recovery != ESP_OK) { storage_blocked = true; err = recovery; }
    }
    xSemaphoreGive(mutex);
    return err;
}
