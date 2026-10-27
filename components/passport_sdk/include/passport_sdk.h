#pragma once
#include "esp_err.h"
#include "nvs.h"
#include "passport_model.h"

/* This declaration is a producer compatibility claim, not a signature. */
#define PASSPORT_DECLARE_APP(id) \
    static const char passport_app_manifest[] __attribute__((used, retain)) = "PASSPORTOS_ABI2:" id
#define PASSPORT_APP_ID (passport_app_manifest + 16)

/* Launcher and apps use the same metadata blob ABI. Never erase on init error. */
esp_err_t passport_registry_load(passport_registry_t *registry);
esp_err_t passport_registry_save(const passport_registry_t *registry);
esp_err_t passport_app_open(const char *expected_id, const char *namespace_name,
                            nvs_open_mode_t mode, nvs_handle_t *handle);
/* Call only after essential initialization succeeds. */
esp_err_t passport_app_mark_healthy(const char *expected_id);
/* Stop and save app work before calling. All resets otherwise return to menu too. */
void passport_return_to_launcher(void);
