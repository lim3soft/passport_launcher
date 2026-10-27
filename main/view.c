#include "view.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(passport_brand_22);

#define INK 0x0c151d
#define PANEL 0x192a35
#define WHITE 0xf3f7fa
#define MUTED 0x9bb2c0
#define GREEN 0xc3f77a
#define RED 0xffa0a0
#define BODY_Y 76
#define BODY_H 204
static lv_obj_t *screen, *brand, *heading, *caption, *level, *content, *footer;
static lv_obj_t *rows[PASSPORT_MENU_MAX_ITEMS], *names[PASSPORT_MENU_MAX_ITEMS], *details[PASSPORT_MENU_MAX_ITEMS];
static passport_item_kind_t kinds[PASSPORT_MENU_MAX_ITEMS];
static lv_obj_t *count_text, *count_bar;
typedef struct {char identity[128];uint32_t state,size,capacity,image,data;} view_record_t;
static view_record_t shown_records[PASSPORT_SLOTS];
static uint32_t shown_last,shown_autostart,shown_pending;
static bool changed(const passport_registry_t *r)
{
    bool dirty=shown_last!=r->last_slot || shown_autostart!=r->autostart || shown_pending!=r->launch_pending;
    shown_last=r->last_slot;shown_autostart=r->autostart;shown_pending=r->launch_pending;
    for(unsigned i=0;i<PASSPORT_SLOTS;i++) {
        const passport_record_t *a=&r->slots[i];
        view_record_t key={.state=a->state,.size=a->package.image_size,.capacity=a->image_capacity,.image=a->image_offset,.data=a->data_offset};
        memcpy(key.identity,a->package.app_id,sizeof(key.identity));
        if(memcmp(&key,&shown_records[i],sizeof(key))){dirty=true;shown_records[i]=key;}
    }
    return dirty;
}
static unsigned row_count;
static int shown_page=-1, focus=-1, shown_seconds=-1;
static bool shown_auto_ready;
static char firmware_version[32], shown_confirm[64], shown_auto_id[64];

static void set_text(lv_obj_t *label,const char *text)
{ if(strcmp(lv_label_get_text(label),text))lv_label_set_text(label,text); }

static lv_obj_t *label(lv_obj_t *parent,const char *text,const lv_font_t *font,int x,int y,int width)
{
    lv_obj_t *obj=lv_label_create(parent);
    lv_obj_set_pos(obj,x,y);lv_obj_set_width(obj,width);
    lv_obj_set_style_text_font(obj,font,0);
    lv_obj_set_style_text_color(obj,lv_color_hex(WHITE),0);
    lv_obj_set_style_base_dir(obj,LV_BASE_DIR_LTR,0);
    lv_label_set_text(obj,text);return obj;
}

static lv_obj_t *card(int height)
{
    lv_obj_t *obj=lv_obj_create(content);lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj,220,height);
    lv_obj_set_style_bg_color(obj,lv_color_hex(PANEL),0);
    lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
    lv_obj_set_style_radius(obj,16,0);
    lv_obj_set_scrollable(obj,false);
    return obj;
}

