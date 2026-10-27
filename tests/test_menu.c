#include "passport_menu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void setup(passport_registry_t *r,unsigned states)
{
    passport_registry_init(r);
    for(unsigned i=0;i<3;i++) {
        unsigned state=(states>>(2*i))&3;
        if(!state)continue;
        passport_package_t p={.format=1,.abi=PASSPORT_ABI,.image_size=1000};memcpy(p.magic,PASSPORT_MAGIC,8);
        snprintf(p.app_id,sizeof(p.app_id),"community.app%u",i);snprintf(p.name,sizeof(p.name),"ROM %u",i);strcpy(p.version,"1");
        assert(passport_install_plan(r,&p,i,&r->slots[i]));
        r->slots[i].state=state;r->slots[i].data_initialized=1;
    }
    assert(passport_registry_valid(r));
}

int main(void)
{
    passport_registry_t r;passport_menu_t m;passport_menu_item_t rows[PASSPORT_MENU_MAX_ITEMS];
    setup(&r,(PASSPORT_RESERVED<<0)|(PASSPORT_READY<<2));passport_menu_init(&m);
    assert(passport_menu_items(&m,&r,rows)==5&&rows[0].slot==1&&rows[0].kind==ITEM_ROM);
    passport_menu_command_t c=passport_menu_input(&m,&r,MENU_OK);
    assert(c.action==MENU_LAUNCH&&c.slot==1); /* No empty/retained entries in library. */
    passport_menu_input(&m,&r,MENU_UP);passport_menu_input(&m,&r,MENU_OK);
    assert(m.page==MENU_ABOUT);assert(passport_menu_input(&m,&r,MENU_DOWN).action==MENU_SCROLL_DOWN);
    passport_menu_input(&m,&r,MENU_BACK);assert(m.page==MENU_LIBRARY&&rows[4].kind==ITEM_ABOUT);
    passport_menu_input(&m,&r,MENU_UP);passport_menu_input(&m,&r,MENU_OK);
    assert(m.page==MENU_AUTOSTART&&m.selected==0);
    assert(passport_menu_input(&m,&r,MENU_OK).action==MENU_TOGGLE_AUTO);
    passport_menu_input(&m,&r,MENU_BACK);passport_menu_input(&m,&r,MENU_UP);passport_menu_input(&m,&r,MENU_OK);
    assert(m.page==MENU_MANAGE);assert(passport_menu_items(&m,&r,rows)==3&&rows[0].slot==0);
    c=passport_menu_input(&m,&r,MENU_OK);assert(c.action==MENU_NONE&&m.page==MENU_CONFIRM&&m.selected==0);
    c=passport_menu_input(&m,&r,MENU_OK);assert(c.action==MENU_NONE&&m.page==MENU_MANAGE); /* Default Cancel. */
    passport_menu_input(&m,&r,MENU_DOWN);passport_menu_input(&m,&r,MENU_OK);
    assert(m.page==MENU_CONFIRM&&m.confirm_slot==1);
    passport_menu_input(&m,&r,MENU_DOWN);c=passport_menu_input(&m,&r,MENU_OK);
    assert(c.action==MENU_DELETE&&c.slot==1&&!strcmp(c.id,"community.app1"));
    passport_menu_input(&m,&r,MENU_BACK);passport_menu_input(&m,&r,MENU_OK);
    passport_menu_input(&m,&r,MENU_DOWN);strcpy(r.slots[1].package.app_id,"community.reassigned");
    c=passport_menu_input(&m,&r,MENU_OK);assert(c.action==MENU_NONE&&m.page==MENU_MANAGE);
    setup(&r,(PASSPORT_READY<<0)|(PASSPORT_READY<<2));r.last_slot=1;
    passport_menu_init(&m);m.page=MENU_AUTOSTART;m.selected=1;
    passport_menu_input(&m,&r,MENU_OK);assert(m.page==MENU_AUTO_PICK&&m.selected==0);
    assert(passport_menu_items(&m,&r,rows)==4&&rows[0].kind==ITEM_USE_LAST);
    assert(passport_autostart_target(&r,m.auto_id)==1);
    passport_menu_input(&m,&r,MENU_DOWN);c=passport_menu_input(&m,&r,MENU_OK);
    assert(c.action==MENU_SET_AUTO&&!strcmp(c.id,"community.app0")&&m.page==MENU_AUTOSTART);
    strcpy(m.auto_id,c.id);assert(passport_autostart_target(&r,m.auto_id)==0);
    passport_menu_input(&m,&r,MENU_OK);assert(m.page==MENU_AUTO_PICK&&m.selected==1);
    passport_menu_input(&m,&r,MENU_UP);c=passport_menu_input(&m,&r,MENU_OK);
    assert(c.action==MENU_SET_AUTO&&!c.id[0]);
    m.page=MENU_AUTO_PICK;strcpy(m.auto_id,"community.app0");
    c=passport_menu_input(&m,&r,MENU_BACK);assert(c.action==MENU_NONE&&m.page==MENU_AUTOSTART&&!strcmp(m.auto_id,"community.app0"));
    /* Exercise all 64 registry state combinations, every key, and navigation
     * between dialogs, lists and empty states without out-of-range selection. */
    for(unsigned states=0;states<64;states++)for(unsigned seed=0;seed<8;seed++) {
        setup(&r,states);passport_menu_init(&m);unsigned random=seed+1;
        for(unsigned step=0;step<300;step++) {
            random=random*1664525u+1013904223u;
            passport_menu_key_t key=(passport_menu_key_t)((random>>24)%4);
            passport_menu_t before=m;c=passport_menu_input(&m,&r,key);
            unsigned n=passport_menu_items(&m,&r,rows);
            assert(n<=PASSPORT_MENU_MAX_ITEMS&&(n?m.selected<n:m.selected==0));
            if(c.action==MENU_DELETE)assert(before.page==MENU_CONFIRM&&before.selected==1&&key==MENU_OK&&
                c.slot<3&&!strcmp(c.id,r.slots[c.slot].package.app_id));
            if(c.action==MENU_LAUNCH)assert(r.slots[c.slot].state==PASSPORT_READY);
            if(c.action==MENU_STOP_WEB)passport_menu_web_closed(&m,&r);
            if(c.action==MENU_DELETE)passport_menu_uninstalled(&m,&r);
        }
    }
    puts("Menu: installed-only library, About scrolling, explicit auto toggle, cancel-first uninstall, stable ID binding and 153600 navigation steps: PASS");
}
