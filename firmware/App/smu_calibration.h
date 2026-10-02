#ifndef SMU_CALIBRATION_H
#define SMU_CALIBRATION_H
#include "calibration_store.h"
#include "smu_measurement.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Pure least-squares fit of 2..8 nominal/reference pairs. Requires finite
 * data and a positive finite gain; leaves output untouched on failure. */
bool smu_calibration_fit_linear(const float* nominal, const float* reference, size_t count, smu_linear_cal_t* out);

void smu_calibration_init(void);
const smu_cal_record_t* smu_calibration_get(void);
smu_cal_load_result_t smu_calibration_load_result(void);
/* Optional foreground application guard. Begin must quiesce acquisition;
 * end runs after every accepted begin, including write/readback failures. */
void smu_calibration_set_save_hooks(bool (*begin)(void), void (*end)(void));
bool smu_calibration_commit(const smu_cal_record_t* candidate);
void smu_calibration_apply_to_measurement(void);
float smu_calibration_vforce_command(float physical_volts);
float smu_calibration_iforce_command(float nominal_command, smu_current_range_t range);

/* ------------------------------------------------------------------------
 * Voltage calibration (3-point VMEAS-to-CALBUS fit)
 * ------------------------------------------------------------------------ */

/* Raw ADS131M03 CH1 average code -> nominal (uncalibrated) SENSE voltage. */
float smu_calibration_vcal_nominal_voltage(float average_code);

/* Raw ADS131M03 CH2 average code -> calibrated CALBUS reference voltage. */
float smu_calibration_vcal_calbus_voltage(int32_t code);

/* Least-squares fit of 3 (nominal, calbus) points. Writes *gain, *offset and
 * residual[3] only on success; leaves them untouched if the points are
 * degenerate (near-singular fit). */
bool smu_calibration_vcal_fit(const volatile float x[3], const volatile float y[3], float* gain, float* offset, float residual[3]);

/* Commits gain/offset as the 15V range voltage calibration, preserving the
 * rest of the currently active calibration record. */
bool smu_calibration_voltage_measurement_commit(float gain, float offset);

#endif
