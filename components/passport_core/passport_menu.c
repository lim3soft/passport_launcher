#include "passport_menu.h"
#include <string.h>

void passport_menu_init(passport_menu_t *m) { *m=(passport_menu_t){.page=MENU_LIBRARY,.confirm_slot=PASSPORT_NO_SLOT}; }

unsigned passport_menu_items(const passport_menu_t *m,const passport_registry_t *r,passport_menu_item_t out[PASSPORT_MENU_MAX_ITEMS])
{
    unsigned n=0;
    if(m->page==MENU_LIBRARY||m->page==MENU_MANAGE) {
        for(unsigned i=0;i<PASSPORT_SLOTS;i++)
            if(m->page==MENU_LIBRARY?r->slots[i].state==PASSPORT_READY:r->slots[i].state!=PASSPORT_EMPTY)
                out[n++]=(passport_menu_item_t){ITEM_ROM,i};
        if(m->page==MENU_LIBRARY) {
            out[n++]=(passport_menu_item_t){ITEM_INSTALL,0};
            out[n++]=(passport_menu_item_t){ITEM_MANAGE,0};
            out[n++]=(passport_menu_item_t){ITEM_AUTOSTART,0};
            out[n++]=(passport_menu_item_t){ITEM_ABOUT,0};
        } else out[n++]=(passport_menu_item_t){ITEM_BACK,0};
    } else if(m->page==MENU_AUTOSTART) {
        out[n++]=(passport_menu_item_t){ITEM_TOGGLE,0};
        out[n++]=(passport_menu_item_t){ITEM_CHOOSE_AUTO,0};out[n++]=(passport_menu_item_t){ITEM_BACK,0};
    } else if(m->page==MENU_AUTO_PICK) {
        out[n++]=(passport_menu_item_t){ITEM_USE_LAST,0};
        for(unsigned i=0;i<PASSPORT_SLOTS;i++)if(r->slots[i].state==PASSPORT_READY)out[n++]=(passport_menu_item_t){ITEM_AUTO_ROM,i};
        out[n++]=(passport_menu_item_t){ITEM_BACK,0};
    } else if(m->page==MENU_CONFIRM) {
        out[n++]=(passport_menu_item_t){ITEM_CANCEL,m->confirm_slot};
        out[n++]=(passport_menu_item_t){ITEM_UNINSTALL,m->confirm_slot};
    }
    return n;
}

static void library_focus(passport_menu_t *m,const passport_registry_t *r,passport_item_kind_t kind)
{
    m->page=MENU_LIBRARY;m->selected=0;
    passport_menu_item_t items[PASSPORT_MENU_MAX_ITEMS];unsigned n=passport_menu_items(m,r,items);
    for(unsigned i=0;i<n;i++)if(items[i].kind==kind){m->selected=i;break;}
}

static void manage_focus(passport_menu_t *m,const passport_registry_t *r)
{
    m->page=MENU_MANAGE;m->selected=0;
    passport_menu_item_t items[PASSPORT_MENU_MAX_ITEMS];unsigned n=passport_menu_items(m,r,items);
    for(unsigned i=0;i<n;i++)if(items[i].kind==ITEM_ROM&&items[i].slot==m->confirm_slot){m->selected=i;break;}
}

bool passport_menu_refresh(passport_menu_t *m,const passport_registry_t *r)
{
    bool valid=true;
    if(m->page==MENU_CONFIRM && (m->confirm_slot>=PASSPORT_SLOTS ||
       r->slots[m->confirm_slot].state==PASSPORT_EMPTY ||
       strcmp(m->confirm_id,r->slots[m->confirm_slot].package.app_id))) {
        manage_focus(m,r);valid=false;
    }
    passport_menu_item_t items[PASSPORT_MENU_MAX_ITEMS];unsigned n=passport_menu_items(m,r,items);
    if(!n)m->selected=0;else if(m->selected>=n)m->selected=n-1;
    return valid;
}

