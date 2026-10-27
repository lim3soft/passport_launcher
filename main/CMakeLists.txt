set(passport_licenses "${CMAKE_CURRENT_BINARY_DIR}/license_bundle.txt.gz")
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
idf_build_get_property(passport_python PYTHON)
execute_process(COMMAND "${passport_python}" "${CMAKE_SOURCE_DIR}/tools/build_license_bundle.py"
    "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/delivery/THIRD-PARTY-NOTICES.txt" "${passport_licenses}"
    RESULT_VARIABLE passport_license_result)
if(NOT passport_license_result EQUAL 0)
    message(FATAL_ERROR "Cannot retain the complete license notices")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/delivery/THIRD-PARTY-NOTICES.txt"
    "${CMAKE_SOURCE_DIR}/tools/build_license_bundle.py")
endif()
idf_component_register(SRCS "main.c" "manager.c" "web.c" "compat.c" "view.c" "storage.c" "autostart.c" "font_brand_22.c"
    INCLUDE_DIRS "." REQUIRES bsp passport_sdk esp_http_server esp_wifi esp_netif
    esp_event mbedtls json esp_timer esp_app_format spi_flash
    EMBED_TXTFILES "bin_import.js"
    EMBED_FILES "${passport_licenses}")