static void row(const char *name,const char *detail,passport_item_kind_t kind)
{
    unsigned i=row_count++;
    rows[i]=card(kind==ITEM_ROM?52:44);kinds[i]=kind;
    names[i]=label(rows[i],name,&lv_font_montserrat_16,12,7,196);
    lv_obj_set_height(names[i],21);lv_label_set_long_mode(names[i],LV_LABEL_LONG_MODE_DOTS);
    details[i]=label(rows[i],detail,&lv_font_montserrat_12,12,kind==ITEM_ROM?32:27,196);
    lv_obj_set_height(details[i],15);lv_label_set_long_mode(details[i],LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_color(details[i],lv_color_hex(MUTED),0);
}

static void focus_row(unsigned selected,bool rebuilding)
{
    if(!row_count||(int)selected==focus)return;
    for(unsigned i=0;i<row_count;i++) {
        bool active=i==selected;
        uint32_t color=kinds[i]==ITEM_UNINSTALL?RED:GREEN;
        lv_obj_set_style_bg_color(rows[i],lv_color_hex(active?color:PANEL),0);
        lv_obj_set_style_text_color(names[i],lv_color_hex(active?INK:WHITE),0);
        lv_obj_set_style_text_color(details[i],lv_color_hex(active?0x314323:MUTED),0);
        /* Change mode only on focus changes. Re-setting text or mode on every
         * 200 ms UI poll would continually restart the marquee. */
        lv_label_set_long_mode(names[i],active?LV_LABEL_LONG_MODE_SCROLL_CIRCULAR:LV_LABEL_LONG_MODE_DOTS);
    }
    focus=(int)selected;
    lv_obj_update_layout(content);
    lv_obj_scroll_to_view(rows[selected],rebuilding?LV_ANIM_OFF:LV_ANIM_ON);
}

static void empty_card(const char *title,const char *message)
{
    lv_obj_t *obj=card(88);
    label(obj,title,&lv_font_montserrat_16,12,12,196);
    lv_obj_t *text=label(obj,message,&lv_font_montserrat_12,12,39,196);
    lv_obj_set_style_text_color(text,lv_color_hex(MUTED),0);
}

static const char *startup_name(const passport_menu_t *m,const passport_registry_t *r)
{
    unsigned slot=passport_autostart_target(r,m->auto_id);
    return slot<PASSPORT_SLOTS?r->slots[slot].package.name:m->auto_id[0]?"Selected app unavailable":"No app launched yet";
}
static void separator(void)
{
    lv_obj_t *space=lv_obj_create(content);lv_obj_remove_style_all(space);lv_obj_set_size(space,220,12);lv_obj_set_scrollable(space,false);
    lv_obj_t *line=lv_obj_create(space);lv_obj_remove_style_all(line);lv_obj_set_pos(line,0,5);lv_obj_set_size(line,220,1);
    lv_obj_set_style_bg_color(line,lv_color_hex(0x344b59),0);lv_obj_set_style_bg_opa(line,LV_OPA_COVER,0);
}

static void app_card(const char *name,const char *note,bool highlight)
{
    lv_obj_t *obj=card(56);
    if(highlight)lv_obj_set_style_bg_color(obj,lv_color_hex(GREEN),0);
    lv_obj_t *small=label(obj,note,&lv_font_montserrat_12,12,7,196);
    lv_obj_t *title=label(obj,name,&lv_font_montserrat_16,12,28,196);
    lv_obj_set_height(title,21);lv_label_set_long_mode(title,LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_style_text_color(small,lv_color_hex(highlight?0x314323:MUTED),0);
    lv_obj_set_style_text_color(title,lv_color_hex(highlight?INK:WHITE),0);
}

static void build_library(const passport_menu_t *m,const passport_registry_t *r)
{
    unsigned installed=0;
    for(unsigned i=0;i<PASSPORT_SLOTS;i++){installed+=r->slots[i].state==PASSPORT_READY;}
    set_text(heading,m->page==MENU_MANAGE?"Manage apps":"Passport Launcher");
    passport_capacity_t c;passport_capacity(r,&c);
    char summary[64];snprintf(summary,sizeof(summary),"%u installed / %u KiB free",installed,(unsigned)(c.free_bytes/1024));
    set_text(caption,m->page==MENU_MANAGE?"Choose an app to uninstall":summary);
    passport_menu_item_t items[PASSPORT_MENU_MAX_ITEMS];unsigned n=passport_menu_items(m,r,items);
    if(items[0].kind!=ITEM_ROM)empty_card(m->page==MENU_MANAGE?"Nothing to uninstall":"No apps installed",
        m->page==MENU_MANAGE?"Your library is empty.":"Choose Install a ROM below\nto add your first app.");
    for(unsigned i=0;i<n;i++) {
        passport_item_kind_t kind=items[i].kind;
        if(kind==ITEM_INSTALL && installed)separator();
        if(kind==ITEM_ROM) {
            const passport_record_t *a=&r->slots[items[i].slot];
            const char *detail=m->page==MENU_LIBRARY?a->package.version:
                a->state==PASSPORT_READY?"App and saved data":
                a->state==PASSPORT_INSTALLING?"Install interrupted":"Finish uninstall to free space";
            row(a->package.name,detail,kind);
        } else if(kind==ITEM_INSTALL)row("Install a ROM","Upload a .bin over Wi-Fi",kind);
        else if(kind==ITEM_MANAGE)row("Manage apps","Uninstall apps and saved data",kind);
        else if(kind==ITEM_AUTOSTART) {
            char title[40];snprintf(title,sizeof(title),"Auto start: %s",r->autostart?"On":"Off");
            row(title,r->autostart?startup_name(m,r):"Choose what starts at power-on",kind);
        } else if(kind==ITEM_ABOUT)row("About","Version, licenses and project",kind);
        else row("Back to library","Your installed ROMs",kind);
    }
}

static void build_install(const passport_registry_t *r)
{
    passport_capacity_t c;passport_capacity(r,&c);
    char capacity[64];snprintf(capacity,sizeof(capacity),"%u KiB free | ROM %u KiB",(unsigned)(c.free_bytes/1024),(unsigned)(c.largest_image_bytes/1024));
    set_text(heading,"Install a ROM");set_text(caption,capacity);
    lv_obj_t *obj=card(98);
    lv_obj_t *hint=label(obj,"WI-FI NETWORK",&lv_font_montserrat_12,12,10,196);
    lv_obj_set_style_text_color(hint,lv_color_hex(MUTED),0);
    label(obj,"Passport Launcher",&lv_font_montserrat_16,12,31,196);
    label(obj,"Password: aipassport",&lv_font_montserrat_14,12,64,196);
    obj=card(94);hint=label(obj,"OPEN IN YOUR BROWSER",&lv_font_montserrat_12,12,10,196);
    lv_obj_set_style_text_color(hint,lv_color_hex(MUTED),0);
    label(obj,"192.168.168.1",&lv_font_montserrat_20,12,32,196);
    label(obj,"Choose a .bin, then Install",&lv_font_montserrat_12,12,68,196);
}

static void build_auto(const passport_menu_t *m,const passport_registry_t *r,bool ready)
{
    set_text(heading,"Auto start");set_text(caption,"Choose what starts at power-on");
    lv_obj_t *obj=card(46);
    const char *note=!r->autostart?"Off: open your library first.\nOn: launch after 5 seconds.":
        ready?"5-second startup countdown.\nPress any key to cancel.":
        passport_autostart_target(r,m->auto_id)>=PASSPORT_SLOTS?"Choose an installed app or\nlaunch one to use last booted.":"Waiting for the app to report\na successful start.";
    label(obj,note,&lv_font_montserrat_12,12,9,196);
    app_card(startup_name(m,r),m->auto_id[0]?"SELECTED STARTUP APP":"LAST BOOTED APP",r->autostart!=0);
    row(r->autostart?"Auto start: On":"Auto start: Off","OK switches On / Off",ITEM_TOGGLE);
    row("Startup app",m->auto_id[0]?startup_name(m,r):"Last booted app (default)",ITEM_CHOOSE_AUTO);
    row("Back to library","Keep this setting",ITEM_BACK);
}

static void build_auto_picker(const passport_menu_t *m,const passport_registry_t *r)
{
    set_text(heading,"Startup app");set_text(caption,"OK selects; hold OK cancels");
    passport_menu_item_t items[PASSPORT_MENU_MAX_ITEMS];unsigned n=passport_menu_items(m,r,items);
    for(unsigned i=0;i<n;i++) {
        passport_item_kind_t kind=items[i].kind;
        if(kind==ITEM_USE_LAST)row("Last booted app",m->auto_id[0]?"Follow the app you launch":"Selected - default",kind);
        else if(kind==ITEM_AUTO_ROM) {
            const passport_record_t *a=&r->slots[items[i].slot];
            row(a->package.name,!strcmp(m->auto_id,a->package.app_id)?"Selected startup app":"Always start this app",kind);
        } else row("Back","Keep current selection",ITEM_BACK);
    }
}

static void build_confirm(const passport_menu_t *m,const passport_registry_t *r)
{
    set_text(heading,"Uninstall app?");set_text(caption,"This cannot be undone");
    app_card(r->slots[m->confirm_slot].package.name,"SELECTED APP",false);
    lv_obj_t *obj=card(42);
    lv_obj_t *warning=label(obj,"All saved data and settings\nwill be permanently erased.",&lv_font_montserrat_12,12,7,196);
    lv_obj_set_style_text_color(warning,lv_color_hex(RED),0);
    row("Cancel","Keep app and saved data",ITEM_CANCEL);
    row("Uninstall","Erase app and all saved data",ITEM_UNINSTALL);
}

static void build_countdown(const passport_menu_t *m,const passport_registry_t *r)
{
    set_text(heading,"Starting your app");set_text(caption,"Any key opens your library");
    app_card(startup_name(m,r),m->auto_id[0]?"SELECTED STARTUP APP":"LAST BOOTED APP",true);
    lv_obj_t *obj=card(128);
    count_text=label(obj,"5",&lv_font_montserrat_28,12,10,196);
    lv_obj_set_style_text_align(count_text,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_t *hint=label(obj,"seconds to launch",&lv_font_montserrat_12,12,52,196);
    lv_obj_set_style_text_align(hint,LV_TEXT_ALIGN_CENTER,0);
    label(obj,"Press any key to cancel",&lv_font_montserrat_12,12,79,196);
    count_bar=lv_bar_create(obj);lv_obj_remove_style_all(count_bar);
    lv_obj_set_pos(count_bar,12,106);lv_obj_set_size(count_bar,196,7);
    lv_obj_set_style_bg_color(count_bar,lv_color_hex(0x344b59),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(count_bar,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_radius(count_bar,4,LV_PART_MAIN);
    lv_obj_set_style_bg_color(count_bar,lv_color_hex(GREEN),LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(count_bar,LV_OPA_COVER,LV_PART_INDICATOR);
    lv_obj_set_style_radius(count_bar,4,LV_PART_INDICATOR);
    lv_bar_set_range(count_bar,0,5);
}

static void build_about(void)
{
    set_text(heading,"About");set_text(caption,"Passport Launcher");
    lv_obj_t *obj=card(86);
    lv_obj_t *hint=label(obj,"Developed by",&lv_font_montserrat_12,12,10,196);
    lv_obj_set_style_text_color(hint,lv_color_hex(MUTED),0);
    hint=label(obj,"9W3SPY",&lv_font_montserrat_28,12,34,196);
    lv_obj_set_style_text_color(hint,lv_color_hex(GREEN),0);
    obj=card(78);char build[96];snprintf(build,sizeof(build),"Version %s\nESP-IDF 5.5.3\nESP32-C3 / 8 MB",firmware_version);
    label(obj,build,&lv_font_montserrat_14,12,12,196);
    obj=card(236);
    label(obj,"OPEN SOURCE",&lv_font_montserrat_12,12,10,196);
    label(obj,"App license: MIT\nUse, modify and redistribute.\nKeep copyright/license notices.\nProvided without warranty.\n\nFoloToy board support: MIT\nESP-IDF: Apache-2.0\nLVGL: MIT\nOther component licenses apply.\nFull notices: web portal\nAbout > Open-source licenses",&lv_font_montserrat_12,12,34,196);
    obj=card(92);label(obj,"Inspired by",&lv_font_montserrat_12,12,10,196);
    hint=label(obj,"M5 Launcher",&lv_font_montserrat_20,12,31,196);lv_obj_set_style_text_color(hint,lv_color_hex(GREEN),0);
    label(obj,"bmorcelli/Launcher",&lv_font_montserrat_12,12,63,196);
}

void passport_view_init(const char *version)
{
    snprintf(firmware_version,sizeof(firmware_version),"%s",version);
    shown_page=-1;focus=-1;
    screen=lv_obj_create(NULL);lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen,240,320);lv_obj_set_scrollable(screen,false);
    lv_obj_set_style_bg_color(screen,lv_color_hex(INK),0);lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    brand=label(screen,"Passport Launcher",&lv_font_montserrat_12,12,8,174);
    lv_obj_set_style_text_color(brand,lv_color_hex(MUTED),0);
    level=label(screen,"--%",&lv_font_montserrat_12,190,8,38);lv_obj_set_style_text_align(level,LV_TEXT_ALIGN_RIGHT,0);
    heading=label(screen,"Passport Launcher",&lv_font_montserrat_20,12,29,216);
    caption=label(screen,"",&lv_font_montserrat_12,12,55,216);lv_obj_set_style_text_color(caption,lv_color_hex(MUTED),0);
    content=lv_obj_create(screen);lv_obj_remove_style_all(content);lv_obj_set_pos(content,0,BODY_Y);lv_obj_set_size(content,240,BODY_H);
    lv_obj_set_style_pad_left(content,10,0);lv_obj_set_style_pad_right(content,10,0);lv_obj_set_style_pad_row(content,6,0);
    lv_obj_set_flex_flow(content,LV_FLEX_FLOW_COLUMN);lv_obj_set_scroll_dir(content,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content,LV_SCROLLBAR_MODE_AUTO);lv_obj_set_style_width(content,2,LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(content,lv_color_hex(GREEN),LV_PART_SCROLLBAR);lv_obj_set_style_bg_opa(content,LV_OPA_50,LV_PART_SCROLLBAR);
    footer=label(screen,"",&lv_font_montserrat_12,12,290,216);lv_obj_set_style_text_color(footer,lv_color_hex(MUTED),0);
    lv_obj_set_height(footer,28);lv_label_set_long_mode(footer,LV_LABEL_LONG_MODE_WRAP);
    lv_screen_load(screen);
}

void passport_view_render(const passport_menu_t *m,const passport_registry_t *r,const char *notice,
                          bool counting,int seconds,int battery,bool auto_ready)
{
    int page=counting?100:(int)m->page;
    bool registry_changed=changed(r);
    bool rebuilding=page!=shown_page||registry_changed||strcmp(shown_auto_id,m->auto_id)||
        (m->page==MENU_CONFIRM&&strcmp(shown_confirm,m->confirm_id))||
        (m->page==MENU_AUTOSTART&&shown_auto_ready!=auto_ready);
    if(rebuilding) {
        lv_obj_clean(content);row_count=0;focus=-1;count_text=count_bar=NULL;shown_seconds=-1;
        memset(rows,0,sizeof(rows));shown_page=page;shown_auto_ready=auto_ready;
        snprintf(shown_confirm,sizeof(shown_confirm),"%s",m->confirm_id);
        snprintf(shown_auto_id,sizeof(shown_auto_id),"%s",m->auto_id);
        bool library=!counting && m->page==MENU_LIBRARY;
        set_text(brand,library?"AI-PASSPORT":"Passport Launcher");
        lv_obj_set_style_text_font(heading,library?&passport_brand_22:&lv_font_montserrat_20,0);
        lv_obj_scroll_to_y(content,0,LV_ANIM_OFF);
        if(counting)build_countdown(m,r);
        else if(m->page==MENU_LIBRARY||m->page==MENU_MANAGE)build_library(m,r);
        else if(m->page==MENU_INSTALL)build_install(r);
        else if(m->page==MENU_AUTOSTART)build_auto(m,r,auto_ready);
        else if(m->page==MENU_AUTO_PICK)build_auto_picker(m,r);
        else if(m->page==MENU_CONFIRM)build_confirm(m,r);
        else build_about();
    }
    char text[64];
    if(battery<0)snprintf(text,sizeof(text),"--%%");else snprintf(text,sizeof(text),"%d%%",battery);
    set_text(level,text);
    if(counting) {
        if(seconds<0)seconds=0;else if(seconds>5)seconds=5;
        if(seconds!=shown_seconds) {
            snprintf(text,sizeof(text),"%d",seconds);set_text(count_text,text);
            lv_bar_set_value(count_bar,seconds,LV_ANIM_OFF);shown_seconds=seconds;
        }
        set_text(footer,"ANY KEY: cancel auto start");
    } else {
        focus_row(m->selected,rebuilding);
        const char *help=m->page==MENU_INSTALL?"HOLD OK: back to library":
            m->page==MENU_ABOUT?"UP/DOWN: scroll   HOLD OK: back":
            m->page==MENU_CONFIRM?"UP/DOWN: choose   OK: confirm":"UP/DOWN: select   OK: open";
        set_text(footer,notice[0]?notice:help);
    }
}

void passport_view_error(const char *title,const char *message,const char *detail)
{
    lv_obj_clean(content);set_text(heading,title);set_text(caption,"Restart or use USB diagnostics");
    lv_obj_t *obj=card(174);label(obj,message,&lv_font_montserrat_14,12,12,196);
    set_text(footer,detail);shown_page=-2;
}

void passport_view_scroll(int direction)
{ lv_obj_scroll_by_bounded(content,0,direction>0?-48:48,LV_ANIM_ON); }
