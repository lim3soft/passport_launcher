idf_component_register(SRCS "main.c" INCLUDE_DIRS "." REQUIRES bsp passport_sdk esp_timer)
if(DEFINED PASSPORT_TEST_ID)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PASSPORT_TEST_ID="${PASSPORT_TEST_ID}")
endif()
