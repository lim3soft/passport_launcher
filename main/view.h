#pragma once
#include "passport_menu.h"

/* Caller holds the BSP LVGL lock. Uses the full 240 x 320 display. */
void passport_view_init(const char *version);
void passport_view_render(const passport_menu_t *menu, const passport_registry_t *registry,
                          const char *notice, bool counting, int seconds, int battery, bool auto_ready);
void passport_view_error(const char *title, const char *message, const char *detail);
void passport_view_scroll(int direction);
