#include "passport_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static passport_package_t package(unsigned id,unsigned size)
{
    passport_package_t p={.format=1,.abi=0,.image_size=size};memcpy(p.magic,PASSPORT_MAGIC,8);
    snprintf(p.app_id,sizeof(p.app_id),"community.test%u",id);strcpy(p.name,"Test ROM");strcpy(p.version,"1");return p;
}
static void verify(const passport_registry_t *r)
{
    assert(passport_registry_valid(r));unsigned char occupied[(PASSPORT_POOL_END-PASSPORT_POOL_START)/4096]={0};
    unsigned allocated=0,payload=0;
    for(unsigned i=0;i<PASSPORT_SLOTS;i++)if(r->slots[i].state!=PASSPORT_EMPTY) {
        const passport_record_t *a=&r->slots[i];
        for(unsigned n=0;n<a->image_capacity/4096;n++){unsigned j=(a->image_offset-PASSPORT_POOL_START)/4096+n;assert(j<sizeof(occupied)&&!occupied[j]);occupied[j]=1;}
        for(unsigned n=0;n<PASSPORT_DATA_SIZE/4096;n++){unsigned j=(a->data_offset-PASSPORT_POOL_START)/4096+n;assert(j<sizeof(occupied)&&!occupied[j]);occupied[j]=1;}
        allocated+=a->image_capacity+PASSPORT_DATA_SIZE;payload+=a->package.image_size;
    }
    passport_capacity_t c;passport_capacity(r,&c);
    assert(c.total_bytes==sizeof(occupied)*4096&&c.allocated_bytes==allocated&&c.used_image_bytes==payload);
    assert(c.free_bytes+c.allocated_bytes==c.total_bytes);
    if(c.largest_image_bytes){passport_package_t p=package(999,c.largest_image_bytes);int slot=passport_install_slot(r,&p);passport_record_t plan;assert(slot>=0&&passport_install_plan(r,&p,slot,&plan));}
}
int main(void)
{
    passport_registry_t r;passport_registry_init(&r);passport_capacity_t c;passport_capacity(&r,&c);
    assert(c.free_bytes==0x660000&&c.largest_image_bytes==0x640000);
    passport_package_t p=package(1,0x280000);assert(passport_install_prepare(&r,&p,0)&&passport_install_finish(&r,0));
    unsigned stable=r.slots[0].data_offset;verify(&r);
    p=package(2,0x200000);assert(passport_install_prepare(&r,&p,1)&&passport_install_finish(&r,1));
    p=package(3,0x100000);assert(passport_install_prepare(&r,&p,2)&&passport_install_finish(&r,2));verify(&r);
    p=package(1,0x2c0000);passport_registry_t before=r;assert(!passport_install_prepare(&r,&p,0)&&!memcmp(&r,&before,sizeof(r)));
    p=package(1,0x100000);assert(passport_install_prepare(&r,&p,0)&&passport_install_finish(&r,0));assert(r.slots[0].data_offset==stable&&r.slots[0].data_initialized);verify(&r);
    p=package(4,0x100000);assert(passport_install_prepare(&r,&p,3)&&passport_install_finish(&r,3));verify(&r); /* fourth app, no three-slot ceiling */
    passport_registry_init(&r);p=package(1,0x640000);assert(passport_install_prepare(&r,&p,0)&&passport_install_finish(&r,0));
    passport_capacity(&r,&c);assert(!c.free_bytes&&!c.largest_image_bytes);verify(&r);
    p=package(2,288);assert(!passport_install_prepare(&r,&p,1));
    p=package(1,288);assert(passport_install_prepare(&r,&p,0)&&passport_install_finish(&r,0));verify(&r); /* same ID update reclaims its code */
    unsigned random=7;
    for(unsigned trial=0;trial<100;trial++) {
        passport_registry_init(&r);
        for(unsigned step=0;step<100;step++) {
            random=random*1664525u+1013904223u;unsigned slot=(random>>16)%PASSPORT_SLOTS;
            if((random&3)==0 && r.slots[slot].state!=PASSPORT_EMPTY){assert(passport_remove(&r,slot));memset(&r.slots[slot],0,sizeof(r.slots[slot]));}
            else {
                p=package(slot,(random%0x180000)+288);
                int target=passport_install_slot(&r,&p);before=r;
                if(target>=0 && passport_install_prepare(&r,&p,target))assert(passport_install_finish(&r,target));
                else assert(!memcmp(&before,&r,sizeof(r)));
            }
            verify(&r);
        }
    }
    puts("Shared flash: >2 MiB images, fourth app, private-data stability, exact free bytes, fragmentation, full-pool updates and 10000 random operations: PASS");
}
