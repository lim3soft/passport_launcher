#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PASSPORT_QSO_ID "org.passportos.qso-vision"
#define PASSPORT_QSO_MIGRATION_MARKER "PASSPORTOS_QSO_MIGRATION1"
#define PASSPORT_QSO_FIELDS 23u

/* Little-endian, fixed-size data-only format. Includes raw Wi-Fi credentials.
 * SHA-256 covers all 1024 payload bytes, including the source NVS digest. */
typedef struct {
    char magic[8];
    uint32_t format, present;
    int32_t numbers[4];
    char profile_ssids[8][33];
    char profile_passwords[8][65];
    char legacy_ssid[33], legacy_password[65], last_ssid[33];
    uint8_t source_sha256[32], reserved[45];
} passport_qso_payload_t;
typedef struct {
    passport_qso_payload_t data;
    uint8_t sha256[32];
} passport_qso_data_t;
_Static_assert(sizeof(passport_qso_payload_t) == 1024, "QSO data payload ABI");
_Static_assert(sizeof(passport_qso_data_t) == 1056, "QSO data file ABI");

/* Structural/range checks only; caller must also check SHA-256. */
bool passport_qso_data_valid(const passport_qso_data_t *data);
const char *passport_qso_field_key(unsigned index);
const char *passport_qso_field_string(const passport_qso_payload_t *data, unsigned index);
