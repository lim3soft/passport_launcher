#include "manager.h"
#include "view.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "passport_startup.h"
#include <stdio.h>
#include <string.h>

static QueueHandle_t keys;
typedef struct { bsp_btn_t key; bsp_btn_ev_t event; int64_t at_us; } passport_key_t;
static passport_registry_t snapshot;
static passport_menu_t menu;
static char notice[96];
static int battery=-1;
static bool auto_ready;

static void on_key(bsp_btn_t key,bsp_btn_ev_t event,void *user)
{
    (void)user;
    passport_key_t input={.key=key,.event=event,.at_us=esp_timer_get_time()};
    if(keys)xQueueSend(keys,&input,0);
}

static void result(esp_err_t err,const char *success)
{
    if(err==ESP_OK)snprintf(notice,sizeof(notice),"%s",success);
    else snprintf(notice,sizeof(notice),"Failed: %s",esp_err_to_name(err));
}

static void paint(bool counting,int seconds,const char *message)
{
    if(!bsp_lvgl_lock(1000))return;
    passport_view_render(&menu,&snapshot,message,counting,seconds,battery,auto_ready);
    bsp_lvgl_unlock();
}

static void refresh(void)
{
    if(passport_snapshot(&snapshot)) {
        passport_autostart_configuration(menu.auto_id);
        auto_ready=passport_autostart_ready(true);
        if(!passport_menu_refresh(&menu,&snapshot))snprintf(notice,sizeof(notice),"App changed. Select it again.");
    }
}

void app_main(void)
{
    esp_err_t storage=passport_manager_init();
    bsp_i2c_init();
    if(bsp_display_init()!=ESP_OK||!bsp_lvgl_init()) {
        ESP_LOGE("launcher","Display unavailable; recover over USB");return;
    }
    bsp_battery_init();
    if(!bsp_lvgl_lock(1000))return;
    passport_view_init(esp_app_get_description()->version);
    bsp_lvgl_unlock();
    bsp_display_backlight(65);
    if(storage!=ESP_OK) {
        if(bsp_lvgl_lock(1000)) {
            passport_view_error("Storage recovery","Storage needs attention.\nNothing was erased.\n\nRestart or use USB\ndiagnostics.",esp_err_to_name(storage));
            bsp_lvgl_unlock();
        }
        return;
    }
    keys=xQueueCreate(16,sizeof(passport_key_t));
    if(!keys||bsp_button_init(on_key,NULL)!=ESP_OK) {
        if(bsp_lvgl_lock(1000)) {
            passport_view_error("Button recovery","Buttons are unavailable.\n\nRestart or use USB\ndiagnostics.","No data was erased");
            bsp_lvgl_unlock();
        }
        return;
    }
    passport_snapshot(&snapshot);passport_menu_init(&menu);
    passport_autostart_configuration(menu.auto_id);
    passport_menu_item_t initial[PASSPORT_MENU_MAX_ITEMS];
    unsigned count=passport_menu_items(&menu,&snapshot,initial);
    for(unsigned i=0;i<count;i++)if(initial[i].kind==ITEM_ROM&&initial[i].slot==snapshot.last_slot){menu.selected=i;break;}
    auto_ready=passport_autostart_ready(true);
    passport_startup_t startup;
    passport_startup_init(&startup,passport_autostart_ready(esp_reset_reason()==ESP_RST_POWERON),esp_timer_get_time());
    int64_t battery_due=0;
    while(true) {
        refresh();
        if(startup.armed) {
            passport_startup_action_t action=passport_startup_poll(&startup,esp_timer_get_time(),uxQueueMessagesWaiting(keys)!=0);
            if(action==PASSPORT_STARTUP_CANCEL)snprintf(notice,sizeof(notice),"Auto start cancelled");
            else if(action==PASSPORT_STARTUP_LAUNCH) {
                paint(true,0,"");result(passport_launch(passport_autostart_boot_slot()),"Opening app");
            }
        }
        if(esp_timer_get_time()>=battery_due) {
            battery=bsp_battery_soc();battery_due=esp_timer_get_time()+10000000;
        }
        int seconds=startup.armed?(int)((startup.deadline_us-esp_timer_get_time()+999999)/1000000):0;
        const char *message=notice;
        char transfer[96];
        if(menu.page==MENU_INSTALL) {
            if(passport_import_message(transfer,sizeof(transfer)))message=transfer;
            else if(passport_manager_busy())message="Installing... keep power on";
        }
        paint(startup.armed,seconds,message);
        passport_key_t input;
        if(xQueueReceive(keys,&input,pdMS_TO_TICKS(200))!=pdTRUE)continue;
        if(passport_startup_consume_input(&startup,input.at_us))continue;
        if(input.event!=BSP_BTN_CLICK&&!(input.key==BSP_BTN_OK&&input.event==BSP_BTN_LONG))continue;
        notice[0]=0;
        passport_menu_key_t key;
        if(input.key==BSP_BTN_OK)key=input.event==BSP_BTN_LONG?MENU_BACK:MENU_OK;
        else key=input.key==BSP_BTN_UP?MENU_UP:MENU_DOWN;
        refresh();
        passport_menu_command_t command=passport_menu_input(&menu,&snapshot,key);
        esp_err_t err;
        switch(command.action) {
        case MENU_LAUNCH:
            paint(false,0,"Opening app...");result(passport_launch(command.slot),"Opening app");break;
        case MENU_START_WEB:
            err=passport_web_start();
            if(err!=ESP_OK){passport_menu_web_closed(&menu,&snapshot);result(err,"");}
            break;
        case MENU_STOP_WEB:
            err=passport_web_stop();result(err,"");
            if(err==ESP_OK){refresh();passport_menu_web_closed(&menu,&snapshot);}
            break;
        case MENU_TOGGLE_AUTO:
            err=passport_autostart_toggle();refresh();
            result(err,snapshot.autostart?"Auto start is On":"Auto start is Off");break;
        case MENU_SET_AUTO:
            err=passport_autostart_select(command.id);refresh();
            result(err,command.id[0]?"Startup app selected":"Following last booted app");break;
        case MENU_DELETE:
            paint(false,0,"Uninstalling... keep power on");
            err=passport_uninstall(command.slot,command.id);refresh();
            passport_menu_uninstalled(&menu,&snapshot);
            result(err,"App and saved data erased");break;
        case MENU_SCROLL_UP:case MENU_SCROLL_DOWN:
            if(bsp_lvgl_lock(1000)){passport_view_scroll(command.action==MENU_SCROLL_DOWN?1:-1);bsp_lvgl_unlock();}
            break;
        case MENU_NONE:break;
        }
    }
}
