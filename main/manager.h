#pragma once
#include "passport_sdk.h"
#include "esp_http_server.h"

esp_err_t passport_manager_init(void);
bool passport_snapshot(passport_registry_t *out);
bool passport_manager_busy(void);
esp_err_t passport_install_http(httpd_req_t *req);
esp_err_t passport_preflight_http(httpd_req_t *req);
typedef enum { IMPORT_IDLE, IMPORT_READY, IMPORT_TOO_LARGE, IMPORT_NO_SPACE,
    IMPORT_LAYOUT, IMPORT_INVALID, IMPORT_WRITING, IMPORT_DONE, IMPORT_FAILED } passport_import_notice_t;
void passport_import_note(passport_import_notice_t notice, unsigned bytes);
bool passport_import_message(char *out, size_t size);
esp_err_t passport_qso_import_http(httpd_req_t *req, unsigned slot);
void passport_install_enable(bool enabled);
esp_err_t passport_delete(unsigned slot, bool erase_private_data);
esp_err_t passport_uninstall(unsigned slot, const char *expected_id);
esp_err_t passport_autostart_toggle(void);
esp_err_t passport_autostart_select(const char *id);
bool passport_autostart_configuration(char id[64]);
unsigned passport_autostart_boot_slot(void);
bool passport_autostart_ready(bool cold_boot);
esp_err_t passport_launch(unsigned slot);
esp_err_t passport_web_start(void);
esp_err_t passport_web_stop(void);
const char *passport_ap_password(void);
esp_err_t passport_prepare_web(void);
