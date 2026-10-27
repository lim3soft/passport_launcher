#include "passport_qso_data.h"
#include <string.h>

const char *passport_qso_field_key(unsigned i)
{
    static const char *keys[PASSPORT_QSO_FIELDS] = {
        "talkgroup", "volume", "dim_mode", "brightness",
        "p0_s", "p0_p", "p1_s", "p1_p", "p2_s", "p2_p", "p3_s", "p3_p",
        "p4_s", "p4_p", "p5_s", "p5_p", "p6_s", "p6_p", "p7_s", "p7_p",
        "wifi_ssid", "wifi_password", "wifi_last_ssid"
    };
    return i < PASSPORT_QSO_FIELDS ? keys[i] : NULL;
}

const char *passport_qso_field_string(const passport_qso_payload_t *p, unsigned i)
{
    if (!p || i < 4 || i >= PASSPORT_QSO_FIELDS) return NULL;
    if (i < 20) return (i & 1) ? p->profile_passwords[(i-4)/2] : p->profile_ssids[(i-4)/2];
    return i == 20 ? p->legacy_ssid : i == 21 ? p->legacy_password : p->last_ssid;
}

bool passport_qso_data_valid(const passport_qso_data_t *p)
{
    if (!p || memcmp(p->data.magic, "PPTQSO1", 8) || p->data.format != 1 ||
        !p->data.present || p->data.present >> PASSPORT_QSO_FIELDS) return false;
    const int32_t low[] = {91, 0, 0, 1}, high[] = {16777215, 100, 4, 100};
    for (unsigned i = 0; i < PASSPORT_QSO_FIELDS; ++i) {
        bool present = (p->data.present & (1u << i)) != 0;
        if (i < 4) {
            int32_t value = p->data.numbers[i];
            if (present ? (value < low[i] || value > high[i]) : value != 0) return false;
        } else {
            const char *value = passport_qso_field_string(&p->data, i);
            size_t length = (i < 20 && (i & 1)) || i == 21 ? 65 : 33;
            const char *end = memchr(value, 0, length);
            if (!end) return false;
            if (!present && end != value) return false;
            for (; end < value + length; ++end) if (*end) return false;
        }
    }
    for (size_t i = 0; i < sizeof(p->data.reserved); ++i) if (p->data.reserved[i]) return false;
    return true;
}
