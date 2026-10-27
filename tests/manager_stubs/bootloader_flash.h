#include "stub_platform.h"
esp_err_t bootloader_flash_read(size_t address,void *out,size_t size,bool decrypt);
esp_err_t bootloader_flash_write(size_t address,const void *in,size_t size,bool encrypt);
esp_err_t bootloader_flash_erase_sector(size_t sector);
esp_err_t bootloader_flash_unlock(void);
