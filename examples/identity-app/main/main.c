#include "passport_sdk.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdio.h>

#ifndef PASSPORT_TEST_ID
#define PASSPORT_TEST_ID "org.passportos.counter-a"
#endif
PASSPORT_DECLARE_APP(PASSPORT_TEST_ID);
static QueueHandle_t keys;

static void on_key(bsp_btn_t key, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (key == BSP_BTN_OK && (event == BSP_BTN_CLICK || event == BSP_BTN_LONG))
        xQueueSend(keys, &event, 0);
}

void app_main(void)
{
    nvs_handle_t handle;
    ESP_ERROR_CHECK(passport_app_open(PASSPORT_APP_ID, "counter", NVS_READWRITE, &handle));
    uint32_t count = 0;
    esp_err_t err = nvs_get_u32(handle, "value", &count);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(bsp_display_init());
    if (!bsp_lvgl_init() || !bsp_lvgl_lock(1000)) return;
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x172732), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xffffff), 0);
    lv_obj_t *label = lv_label_create(screen);
    lv_obj_set_width(label, 192);
    lv_obj_center(label);
    lv_screen_load(screen);
    bsp_lvgl_unlock();
    bsp_display_backlight(65);
    keys = xQueueCreate(8, sizeof(bsp_btn_ev_t));
    if (!keys) return;
    ESP_ERROR_CHECK(bsp_button_init(on_key, NULL));
    ESP_ERROR_CHECK(passport_app_mark_healthy(PASSPORT_APP_ID));
    while (true) {
        if (bsp_lvgl_lock(1000)) {
            char text[220];
            snprintf(text, sizeof(text), "PRIVATE COUNTER\n\n%s\n\nSaved value: %lu\n\nOK: add and save\nHold OK: launcher", PASSPORT_APP_ID, (unsigned long)count);
            lv_label_set_text(label, text);
            bsp_lvgl_unlock();
        }
        bsp_btn_ev_t event;
        if (xQueueReceive(keys, &event, portMAX_DELAY) != pdTRUE) continue;
        if (event == BSP_BTN_LONG) { nvs_close(handle); passport_return_to_launcher(); }
        else {
            ++count;
            ESP_ERROR_CHECK(nvs_set_u32(handle, "value", count));
            ESP_ERROR_CHECK(nvs_commit(handle));
        }
    }
}
