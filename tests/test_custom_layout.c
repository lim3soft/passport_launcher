/* Reuse the production lifecycle backend, not an alternate installer. */
#define main previous_lifecycle_main
#include "test_lifecycle.c"
#undef main
static void load_custom(const char *dir,const char *name)
{
    char path[512];snprintf(path,sizeof(path),"%s/%s",dir,name);FILE *f=fopen(path,"rb");assert(f);
    fixture[0].size=fread(fixture[0].bytes,1,sizeof(fixture[0].bytes),f);assert(fgetc(f)==EOF);fclose(f);
    assert(passport_package_valid((passport_package_t *)fixture[0].bytes));
}
static void mapped_resources(const passport_record_t *r)
{
    passport_layout_t l;assert(passport_package_layout(&r->package,&l));
    const esp_partition_info_t *p=(const esp_partition_info_t *)(disk.flash+0x8000);
    for(unsigned i=0;i<l.count;i++) {
        const passport_resource_t *e=&l.entries[i];unsigned matches=0;
        for(unsigned j=0;p[j].magic==0x50aa;j++)if(!strcmp((const char *)p[j].label,e->label)) {
            assert(p[j].type==e->type&&p[j].subtype==e->subtype&&p[j].pos.offset==r->data_offset+e->offset&&p[j].pos.size==e->size);matches++;
            assert(p[j].pos.offset>=PASSPORT_POOL_START&&p[j].pos.offset+p[j].pos.size<=PASSPORT_POOL_END);
            if(!e->type)assert(!(p[j].pos.offset&65535));
        }
        assert(matches==1);
    }
}
static void lifecycle_custom(void)
{
    fresh();assert(install(0)==ESP_OK);passport_record_t a=stored_registry().slots[0];passport_layout_t l;
    assert(passport_package_layout(&a.package,&l));passport_capacity_t c;passport_capacity(&registry,&c);
    assert(c.allocated_bytes==a.image_capacity+passport_package_data_size(&a.package));
    const uint8_t *initial=fixture[0].bytes+512+a.package.image_size;
    assert(!memcmp(disk.flash+a.data_offset+PASSPORT_RESOURCE_BASE,initial,l.payload_size));
    launch(0);mapped_resources(&a);
    /* Model app writes through each relocated partition. Includes the full NVS
     * and test/recovery region; a return/reset must preserve all those bytes. */
    for(unsigned i=0;i<l.count;i++)disk.flash[a.data_offset+l.entries[i].offset+l.entries[i].size-1]=(uint8_t)(0x30+i);
    return_to_menu();assert(install(0)==ESP_OK); /* Update keeps private bytes. */
    assert(stored_registry().slots[0].data_offset==a.data_offset);
    launch(0);mapped_resources(&a);
    for(unsigned i=0;i<l.count;i++)assert(disk.flash[a.data_offset+l.entries[i].offset+l.entries[i].size-1]==(uint8_t)(0x30+i));
    return_to_menu();factory_unchanged();
    /* Corrupt a boot manifest: never issue a ticket. */
    disk.flash[a.data_offset+PASSPORT_DATA_SIZE+180]^=1;
    assert(passport_launch(0)==ESP_ERR_INVALID_CRC);
    disk.flash[a.data_offset+PASSPORT_DATA_SIZE+180]^=1;
    assert(passport_uninstall(0,a.package.app_id)==ESP_OK);
    for(uint32_t i=0;i<passport_package_data_size(&a.package);i++)assert(disk.flash[a.data_offset+i]==255);
    passport_capacity(&registry,&c);assert(c.free_bytes==c.total_bytes);
    assert(install(0)==ESP_OK);a=stored_registry().slots[0];
    assert(!memcmp(disk.flash+a.data_offset+PASSPORT_RESOURCE_BASE,initial,l.payload_size));
    factory_unchanged();
}
int main(int argc,char **argv)
{
    assert(argc==2);load_custom(argv[1],"custom-synthetic.transport");lifecycle_custom();
    fresh();uint8_t original=fixture[0].bytes[fixture[0].size-1];fixture[0].bytes[fixture[0].size-1]^=1;
    assert(install(0)==ESP_ERR_INVALID_CRC);assert(stored_registry().slots[0].state==PASSPORT_INSTALLING);
    fixture[0].bytes[fixture[0].size-1]=original;assert(install(0)==ESP_OK);
    passport_package_t *header=(passport_package_t *)fixture[0].bytes;passport_layout_t invalid;
    assert(passport_package_layout(header,&invalid));invalid.entries[0].offset=PASSPORT_RESOURCE_BASE-4096;
    memcpy(header->reserved,&invalid,sizeof(invalid));io_count=0;assert(install(0)!=ESP_OK);assert(stored_registry().slots[0].state==PASSPORT_READY);
    load_custom(argv[1],"custom-synthetic.transport");
    /* Exhaust each small custom install boundary before/after persistence.
     * Retrying after restart must initialize again, never boot partial data. */
    fresh();assert(install(0)==ESP_OK);int boundaries=io_count; /* exclude initial fresh scan below */
    fresh();io_count=0;assert(install(0)==ESP_OK);boundaries=io_count;
    for(int after=0;after<2;after++)for(int f=1;f<=boundaries;f++) {
        fresh();io_count=0;fail_at=f;fail_after_write=after;assert(install(0)!=ESP_OK);fail_at=0;
        assert(menu()==ESP_OK);passport_registry_t r=stored_registry();
        assert(r.slots[0].state!=PASSPORT_READY || image_ok(&apps[0],r.slots[0].package.image_size));
        assert(install(0)==ESP_OK);launch(0);r=stored_registry();mapped_resources(&r.slots[0]);return_to_menu();factory_unchanged();
    }
    printf("Custom layout: %d install I/O boundaries before/after writes, restart, retry, private-data updates and uninstall: PASS\n",boundaries);
    /* Failed updates preserve live saves, including interrupted manifest writes. */
    fresh();assert(install(0)==ESP_OK);passport_record_t own=stored_registry().slots[0];
    uint32_t save=own.data_offset+PASSPORT_RESOURCE_BASE+51;disk.flash[save]=0x27;baseline=disk;
    io_count=0;assert(install(0)==ESP_OK);int update_io=io_count;
    for(int after=0;after<2;after++)for(int f=1;f<=update_io;f++) {
        restore();fail_at=f;fail_after_write=after;assert(install(0)!=ESP_OK);fail_at=0;
        assert(menu()==ESP_OK && disk.flash[save]==0x27);assert(install(0)==ESP_OK);
        assert(disk.flash[save]==0x27);launch(0);return_to_menu();
    }
    restore();io_count=0;assert(passport_uninstall(0,own.package.app_id)==ESP_OK);int delete_io=io_count;
    for(int after=0;after<2;after++)for(int f=1;f<=delete_io;f++) {
        restore();fail_at=f;fail_after_write=after;assert(passport_uninstall(0,own.package.app_id)!=ESP_OK);fail_at=0;
        assert(menu()==ESP_OK);passport_registry_t r=stored_registry();
        if(r.slots[0].state!=PASSPORT_EMPTY)assert(passport_uninstall(0,own.package.app_id)==ESP_OK);
        for(uint32_t i=0;i<passport_package_data_size(&own.package);i++)assert(disk.flash[own.data_offset+i]==255);
    }
    printf("Custom update/delete faults: %d/%d I/O boundaries, saved data retained on update and full private erasure on uninstall: PASS\n",update_io,delete_io);
    for(unsigned n=0;n<3;n++) {
        char name[64],path[512];snprintf(name,sizeof(name),"custom-%u.transport",n);snprintf(path,sizeof(path),"%s/%s",argv[1],name);
        FILE *f=fopen(path,"rb");if(!f)continue;fclose(f);load_custom(argv[1],name);lifecycle_custom();
        printf("Owner community file %u: production install/hash/relocated table/private persistence/uninstall PASS; ESP32 code NOT executed\n",n);
    }
    return 0;
}
