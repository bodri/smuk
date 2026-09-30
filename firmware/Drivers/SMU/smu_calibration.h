#ifndef SMU_CALIBRATION_H
#define SMU_CALIBRATION_H
#include "smu_measurement.h"
#include "../Storage/calibration_store.h"
#include <stdbool.h>

void smu_calibration_init(void);
const smu_cal_record_t* smu_calibration_get(void);
bool smu_calibration_commit(const smu_cal_record_t *candidate);
void smu_calibration_apply_to_measurement(void);
float smu_calibration_vforce_command(float physical_volts);
float smu_calibration_iforce_command(float nominal_command,
		smu_current_range_t range);

#endif
