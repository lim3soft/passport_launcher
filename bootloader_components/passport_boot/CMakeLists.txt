idf_component_register(SRCS "hooks.c" "../../components/passport_core/passport_model.c"
    INCLUDE_DIRS "../../components/passport_core/include" REQUIRES bootloader_support spi_flash esp_rom)
