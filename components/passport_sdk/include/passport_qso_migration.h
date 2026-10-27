#pragma once
#include "esp_err.h"
#include "passport_qso_data.h"

/* Launcher-side staging. The caller serializes this with app management. */
esp_err_t passport_qso_stage(unsigned slot, const passport_qso_data_t *data);
/* QSO startup, BEFORE settings/network tasks. Never erases or overwrites a
 * completed import. Failed partial imports retry from the immutable blob. */
esp_err_t passport_qso_apply_migration(void);
