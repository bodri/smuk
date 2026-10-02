#ifndef SMU_WATCHDOG_H
#define SMU_WATCHDOG_H
#include "smu_types.h"
#include <stdbool.h>
/* Start once after logging transport is available, before instrument bring-up.
 * Foreground only. Hardware timeout is nominally 2 s. */
bool smu_watchdog_start(void);
/* Call only after instrument and console work completed. A responsive fault
 * stays latched and serviceable only with PA request and input load off. */
bool smu_watchdog_service(smu_state_t state, bool pa_requested, bool input_10m);
/* One refresh before an accepted calibration save, never inside Flash waits.
 * A not-yet-started watchdog is allowed for standalone module use. */
bool smu_watchdog_before_flash(void);
bool smu_watchdog_active(void);
bool smu_watchdog_was_reset(void);
#endif
