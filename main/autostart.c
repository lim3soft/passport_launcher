#include "autostart.h"
#include "nvs_flash.h"
#include <string.h>
/* Separate optional key keeps the SDK 2 registry blob/layout compatible. */
typedef struct {uint32_t magic;char id[64];} selection_t;
static bool valid(const char id[64])
{
    const char *end=memchr(id,0,64);if(!end)return false;
    if(end!=id && (id[0]<'a'||id[0]>'z'))return false;
    for(const char *c=id;c<end;c++)if(!((*c>='a'&&*c<='z')||(*c>='0'&&*c<='9')||*c=='.'||*c=='-'||*c=='_'))return false;
    for(const char *c=end;c<id+64;c++)if(*c)return false;
    return true;
}
esp_err_t passport_autostart_load(char id[64])
{
    memset(id,0,64);nvs_handle_t h;
    esp_err_t err=nvs_open_from_partition("pos_meta","passport",NVS_READONLY,&h);
    if(err==ESP_ERR_NVS_NOT_FOUND)return ESP_OK;
    if(err!=ESP_OK)return err;
    selection_t selected;size_t bytes=sizeof(selected);
    err=nvs_get_blob(h,"auto_target_v1",&selected,&bytes);nvs_close(h);
    if(err==ESP_ERR_NVS_NOT_FOUND)return ESP_OK;
    if(err!=ESP_OK)return err;
    if(bytes!=sizeof(selected)||selected.magic!=0x41555431u||!valid(selected.id))return ESP_ERR_INVALID_STATE;
    memcpy(id,selected.id,64);return ESP_OK;
}
esp_err_t passport_autostart_save(const char *id)
{
    if(!id)return ESP_ERR_INVALID_ARG;
    selection_t selected={.magic=0x41555431u};
    size_t length=0;while(length<64 && id[length])length++;if(length==64)return ESP_ERR_INVALID_ARG;
    memcpy(selected.id,id,length);if(!valid(selected.id))return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;esp_err_t err=nvs_open_from_partition("pos_meta","passport",NVS_READWRITE,&h);
    if(err!=ESP_OK)return err;
    err=nvs_set_blob(h,"auto_target_v1",&selected,sizeof(selected));
    if(err==ESP_OK)err=nvs_commit(h);
    nvs_close(h);return err;
}
