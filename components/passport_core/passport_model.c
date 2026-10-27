#include "passport_model.h"
#include <string.h>

static bool overlap(uint32_t a,uint32_t n,uint32_t b,uint32_t m)
{ return a<b+m && b<a+n; }
static bool overlap_record(const passport_record_t *a,const passport_record_t *b)
{
    return overlap(a->image_offset,a->image_capacity,b->image_offset,b->image_capacity) ||
        overlap(a->image_offset,a->image_capacity,b->data_offset,PASSPORT_DATA_SIZE) ||
        overlap(a->data_offset,PASSPORT_DATA_SIZE,b->image_offset,b->image_capacity) ||
        overlap(a->data_offset,PASSPORT_DATA_SIZE,b->data_offset,PASSPORT_DATA_SIZE);
}
bool passport_allocation_valid(uint32_t image,uint32_t size,uint32_t data)
{
    return image>=PASSPORT_POOL_START && image<PASSPORT_POOL_END && !(image&0xffffu) &&
        size && !(size&0xfffu) && size<=PASSPORT_POOL_END-image &&
        data>=PASSPORT_POOL_START && data<PASSPORT_POOL_END && !(data&0xfffu) &&
        PASSPORT_DATA_SIZE<=PASSPORT_POOL_END-data && !overlap(image,size,data,PASSPORT_DATA_SIZE);
}
/* First fit over occupied extents, without moving another application's code
 * or private data. An update may reuse only its own previous code extent. */
static uint32_t find_space(const passport_registry_t *r,unsigned ignore_image,uint32_t size,
                           uint32_t alignment,uint32_t extra,uint32_t extra_size)
{
    uint32_t start=PASSPORT_POOL_START;
    while(start<PASSPORT_POOL_END && size<=PASSPORT_POOL_END-start) {
        uint32_t end=start;
        for(unsigned i=0;i<PASSPORT_SLOTS;i++)if(r->slots[i].state!=PASSPORT_EMPTY) {
            const passport_record_t *a=&r->slots[i];
            if(i!=ignore_image && overlap(start,size,a->image_offset,a->image_capacity) && a->image_offset+a->image_capacity>end)
                end=a->image_offset+a->image_capacity;
            if(overlap(start,size,a->data_offset,PASSPORT_DATA_SIZE) && a->data_offset+PASSPORT_DATA_SIZE>end)
                end=a->data_offset+PASSPORT_DATA_SIZE;
        }
        if(extra_size && overlap(start,size,extra,extra_size) && extra+extra_size>end)end=extra+extra_size;
        if(end==start)return start;
        start=(end+alignment-1)&~(alignment-1);
    }
    return 0;
}
bool passport_install_plan(const passport_registry_t *r,const passport_package_t *p,unsigned slot,passport_record_t *out)
{
    if(!out || slot>=PASSPORT_SLOTS || !passport_registry_valid(r) || !passport_package_valid(p))return false;
    uint32_t size=(p->image_size+0xfffu)&~0xfffu;
    uint32_t image=find_space(r,slot,size,0x10000,0,0);
    if(!image)return false;
    uint32_t data=r->slots[slot].state!=PASSPORT_EMPTY?r->slots[slot].data_offset:
        find_space(r,slot,PASSPORT_DATA_SIZE,0x1000,image,size);
    if(!data)return false;
    *out=(passport_record_t){.package=*p,.state=PASSPORT_INSTALLING,
        .image_offset=image,.image_capacity=size,.data_offset=data,
        .data_initialized=r->slots[slot].data_initialized};
    return true;
}
void passport_capacity(const passport_registry_t *r,passport_capacity_t *out)
{
    *out=(passport_capacity_t){.total_bytes=PASSPORT_POOL_END-PASSPORT_POOL_START};
    if(!passport_registry_valid(r))return;
    for(unsigned i=0;i<PASSPORT_SLOTS;i++)if(r->slots[i].state!=PASSPORT_EMPTY) {
        out->allocated_bytes+=r->slots[i].image_capacity+PASSPORT_DATA_SIZE;
        out->used_image_bytes+=r->slots[i].package.image_size;
    }
    out->free_bytes=out->total_bytes-out->allocated_bytes;
    passport_package_t p={.format=1,.abi=0};
    memcpy(p.magic,PASSPORT_MAGIC,8);strcpy(p.app_id,"capacity.probe");strcpy(p.name,"Probe");strcpy(p.version,"1");
    unsigned slot=0;while(slot<PASSPORT_SLOTS && r->slots[slot].state!=PASSPORT_EMPTY)slot++;
    if(slot==PASSPORT_SLOTS)return;
    /* Feasibility is monotone: binary search in erase-sector units. */
    unsigned low=0,high=PASSPORT_SLOT_SIZE/0x1000;
    passport_record_t plan;
    while(low<high) {
        unsigned middle=(low+high+1)/2;p.image_size=middle*0x1000;
        if(passport_install_plan(r,&p,slot,&plan))low=middle;else high=middle-1;
    }
    out->largest_image_bytes=low*0x1000;
}

