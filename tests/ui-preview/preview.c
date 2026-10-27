/* Render the production view with the actual pinned LVGL and a 48 KiB pool.
 * This is a native pixel/interaction check, not an AI-Passport screenshot. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../main/view.c"

static uint16_t pixels[240*320],buffer[240*40];
static size_t least_free=SIZE_MAX;
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *data)
{
    int w=lv_area_get_width(area),h=lv_area_get_height(area);
    uint32_t stride=lv_draw_buf_width_to_stride((uint32_t)w,LV_COLOR_FORMAT_RGB565);
    for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
        int dx=area->x1+x,dy=area->y1+y;
        assert(dx>=0&&dx<240&&dy>=0&&dy<320);
        memcpy(&pixels[dy*240+dx],data+(size_t)y*stride+x*2,2);
    }
    lv_display_flush_ready(display);
}
static void frames(unsigned n)
{
    for(unsigned i=0;i<n;i++){lv_tick_inc(40);lv_timer_handler();}
    lv_mem_monitor_t memory;lv_mem_monitor(&memory);
    if(memory.free_size<least_free)least_free=memory.free_size;
    assert(LV_USE_STDLIB_MALLOC==LV_STDLIB_BUILTIN&&LV_MEM_SIZE==48*1024);
    assert(memory.total_size>0&&memory.total_size<=48*1024&&memory.free_size>0);
}
static void save(const char *dir,const char *name)
{
    char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 320\n255\n");
    for(unsigned i=0;i<240*320;i++) {
        uint16_t p=pixels[i];uint8_t rgb[]={((p>>11)&31)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};
        assert(fwrite(rgb,1,3,f)==3);
    }
    fclose(f);
}
static void app(passport_registry_t *r,unsigned slot,const char *name)
{
    passport_package_t p={.format=1,.abi=PASSPORT_ABI,.image_size=1000};memcpy(p.magic,PASSPORT_MAGIC,8);
    snprintf(p.app_id,sizeof(p.app_id),"community.rom%u",slot);snprintf(p.name,sizeof(p.name),"%s",name);strcpy(p.version,"2.3.3");
    assert(passport_install_prepare(r,&p,slot)&&passport_install_finish(r,slot));
}
int main(int argc,char **argv)
{
    assert(argc==2);lv_init();
    lv_display_t *display=lv_display_create(240,320);assert(display);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    passport_view_init("0.4.1");passport_registry_t r;passport_registry_init(&r);
    passport_menu_t m;passport_menu_init(&m);
    passport_view_render(&m,&r,"",false,0,82,false);frames(8);save(argv[1],"empty");
    app(&r,0,"Internet Radio - Worldwide Mix");app(&r,1,"QSO Vision");app(&r,2,"Radio Tools");
    passport_view_render(&m,&r,"",false,0,82,false);frames(2);save(argv[1],"library");
    assert(!strcmp(lv_label_get_text(heading),"Passport Launcher"));
    assert(lv_obj_get_style_text_font(heading,0)==&passport_brand_22);
    for(unsigned i=0;i<row_count;i++)assert(!strstr(lv_label_get_text(details[i]),"9W3SPY"));
    assert(lv_obj_get_child_count(content)==row_count+1); /* non-focusable group separator */
    assert(lv_label_get_long_mode(names[0])==LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_t *same_label=names[0];uint16_t before[196*20];
    for(unsigned y=0;y<20;y++)memcpy(before+y*196,pixels+(BODY_Y+7+y)*240+22,196*2);
    for(unsigned i=0;i<100;i++){passport_view_render(&m,&r,"",false,0,82,false);frames(1);assert(names[0]==same_label);}
    bool changed=false;
    for(unsigned y=0;y<20;y++)if(memcmp(before+y*196,pixels+(BODY_Y+7+y)*240+22,196*2))changed=true;
    assert(changed);save(argv[1],"library-scrolled-title");
    passport_menu_item_t items[PASSPORT_MENU_MAX_ITEMS];m.selected=passport_menu_items(&m,&r,items)-1;
    passport_view_render(&m,&r,"",false,0,82,false);frames(20);
    assert(lv_obj_get_scroll_y(content)>0);lv_area_t selected;lv_obj_get_coords(rows[m.selected],&selected);
    assert(selected.y1>=BODY_Y&&selected.y2<BODY_Y+BODY_H);save(argv[1],"library-actions");
    m.page=MENU_MANAGE;m.selected=0;passport_view_render(&m,&r,"",false,0,82,false);frames(8);save(argv[1],"manage");
    m.page=MENU_CONFIRM;m.selected=0;m.confirm_slot=1;strcpy(m.confirm_id,r.slots[1].package.app_id);
    passport_view_render(&m,&r,"",false,0,82,false);frames(8);save(argv[1],"confirm-cancel");
    assert(kinds[0]==ITEM_CANCEL&&kinds[1]==ITEM_UNINSTALL);
    m.selected=1;passport_view_render(&m,&r,"",false,0,82,false);frames(8);save(argv[1],"confirm-erase");
    r.last_slot=0;r.autostart=1;m.page=MENU_AUTOSTART;m.selected=0;
    passport_view_render(&m,&r,"",false,0,82,true);frames(8);save(argv[1],"auto-on");
    r.autostart=0;passport_view_render(&m,&r,"",false,0,82,false);frames(8);save(argv[1],"auto-off");r.autostart=1;
    passport_view_render(&m,&r,"",true,5,82,true);frames(8);save(argv[1],"countdown-5");
    lv_obj_t *hero=lv_obj_get_child(content,0);
    passport_view_render(&m,&r,"",true,3,82,true);frames(8);
    assert(lv_obj_get_child(content,0)==hero&&!strcmp(lv_label_get_text(count_text),"3")&&lv_bar_get_value(count_bar)==3);
    save(argv[1],"countdown-3");
    strcpy(m.auto_id,r.slots[1].package.app_id);m.page=MENU_AUTO_PICK;m.selected=2;
    passport_view_render(&m,&r,"",false,0,82,true);frames(8);save(argv[1],"auto-app-picker");
    assert(!strcmp(lv_label_get_text(names[0]),"Last booted app"));
    passport_view_render(&m,&r,"",true,5,82,true);frames(8);
    hero=lv_obj_get_child(content,0);assert(!strcmp(lv_label_get_text(lv_obj_get_child(hero,1)),"QSO Vision"));
    save(argv[1],"countdown-selected");
    m.page=MENU_INSTALL;passport_view_render(&m,&r,"",false,0,82,true);frames(8);save(argv[1],"install");
    passport_view_render(&m,&r,"Not enough usable space.\nUninstall an app first.",false,0,82,true);frames(8);save(argv[1],"install-too-large");
    passport_view_render(&m,&r,"Unsupported ROM layout.\nCustom partitions need a port.",false,0,82,true);frames(8);save(argv[1],"install-layout-error");
    m.page=MENU_ABOUT;passport_view_render(&m,&r,"",false,0,82,true);frames(8);save(argv[1],"about");
    passport_view_scroll(1);passport_view_scroll(1);frames(20);int offset=lv_obj_get_scroll_y(content);assert(offset>0);
    passport_view_render(&m,&r,"",false,0,81,true);frames(8);assert(lv_obj_get_scroll_y(content)==offset);save(argv[1],"about-scroll");
    for(unsigned i=0;i<100;i++) {
        m.page=(passport_page_t)(i%7);m.selected=0;
        passport_view_render(&m,&r,"",false,0,82,true);frames(1);
    }
    /* Stress the full supported registry, beyond the original three ROMs. */
    for(unsigned i=3;i<PASSPORT_SLOTS;i++)app(&r,i,"A long additional community ROM title");
    m.page=MENU_LIBRARY;m.selected=PASSPORT_SLOTS-1;
    passport_view_render(&m,&r,"",false,0,82,true);frames(20);
    assert(row_count==PASSPORT_MENU_MAX_ITEMS);save(argv[1],"full-library");
    printf("Production LVGL 240x320 UI: selected cards, marquee continuity, scrolling viewport, cancel/erase dialog, countdown, About and 100 page changes PASS; minimum free LVGL pool: %zu / 49152 bytes\n",least_free);
    return 0;
}
