#pragma once
#include "stub_platform.h"
#define ESP_PARTITION_TABLE_MAX_LEN 0xc00
#define ESP_PARTITION_MAGIC 0x50aa
typedef struct {uint32_t offset,size;} esp_partition_pos_t;
typedef struct {uint16_t magic;uint8_t type,subtype;esp_partition_pos_t pos;uint8_t label[16];uint32_t flags;} esp_partition_info_t;
_Static_assert(sizeof(esp_partition_info_t)==32,"partition record");
