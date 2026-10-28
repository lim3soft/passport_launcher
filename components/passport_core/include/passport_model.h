#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PASSPORT_SLOTS 16u
#define PASSPORT_POOL_START 0x190000u
#define PASSPORT_POOL_END 0x7f0000u
#define PASSPORT_DATA_SIZE 0x20000u
#define PASSPORT_META_ADDRESS 0x7f0000u
#define PASSPORT_SLOT_SIZE (PASSPORT_POOL_END - PASSPORT_POOL_START - PASSPORT_DATA_SIZE)
#define PASSPORT_ABI 2u
#define PASSPORT_MAGIC "PPTOS01"
#define PASSPORT_TICKET_MAGIC 0x50505431u
#define PASSPORT_NO_SLOT UINT32_MAX
#define PASSPORT_LAYOUT_MAGIC 0x31444c50u /* PLD1 */
#define PASSPORT_RESOURCE_BASE (PASSPORT_DATA_SIZE + 0x1000u)
#define PASSPORT_RESOURCE_COUNT 8u

/* Offsets are relative to this app's private extent, never input flash addresses. */
typedef struct {
    uint16_t magic;
    uint8_t type, subtype;
    uint32_t offset, size;
    char label[16];
    uint32_t flags;
} passport_resource_t;
typedef struct {
    uint32_t magic, count, payload_size, reserved;
    uint8_t sha256[32];
    passport_resource_t entries[PASSPORT_RESOURCE_COUNT];
    uint8_t padding[28];
} passport_layout_t;
_Static_assert(sizeof(passport_layout_t)==332, "layout extension ABI");

/* All integer fields are little endian. Reserved bytes are zero for the original
 * format, or contain the validated PLD1 private-partition extension.
 * Upload ABI 0 denotes an unmodified community image. Durable v2 registry entries
 * use SDK ABI 2; pos_meta records community storage mode. */
typedef struct {
    char magic[8];
    uint32_t format, abi, image_size;
    char app_id[64], name[32], version[32];
    uint8_t sha256[32], reserved[332];
} passport_package_t;
_Static_assert(sizeof(passport_package_t) == 512, "package ABI");

typedef enum {
    PASSPORT_EMPTY, PASSPORT_RESERVED, PASSPORT_INSTALLING, PASSPORT_READY
} passport_state_t;
typedef struct {
    passport_package_t package;
    uint32_t state;
    uint32_t image_offset, image_capacity, data_offset, data_initialized;
} passport_record_t;
typedef struct {
    uint32_t schema, last_slot, autostart, launch_pending;
    passport_record_t slots[PASSPORT_SLOTS];
} passport_registry_t;
typedef struct {
    uint32_t magic, slot, inverse, reserved;
    uint32_t image_offset, image_capacity, data_offset, checksum;
} passport_ticket_t;

typedef struct {uint32_t total_bytes, free_bytes, largest_image_bytes, used_image_bytes, allocated_bytes;} passport_capacity_t;
void passport_capacity(const passport_registry_t *r, passport_capacity_t *out);
bool passport_install_plan(const passport_registry_t *r, const passport_package_t *p, unsigned slot, passport_record_t *out);
bool passport_allocation_valid(uint32_t image_offset,uint32_t image_capacity,uint32_t data_offset);
passport_ticket_t passport_make_launch_ticket(unsigned slot, const passport_record_t *record);

bool passport_package_valid(const passport_package_t *p);
bool passport_package_layout(const passport_package_t *p, passport_layout_t *out);
uint32_t passport_package_data_size(const passport_package_t *p);
bool passport_package_layout_equal(const passport_package_t *a,const passport_package_t *b);
void passport_registry_init(passport_registry_t *r);
bool passport_registry_valid(const passport_registry_t *r);
/* Reuses the same ID's private data. Code is sized from remaining extents. */
int passport_install_slot(const passport_registry_t *r, const passport_package_t *p);
bool passport_install_prepare(passport_registry_t *r, const passport_package_t *p, unsigned slot);
bool passport_install_finish(passport_registry_t *r, unsigned slot);
bool passport_remove(passport_registry_t *r, unsigned slot);
unsigned passport_autostart_target(const passport_registry_t *r,const char *id);
bool passport_should_autostart(const passport_registry_t *r, bool cold_boot);
passport_ticket_t passport_make_ticket(unsigned slot);
int passport_consume_ticket(passport_ticket_t *ticket, bool software_reset);
