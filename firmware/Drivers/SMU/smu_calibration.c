#include "smu_calibration.h"

static smu_cal_record_t active;

static float inverse(float y, smu_source_cal_t c) {
	return (c.gain != 0.0f) ? ((y - c.offset) / c.gain) : 0.0f;
}

void smu_calibration_init(void) {
	(void) smu_cal_store_load(&active);
	smu_calibration_apply_to_measurement();
}

const smu_cal_record_t* smu_calibration_get(void) {
	return &active;
}

bool smu_calibration_commit(const smu_cal_record_t *c) {
	if (!c)
		return false;
	smu_cal_record_t x = *c;
	smu_cal_record_finalize(&x);
	if (!smu_cal_record_validate(&x) || !smu_cal_store_save(&x))
		return false;
	(void) smu_cal_store_load(&active);
	smu_calibration_apply_to_measurement();
	return true;
}

void smu_calibration_apply_to_measurement(void) {
	smu_filter_config_t cfg = { .fast_alpha = 0.25f, .precision_n = 32u };
	smu_measurement_init(&active.measurement, &cfg);
}

float smu_calibration_vforce_command(float v) {
	return inverse(v, active.vforce);
}

float smu_calibration_iforce_command(float v, smu_current_range_t r) {
	int i = (int) r - (int) SMU_RANGE_1P5A;
	if (i < 0 || i >= 5)
		return 0.0f;
	return inverse(v, active.iforce[i]);
}