static bool text_valid(const char *s, size_t n, bool id)
{
    const char *end = memchr(s, 0, n);
    if (!end || end == s) return false;
    if (id && (s[0] < 'a' || s[0] > 'z')) return false;
    for (const char *c = s; c < end; ++c) {
        if (id) {
            if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') ||
                  *c == '.' || *c == '-' || *c == '_')) return false;
        } else if ((unsigned char)*c < 32 || (unsigned char)*c > 126) return false;
    }
    for (const char *c = end; c < s + n; ++c) if (*c) return false;
    return true;
}

bool passport_package_valid(const passport_package_t *p)
{
    if (!p || memcmp(p->magic, PASSPORT_MAGIC, 8) || p->format != 1 ||
        (p->abi != 0 && p->abi != PASSPORT_ABI) || p->image_size < 288 || p->image_size > PASSPORT_SLOT_SIZE ||
        !text_valid(p->app_id, sizeof(p->app_id), true) ||
        !text_valid(p->name, sizeof(p->name), false) ||
        !text_valid(p->version, sizeof(p->version), false)) return false;
    for (unsigned i = 0; i < sizeof(p->reserved); ++i) if (p->reserved[i]) return false;
    return true;
}

void passport_registry_init(passport_registry_t *r)
{
    memset(r, 0, sizeof(*r));
    r->schema = 2;
    r->last_slot = PASSPORT_NO_SLOT;
}

bool passport_registry_valid(const passport_registry_t *r)
{
    if (!r || r->schema != 2 || r->autostart > 1 || r->launch_pending > 1 ||
        (r->last_slot != PASSPORT_NO_SLOT && r->last_slot >= PASSPORT_SLOTS)) return false;
    for (unsigned i = 0; i < PASSPORT_SLOTS; ++i) {
        const passport_record_t *a = &r->slots[i];
        if (a->state > PASSPORT_READY) return false;
        if (a->state == PASSPORT_EMPTY) {
            passport_package_t empty = {0};
            if (memcmp(&a->package, &empty, sizeof(empty)) || a->image_offset || a->image_capacity || a->data_offset || a->data_initialized) return false;
            continue;
        }
        if (a->package.abi != PASSPORT_ABI || !passport_package_valid(&a->package) ||
            !passport_allocation_valid(a->image_offset,a->image_capacity,a->data_offset) ||
            a->package.image_size > a->image_capacity || a->data_initialized>1 ||
            (a->state==PASSPORT_READY && !a->data_initialized)) return false;
        for (unsigned j = 0; j < i; ++j)
            if (r->slots[j].state != PASSPORT_EMPTY &&
                (!strcmp(a->package.app_id, r->slots[j].package.app_id) ||
                 overlap_record(a, &r->slots[j]))) return false;
    }
    return true;
}

int passport_install_slot(const passport_registry_t *r, const passport_package_t *p)
{
    if (!passport_registry_valid(r) || !passport_package_valid(p)) return -1;
    for (unsigned i = 0; i < PASSPORT_SLOTS; ++i)
        if (r->slots[i].state != PASSPORT_EMPTY && !strcmp(r->slots[i].package.app_id, p->app_id)) return (int)i;
    for (unsigned i = 0; i < PASSPORT_SLOTS; ++i)
        if (r->slots[i].state == PASSPORT_EMPTY) return (int)i;
    return -1;
}

