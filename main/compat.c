#include "compat.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "mbedtls/sha256.h"
#include <stdlib.h>
#include <string.h>

/* NVS blobs are atomic records. The journal always precedes raw flash changes.
 * RESTORING: no application could have run; keep its previous saved snapshot.
 * ACTIVE: default NVS is authoritative; save it before clearing launcher NVS.
 * SAVED: snapshot committed; never recapture a partially cleared default NVS.
 * IDLE: the launcher may initialize default NVS for its own radio calibration.
 * This isolates cooperating default-NVS users, not arbitrary raw flash writes. */
enum { IDLE, RESTORING, ACTIVE, SAVED, NVS_BYTES=0x4000 };
#define CONTEXT_MAGIC 0x504c4331u
typedef struct { uint32_t magic, phase, slot; char id[64]; } context_t;
typedef struct { char id[64]; uint8_t data[NVS_BYTES], digest[32]; } saved_t;
typedef struct { uint32_t magic, legacy; char id[64]; } passport_mode_t;

esp_err_t passport_compat_mode(const passport_registry_t *r,unsigned slot,bool *legacy)
{
    if(slot>=PASSPORT_SLOTS||!legacy)return ESP_ERR_INVALID_ARG;
    *legacy=false;
    nvs_handle_t h;
    esp_err_t e=nvs_open_from_partition("pos_meta","compat",NVS_READONLY,&h);
    if(e==ESP_ERR_NVS_NOT_FOUND)return ESP_OK;
    if(e!=ESP_OK)return e;
    char key[]="mode0";key[4]="0123456789abcdef"[slot];
    passport_mode_t mode;size_t n=sizeof(mode);
    e=nvs_get_blob(h,key,&mode,&n);nvs_close(h);
    if(e==ESP_ERR_NVS_NOT_FOUND)return ESP_OK; /* Earlier SDK app: native by default. */
    if(e!=ESP_OK)return e;
    if(n!=sizeof(mode)||mode.magic!=CONTEXT_MAGIC||mode.legacy>1||!memchr(mode.id,0,sizeof(mode.id)))
        return ESP_ERR_INVALID_STATE;
    if(!strcmp(mode.id,r->slots[slot].package.app_id))*legacy=mode.legacy!=0;
    return ESP_OK;
}

esp_err_t passport_compat_set_mode(unsigned slot,const char *id,bool legacy)
{
    if(slot>=PASSPORT_SLOTS||!id||strlen(id)>=64)return ESP_ERR_INVALID_ARG;
    passport_mode_t mode={.magic=CONTEXT_MAGIC,.legacy=legacy};strcpy(mode.id,id);
    nvs_handle_t h;
    esp_err_t e=nvs_open_from_partition("pos_meta","compat",NVS_READWRITE,&h);
    if(e!=ESP_OK)return e;
    char key[]="mode0";key[4]="0123456789abcdef"[slot];
    e=nvs_set_blob(h,key,&mode,sizeof(mode));
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);return e;
}

static const esp_partition_t *default_nvs(void)
{
    const esp_partition_t *p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_NVS,"nvs");
    return p && p->address==0x9000 && p->size==NVS_BYTES ? p : NULL;
}

static esp_err_t context_read(context_t *c)
{
    *c=(context_t){.magic=CONTEXT_MAGIC,.phase=IDLE,.slot=PASSPORT_NO_SLOT};
    nvs_handle_t h;
    esp_err_t e=nvs_open_from_partition("pos_meta","compat",NVS_READONLY,&h);
    if(e==ESP_ERR_NVS_NOT_FOUND)return ESP_OK;
    if(e!=ESP_OK)return e;
    size_t n=sizeof(*c); e=nvs_get_blob(h,"context_v1",c,&n); nvs_close(h);
    if(e==ESP_ERR_NVS_NOT_FOUND) {
        *c=(context_t){.magic=CONTEXT_MAGIC,.phase=IDLE,.slot=PASSPORT_NO_SLOT};return ESP_OK;
    }
    if(e!=ESP_OK)return e;
    if(n!=sizeof(*c)||c->magic!=CONTEXT_MAGIC||c->phase>SAVED)return ESP_ERR_INVALID_STATE;
    if(c->phase==IDLE && (c->slot!=PASSPORT_NO_SLOT || c->id[0]))return ESP_ERR_INVALID_STATE;
    return ESP_OK;
}

static esp_err_t context_write(const context_t *c)
{
    nvs_handle_t h;
    esp_err_t e=nvs_open_from_partition("pos_meta","compat",NVS_READWRITE,&h);
    if(e!=ESP_OK)return e;
    e=nvs_set_blob(h,"context_v1",c,sizeof(*c));
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);return e;
}

static esp_err_t private_open(const passport_registry_t *r,unsigned slot,nvs_handle_t *h)
{
    char label[]="appdata0";label[7]="0123456789abcdef"[slot];
    const esp_partition_t *p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_NVS,label);
    if(!p||p->address!=r->slots[slot].data_offset||p->size!=PASSPORT_DATA_SIZE)return ESP_ERR_INVALID_STATE;
    esp_err_t e=nvs_flash_init_partition(label);
    return e==ESP_OK ? nvs_open_from_partition(label,"ppl_legacy",NVS_READWRITE,h) : e;
}

