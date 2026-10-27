/* Production boot hook, byte-addressed flash, ROM MD5 and torn table writes. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "../bootloader_components/passport_boot/hooks.c"
static rtc_retain_mem_t rtc;
static unsigned char table[0x1000];
static int reason,loaded,io,fail_at,writes;
static bool partial;
static jmp_buf boot_exit;
rtc_retain_mem_t *bootloader_common_get_rtc_retain_mem(void){return &rtc;}
int esp_rom_get_reset_reason(int cpu){assert(cpu==0);return reason;}
static bool fault(void){return ++io==fail_at;}
esp_err_t bootloader_flash_unlock(void){return fault()?ESP_FAIL:ESP_OK;}
esp_err_t bootloader_flash_read(size_t address,void *out,size_t n,bool decrypt)
{assert(address==0x8000&&n==0xc00&&!decrypt);if(fault())return ESP_FAIL;memcpy(out,table,n);return ESP_OK;}
esp_err_t bootloader_flash_erase_sector(size_t sector)
{assert(sector==8);bool f=fault();if(!f||partial)memset(table,255,f?2048:4096);writes++;return f?ESP_FAIL:ESP_OK;}
esp_err_t bootloader_flash_write(size_t address,const void *bytes,size_t n,bool encrypt)
{assert(address==0x8000&&n==0xc00&&!encrypt);bool f=fault();if(!f||partial)memcpy(table,bytes,f?n/2:n);writes++;return f?ESP_FAIL:ESP_OK;}
bool bootloader_utility_load_partition_table(bootloader_state_t *state)
{
    (void)state;unsigned count=0;
    const esp_partition_info_t *p=(const esp_partition_info_t *)table;
    while(p[count].magic==ESP_PARTITION_MAGIC)count++;
    assert(count==5||count==7);uint8_t digest[16];md5_context_t md5;
    esp_rom_md5_init(&md5);esp_rom_md5_update(&md5,table,count*32);esp_rom_md5_final(digest,&md5);
    assert(table[count*32]==0xeb&&!memcmp(digest,table+count*32+16,16));
    assert(p[3].pos.offset==0x10000&&p[3].pos.size==0x180000);
    assert(p[4].pos.offset==PASSPORT_META_ADDRESS);
    return true;
}
int bootloader_utility_get_selected_boot_partition(const bootloader_state_t *state){(void)state;return 0;}
void bootloader_reset(void){loaded=-99;longjmp(boot_exit,1);}
void bootloader_utility_load_boot_image(const bootloader_state_t *state,int index)
{(void)state;for(unsigned i=0;i<sizeof(rtc.custom);i++)assert(!rtc.custom[i]);loaded=index;longjmp(boot_exit,1);}
static void run(int reset_reason,int expected)
{reason=reset_reason;if(!setjmp(boot_exit))bootloader_after_init();assert(loaded==expected);}
int main(void)
{
    memset(table,255,sizeof(table));run(1,FACTORY_INDEX);assert(writes==2);
    io=0;writes=0;run(1,FACTORY_INDEX);assert(!writes); /* no erase on repeated menu boots */
    for(unsigned slot=0;slot<PASSPORT_SLOTS;slot++) {
        passport_ticket_t ticket=passport_make_ticket(slot);memcpy(rtc.custom,&ticket,sizeof(ticket));
        run(RESET_REASON_CORE_SW,0);run(RESET_REASON_CORE_SW,FACTORY_INDEX);
        for(int reset=1;reset<25;reset++) {
            memcpy(rtc.custom,&ticket,sizeof(ticket));
            run(reset,reset==RESET_REASON_CORE_SW||reset==RESET_REASON_CPU0_SW?0:FACTORY_INDEX);
        }
        for(unsigned byte=0;byte<sizeof(ticket);byte++) {
            memcpy(rtc.custom,&ticket,sizeof(ticket));rtc.custom[byte]^=1;
            run(RESET_REASON_CORE_SW,FACTORY_INDEX);
        }
    }
    /* Simulate loss before/after partial table read/erase/write/readback.
     * The very next reset must reconstruct the factory table without a ticket. */
    for(unsigned after=0;after<2;after++)for(int fail=1;fail<=5;fail++) {
        fail_at=0;run(1,FACTORY_INDEX);passport_ticket_t ticket=passport_make_ticket(2);
        memcpy(rtc.custom,&ticket,sizeof(ticket));io=0;fail_at=fail;partial=after;
        run(RESET_REASON_CORE_SW,-99);fail_at=0;run(RESET_REASON_CORE_SW,FACTORY_INDEX);
    }
    puts("Production boot: all reset causes and ticket bytes, exact table/MD5, no redundant erase, torn table recovery before/after every I/O: PASS");
}
