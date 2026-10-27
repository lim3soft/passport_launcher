idf_component_register(SRCS "passport_sdk.c" "passport_qso_migration.c" INCLUDE_DIRS "include"
    REQUIRES passport_core nvs_flash app_update bootloader_support esp_system mbedtls)
