#pragma once
#include "passport_sdk.h"
esp_err_t passport_autostart_load(char id[64]);
esp_err_t passport_autostart_save(const char *id);