static esp_err_t unmount_default(void)
{
    esp_err_t e=nvs_flash_deinit_partition("nvs");
    return e==ESP_ERR_NVS_NOT_INITIALIZED ? ESP_OK : e;
}

esp_err_t passport_compat_recover(const passport_registry_t *r)
{
    context_t c;
    esp_err_t e=context_read(&c);
    if(e!=ESP_OK||c.phase==IDLE)return e;
    const esp_partition_t *p=default_nvs();
    if(!p||c.slot>=PASSPORT_SLOTS||!memchr(c.id,0,sizeof(c.id))||
       r->slots[c.slot].state!=PASSPORT_READY||
       strcmp(c.id,r->slots[c.slot].package.app_id))return ESP_ERR_INVALID_STATE;
    bool legacy=false;e=passport_compat_mode(r,c.slot,&legacy);
    if(e!=ESP_OK)return e;
    if(!legacy)return ESP_ERR_INVALID_STATE;
    e=unmount_default();if(e!=ESP_OK)return e;
    if(c.phase==ACTIVE) {
        saved_t *s=malloc(sizeof(*s));
        if(!s)return ESP_ERR_NO_MEM;
        memcpy(s->id,c.id,sizeof(s->id));
        e=esp_partition_read(p,0,s->data,NVS_BYTES);
        if(e==ESP_OK && mbedtls_sha256((uint8_t *)s,offsetof(saved_t,digest),s->digest,0)!=0)e=ESP_FAIL;
        nvs_handle_t h;
        if(e==ESP_OK) {
            e=private_open(r,c.slot,&h);
            if(e==ESP_OK) {
                e=nvs_set_blob(h,"nvs_v1",s,sizeof(*s));
                if(e==ESP_OK)e=nvs_commit(h);
                nvs_close(h);
            }
        }
        free(s);
        if(e!=ESP_OK)return e;
        c.phase=SAVED;e=context_write(&c);if(e!=ESP_OK)return e;
    }
    e=esp_partition_erase_range(p,0,NVS_BYTES);if(e!=ESP_OK)return e;
    c=(context_t){.magic=CONTEXT_MAGIC,.phase=IDLE,.slot=PASSPORT_NO_SLOT};
    return context_write(&c);
}

esp_err_t passport_compat_begin(const passport_registry_t *r,unsigned slot)
{
    if(slot>=PASSPORT_SLOTS||r->slots[slot].state!=PASSPORT_READY)
        return ESP_ERR_INVALID_ARG;
    esp_err_t e=passport_compat_recover(r);if(e!=ESP_OK)return e;
    bool legacy=false;e=passport_compat_mode(r,slot,&legacy);
    if(e!=ESP_OK)return e;
    if(!legacy)return ESP_ERR_INVALID_STATE;
    const esp_partition_t *p=default_nvs();if(!p)return ESP_ERR_INVALID_STATE;
    saved_t *s=malloc(sizeof(*s));if(!s)return ESP_ERR_NO_MEM;
    nvs_handle_t h;e=private_open(r,slot,&h);
    if(e!=ESP_OK){free(s);return e;}
    size_t n=sizeof(*s);e=nvs_get_blob(h,"nvs_v1",s,&n);nvs_close(h);
    if(e==ESP_ERR_NVS_NOT_FOUND) {
        memset(s,0,sizeof(*s));memcpy(s->id,r->slots[slot].package.app_id,sizeof(s->id));
        memset(s->data,0xff,sizeof(s->data));e=ESP_OK;
    } else if(e==ESP_OK) {
        uint8_t digest[32];
        if(n!=sizeof(*s)||memcmp(s->id,r->slots[slot].package.app_id,sizeof(s->id))||
           mbedtls_sha256((uint8_t *)s,offsetof(saved_t,digest),digest,0)!=0||memcmp(digest,s->digest,32))
            e=ESP_ERR_INVALID_CRC;
    }
    context_t c={.magic=CONTEXT_MAGIC,.phase=RESTORING,.slot=slot};
    memcpy(c.id,r->slots[slot].package.app_id,sizeof(c.id));
    if(e==ESP_OK)e=unmount_default();
    if(e==ESP_OK)e=context_write(&c);
    if(e==ESP_OK)e=esp_partition_erase_range(p,0,NVS_BYTES);
    if(e==ESP_OK)e=esp_partition_write(p,0,s->data,NVS_BYTES);
    if(e==ESP_OK) {
        uint8_t check[256];
        for(size_t i=0;e==ESP_OK && i<NVS_BYTES;i+=sizeof(check)) {
            e=esp_partition_read(p,i,check,sizeof(check));
            if(e==ESP_OK && memcmp(check,s->data+i,sizeof(check)))e=ESP_ERR_INVALID_CRC;
        }
    }
    free(s);
    if(e==ESP_OK){c.phase=ACTIVE;e=context_write(&c);}
    return e;
}
