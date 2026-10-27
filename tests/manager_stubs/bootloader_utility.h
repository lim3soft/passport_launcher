#include "stub_platform.h"
#define FACTORY_INDEX (-1)
typedef struct { unsigned unused; } bootloader_state_t;
bool bootloader_utility_load_partition_table(bootloader_state_t *state);
int bootloader_utility_get_selected_boot_partition(const bootloader_state_t *state);
void bootloader_reset(void) __attribute__((noreturn));
void bootloader_utility_load_boot_image(const bootloader_state_t *state, int index) __attribute__((noreturn));