bool passport_install_prepare(passport_registry_t *r, const passport_package_t *p, unsigned slot)
{
    if (slot >= PASSPORT_SLOTS || passport_install_slot(r, p) != (int)slot) return false;
    passport_record_t planned;
    if (!passport_install_plan(r,p,slot,&planned)) return false;
    r->slots[slot] = planned;
    r->slots[slot].package = *p;
    /* ABI 0 is an upload transport for legacy images. Keep durable registry
     * entries readable by earlier ABI 1 SDK apps; mode lives in pos_meta. */
    r->slots[slot].package.abi = PASSPORT_ABI;
    r->slots[slot].state = PASSPORT_INSTALLING;
    if (r->last_slot == slot) r->last_slot = PASSPORT_NO_SLOT;
    return true;
}

bool passport_install_finish(passport_registry_t *r, unsigned slot)
{
    if (slot >= PASSPORT_SLOTS || r->slots[slot].state != PASSPORT_INSTALLING) return false;
    r->slots[slot].state = PASSPORT_READY;
    r->slots[slot].data_initialized=1;
    return true;
}

bool passport_remove(passport_registry_t *r, unsigned slot)
{
    if (slot >= PASSPORT_SLOTS || r->slots[slot].state == PASSPORT_EMPTY) return false;
    r->slots[slot].state = PASSPORT_RESERVED;
    if (r->last_slot == slot) r->last_slot = PASSPORT_NO_SLOT;
    return true;
}

unsigned passport_autostart_target(const passport_registry_t *r,const char *id)
{
    if(id && id[0]) {
        for(unsigned i=0;i<PASSPORT_SLOTS;i++)if(r->slots[i].state!=PASSPORT_EMPTY && !strcmp(r->slots[i].package.app_id,id))
            return r->slots[i].state==PASSPORT_READY?i:PASSPORT_NO_SLOT;
        return PASSPORT_NO_SLOT;
    }
    return r->last_slot<PASSPORT_SLOTS && r->slots[r->last_slot].state==PASSPORT_READY?r->last_slot:PASSPORT_NO_SLOT;
}

bool passport_should_autostart(const passport_registry_t *r, bool cold_boot)
{
    return passport_registry_valid(r) && cold_boot && r->autostart && !r->launch_pending &&
           r->last_slot < PASSPORT_SLOTS && r->slots[r->last_slot].state == PASSPORT_READY;
}

static uint32_t ticket_checksum(const passport_ticket_t *t)
{
    const uint8_t *bytes=(const uint8_t *)t;
    uint32_t hash=2166136261u;
    for(size_t i=0;i<offsetof(passport_ticket_t,checksum);i++)hash=(hash^bytes[i])*16777619u;
    return hash;
}
passport_ticket_t passport_make_launch_ticket(unsigned slot,const passport_record_t *a)
{
    passport_ticket_t t={.magic=PASSPORT_TICKET_MAGIC,.slot=slot,.inverse=~slot,
        .image_offset=a->image_offset,.image_capacity=a->image_capacity,.data_offset=a->data_offset};
    t.checksum=ticket_checksum(&t);return t;
}
passport_ticket_t passport_make_ticket(unsigned slot)
{
    passport_record_t a={.image_offset=PASSPORT_POOL_START+slot*0x30000u,
        .image_capacity=0x10000,.data_offset=PASSPORT_POOL_START+slot*0x30000u+0x10000};
    return passport_make_launch_ticket(slot,&a);
}
int passport_consume_ticket(passport_ticket_t *ticket,bool software_reset)
{
    passport_ticket_t old=*ticket;memset(ticket,0,sizeof(*ticket));
    return software_reset && old.magic==PASSPORT_TICKET_MAGIC && old.slot<PASSPORT_SLOTS &&
        old.inverse==~old.slot && !old.reserved && old.checksum==ticket_checksum(&old) &&
        passport_allocation_valid(old.image_offset,old.image_capacity,old.data_offset) ? (int)old.slot:-1;
}
