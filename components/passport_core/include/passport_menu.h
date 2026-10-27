#pragma once
#include "passport_model.h"

#define PASSPORT_MENU_MAX_ITEMS (PASSPORT_SLOTS + 4u)

typedef enum { MENU_LIBRARY, MENU_MANAGE, MENU_INSTALL, MENU_AUTOSTART, MENU_CONFIRM, MENU_ABOUT, MENU_AUTO_PICK } passport_page_t;
typedef enum { ITEM_ROM, ITEM_INSTALL, ITEM_MANAGE, ITEM_AUTOSTART, ITEM_ABOUT, ITEM_BACK, ITEM_TOGGLE, ITEM_CANCEL, ITEM_UNINSTALL, ITEM_CHOOSE_AUTO, ITEM_USE_LAST, ITEM_AUTO_ROM } passport_item_kind_t;
typedef enum { MENU_UP, MENU_DOWN, MENU_OK, MENU_BACK } passport_menu_key_t;
typedef enum { MENU_NONE, MENU_LAUNCH, MENU_START_WEB, MENU_STOP_WEB, MENU_TOGGLE_AUTO, MENU_DELETE, MENU_SCROLL_UP, MENU_SCROLL_DOWN, MENU_SET_AUTO } passport_menu_action_t;
typedef struct { passport_item_kind_t kind; unsigned slot; } passport_menu_item_t;
typedef struct {
    passport_page_t page;
    unsigned selected, confirm_slot;
    char confirm_id[64], auto_id[64];
} passport_menu_t;
typedef struct { passport_menu_action_t action; unsigned slot; char id[64]; } passport_menu_command_t;

void passport_menu_init(passport_menu_t *menu);
unsigned passport_menu_items(const passport_menu_t *menu, const passport_registry_t *r, passport_menu_item_t items[PASSPORT_MENU_MAX_ITEMS]);
/* Returns false if a confirmation was invalidated by changed ownership. */
bool passport_menu_refresh(passport_menu_t *menu, const passport_registry_t *r);
passport_menu_command_t passport_menu_input(passport_menu_t *menu, const passport_registry_t *r, passport_menu_key_t key);
void passport_menu_uninstalled(passport_menu_t *menu, const passport_registry_t *r);
void passport_menu_web_closed(passport_menu_t *menu, const passport_registry_t *r);
