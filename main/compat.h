#pragma once
#include "passport_sdk.h"

/* Must run before the launcher opens default NVS/Wi-Fi. No app is running. */
esp_err_t passport_compat_recover(const passport_registry_t *registry);
/* Restore an unmodified community app's default NVS before its one-shot boot. */
esp_err_t passport_compat_begin(const passport_registry_t *registry, unsigned slot);
esp_err_t passport_compat_mode(const passport_registry_t *registry, unsigned slot, bool *legacy);
esp_err_t passport_compat_set_mode(unsigned slot, const char *id, bool legacy);
