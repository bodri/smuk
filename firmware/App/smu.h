#ifndef SMU_H
#define SMU_H
#include "smu_acquisition.h"
#include "smu_measurement.h"
#include "smu_range.h"
#include "smu_types.h"
#include <stdbool.h>

/*
 * Top level of the SMU. main.c only calls smu_init() once after the CubeMX
 * peripheral init and smu_process() from the main loop.
 *
 * Current scope: measurement only (no sourcing). Boots in 1.5 A / 15 V with
 * current and voltage autorange enabled.
 */

/* Loads calibration, brings up the ADS131M03, selects the boot ranges and
 * starts acquisition. False if the ADC bring-up failed. */
bool smu_init(void);

/* Foreground loop: ADC frames -> measurement -> autorange -> range switches. */
void smu_process(void);

void smu_get_measurement(smu_measurement_outputs_t* out);
const smu_context_t* smu_get_context(void);

/* Manual current range; disables current autorange. */
smu_status_t smu_set_current_range(smu_current_range_t range);
/* Current autorange control. */
void smu_set_current_autorange(bool enabled);
void smu_set_voltage_autorange(bool enabled);
/* Manual voltage range; disables voltage autorange on success. */
smu_status_t smu_set_voltage_range(smu_voltage_range_t range);

/* Select differential input loading; rejects 10M while PA requested. */
smu_status_t smu_set_input_10m(bool enabled);

/* Foreground only; default stale 20 ms, stopped fault 1 s, 10 errors/1 s. */
smu_acquisition_config_t* smu_acquisition_config(void);

/* Nominal precision integration time: 1, 8, 20, 50 or 100 ms. */
smu_status_t smu_set_integration_ms(uint16_t milliseconds);

/* Tuning knobs (thresholds, persistence, discard counts). */
smu_range_config_t* smu_range_config(void);

#endif
