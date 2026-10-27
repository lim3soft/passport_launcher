#pragma once
#include "passport_sdk.h"
/* Factory boots expose no app pool partition. Register only validated extents. */
esp_err_t passport_storage_sync(const passport_registry_t *registry);
