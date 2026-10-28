#include "bootloader_common.h"
#include "bootloader_utility.h"
#include "bootloader_flash_priv.h"
#include "esp_flash_partitions.h"
#include "esp_rom_md5.h"
#include "esp_rom_sys.h"
#include "soc/reset_reasons.h"
#include "passport_model.h"
#include <string.h>

void bootloader_hooks_include(void) {}
_Alignas(4) static uint8_t desired[ESP_PARTITION_TABLE_MAX_LEN], previous[ESP_PARTITION_TABLE_MAX_LEN];
static unsigned entries;
static passport_package_t resource_package;
static void entry(uint8_t type,uint8_t subtype,uint32_t offset,uint32_t size,const char *label)
{
    esp_partition_info_t *p=(esp_partition_info_t *)desired+entries++;
    memset(p,0,sizeof(*p));p->magic=ESP_PARTITION_MAGIC;p->type=type;p->subtype=subtype;
    p->pos.offset=offset;p->pos.size=size;strncpy((char *)p->label,label,sizeof(p->label)-1);
}
/* The immutable bootloader reconstructs the table BEFORE the SDK reads it.
 * A torn table erase/write therefore cannot strand the device: the launch
 * ticket has already been consumed, and the next reset repairs the factory
 * table. Neither factory code nor application/data extents are rewritten here.
 * The table sector is programmed only when its contents must change. */
static bool prepare_table(const passport_ticket_t *ticket,bool launch)
{
    passport_layout_t layout={0};
    bool custom=launch && ticket->reserved;
    if(custom) {
        if(bootloader_flash_read(ticket->data_offset+PASSPORT_DATA_SIZE,&resource_package,sizeof(resource_package),false)!=ESP_OK ||
           !passport_package_valid(&resource_package) || !passport_package_layout(&resource_package,&layout) ||
           passport_package_data_size(&resource_package)!=ticket->reserved || resource_package.image_size>ticket->image_capacity)
            return false;
    }
    memset(desired,255,sizeof(desired));entries=0;
    bool custom_phy=false;
    for(unsigned i=0;i<layout.count;i++)if(!strcmp(layout.entries[i].label,"phy_init"))custom_phy=true;
    if(!custom)entry(1,2,0x9000,0x4000,"nvs");
    entry(1,0,0xd000,0x2000,"otadata");
    if(!custom_phy)entry(1,1,0xf000,0x1000,"phy_init");
    entry(0,0,0x10000,0x180000,"factory");
    entry(1,2,PASSPORT_META_ADDRESS,0x10000,"pos_meta");
    if(launch) {
        entry(0,16,ticket->image_offset,ticket->image_capacity,"active_app");
        char label[]="appdata0";label[7]="0123456789abcdef"[ticket->slot];
        entry(1,2,ticket->data_offset,PASSPORT_DATA_SIZE,label);
        for(unsigned i=0;i<layout.count;i++) {
            const passport_resource_t *p=&layout.entries[i];
            entry(p->type,p->subtype,ticket->data_offset+p->offset,p->size,p->label);
        }
    }
    uint8_t *marker=desired+entries*sizeof(esp_partition_info_t);
    marker[0]=0xeb;marker[1]=0xeb;
    md5_context_t md5;esp_rom_md5_init(&md5);
    esp_rom_md5_update(&md5,desired,entries*sizeof(esp_partition_info_t));esp_rom_md5_final(marker+16,&md5);
    if(bootloader_flash_read(0x8000,previous,sizeof(previous),false)!=ESP_OK)return false;
    if(!memcmp(previous,desired,sizeof(desired)))return true;
    if(bootloader_flash_unlock()!=ESP_OK || bootloader_flash_erase_sector(8)!=ESP_OK ||
       bootloader_flash_write(0x8000,desired,sizeof(desired),false)!=ESP_OK ||
       bootloader_flash_read(0x8000,previous,sizeof(previous),false)!=ESP_OK)return false;
    return !memcmp(previous,desired,sizeof(desired));
}
void bootloader_after_init(void)
{
    rtc_retain_mem_t *mem=bootloader_common_get_rtc_retain_mem();
    passport_ticket_t ticket,check;memcpy(&ticket,mem->custom,sizeof(ticket));check=ticket;
    int reason=esp_rom_get_reset_reason(0);
    int slot=passport_consume_ticket(&check,reason==RESET_REASON_CORE_SW || reason==RESET_REASON_CPU0_SW);
    memcpy(mem->custom,&check,sizeof(check)); /* Clear BEFORE any table erase. */
    if(!prepare_table(&ticket,slot>=0))bootloader_reset();
    bootloader_state_t state={0};
    if(!bootloader_utility_load_partition_table(&state))bootloader_reset();
    bootloader_utility_load_boot_image(&state,slot>=0?0:FACTORY_INDEX);
}