passport_menu_command_t passport_menu_input(passport_menu_t *m,const passport_registry_t *r,passport_menu_key_t key)
{
    passport_menu_command_t command={.action=MENU_NONE,.slot=PASSPORT_NO_SLOT};
    if(!passport_menu_refresh(m,r))return command;
    if(key==MENU_BACK) {
        if(m->page==MENU_INSTALL)command.action=MENU_STOP_WEB;
        else if(m->page==MENU_CONFIRM)manage_focus(m,r);
        else if(m->page==MENU_MANAGE)library_focus(m,r,ITEM_MANAGE);
        else if(m->page==MENU_AUTOSTART)library_focus(m,r,ITEM_AUTOSTART);
        else if(m->page==MENU_ABOUT)library_focus(m,r,ITEM_ABOUT);
        else if(m->page==MENU_AUTO_PICK){m->page=MENU_AUTOSTART;m->selected=1;}
        return command;
    }
    if(m->page==MENU_ABOUT) {
        if(key==MENU_UP)command.action=MENU_SCROLL_UP;
        else if(key==MENU_DOWN)command.action=MENU_SCROLL_DOWN;
        else if(key==MENU_OK)library_focus(m,r,ITEM_ABOUT);
        return command;
    }
    passport_menu_item_t items[PASSPORT_MENU_MAX_ITEMS];unsigned n=passport_menu_items(m,r,items);
    if(!n)return command;
    if(key==MENU_UP)m->selected=(m->selected+n-1)%n;
    else if(key==MENU_DOWN)m->selected=(m->selected+1)%n;
    else if(key==MENU_OK) {
        passport_menu_item_t item=items[m->selected];
        switch(item.kind) {
        case ITEM_ROM:
            if(m->page==MENU_LIBRARY){command.action=MENU_LAUNCH;command.slot=item.slot;}
            else {
                m->confirm_slot=item.slot;
                memcpy(m->confirm_id,r->slots[item.slot].package.app_id,sizeof(m->confirm_id));
                m->page=MENU_CONFIRM;m->selected=0; /* Cancel is always selected first. */
            }
            break;
        case ITEM_INSTALL:m->page=MENU_INSTALL;m->selected=0;command.action=MENU_START_WEB;break;
        case ITEM_MANAGE:m->page=MENU_MANAGE;m->selected=0;break;
        case ITEM_AUTOSTART:m->page=MENU_AUTOSTART;m->selected=0;break;
        case ITEM_ABOUT:m->page=MENU_ABOUT;m->selected=0;break;
        case ITEM_TOGGLE:command.action=MENU_TOGGLE_AUTO;break;
        case ITEM_CHOOSE_AUTO:
            m->page=MENU_AUTO_PICK;m->selected=0;
            n=passport_menu_items(m,r,items);
            for(unsigned i=0;i<n;i++)if(items[i].kind==ITEM_AUTO_ROM && !strcmp(m->auto_id,r->slots[items[i].slot].package.app_id))m->selected=i;
            break;
        case ITEM_USE_LAST:case ITEM_AUTO_ROM:
            command.action=MENU_SET_AUTO;
            if(item.kind==ITEM_AUTO_ROM)memcpy(command.id,r->slots[item.slot].package.app_id,sizeof(command.id));
            m->page=MENU_AUTOSTART;m->selected=1;break;
        case ITEM_BACK:
            if(m->page==MENU_AUTO_PICK){m->page=MENU_AUTOSTART;m->selected=1;break;}
            library_focus(m,r,m->page==MENU_MANAGE?ITEM_MANAGE:ITEM_AUTOSTART);break;
        case ITEM_CANCEL:manage_focus(m,r);break;
        case ITEM_UNINSTALL:
            command.action=MENU_DELETE;command.slot=m->confirm_slot;
            memcpy(command.id,m->confirm_id,sizeof(command.id));break;
        }
    }
    return command;
}

void passport_menu_uninstalled(passport_menu_t *m,const passport_registry_t *r)
{ manage_focus(m,r);passport_menu_refresh(m,r); }

void passport_menu_web_closed(passport_menu_t *m,const passport_registry_t *r)
{ (void)r;passport_menu_init(m); }
