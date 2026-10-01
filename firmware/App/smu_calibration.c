#include "smu_calibration.h"

static smu_cal_record_t active;

static float inverse(float y, smu_source_cal_t c) {
    return (c.gain != 0.0f) ? ((y - c.offset) / c.gain) : 0.0f;
}

void smu_calibration_init(void) {
    (void)smu_cal_store_load(&active);
    smu_measurement_init(&active.measurement, NULL);
}

const smu_cal_record_t* smu_calibration_get(void) {
    return &active;
}

bool smu_calibration_commit(const smu_cal_record_t* c) {
    if (!c)
        return false;
    smu_cal_record_t x = *c;
    smu_cal_record_finalize(&x);
    if (!smu_cal_record_validate(&x) || !smu_cal_store_save(&x))
        return false;
    (void)smu_cal_store_load(&active);
    smu_calibration_apply_to_measurement();
    return true;
}

void smu_calibration_apply_to_measurement(void) {
    smu_measurement_set_calibration(&active.measurement);
}

float smu_calibration_vforce_command(float v) {
    return inverse(v, active.vforce);
}

float smu_calibration_iforce_command(float v, smu_current_range_t r) {
    int i = (int)r - (int)SMU_RANGE_1P5A;
    if (i < 0 || i >= 5)
        return 0.0f;
    return inverse(v, active.iforce[i]);
}

float smu_calibration_vcal_nominal_voltage(float average_code) {
    float adc_v = smu_ads_code_to_volts((int32_t)average_code);

    return smu_voltage_from_adc(adc_v, SMU_VRANGE_15V);
}

/*
 * CH2 nominal analog scaling is CALBUS / 3.
 * Then apply the CALBUS calibration already stored in Flash.
 */
float smu_calibration_vcal_calbus_voltage(int32_t code) {
    const float adc_v = smu_ads_code_to_volts(code);

    const float nominal_calbus_v = smu_calbus_from_adc(adc_v);

    return (nominal_calbus_v * active.measurement.calbus.gain) + active.measurement.calbus.offset;
}

bool smu_calibration_vcal_fit(const volatile float x[3], const volatile float y[3], float* gain, float* offset, float residual[3]) {
    float sx = 0.0f;
    float sy = 0.0f;
    float sxx = 0.0f;
    float sxy = 0.0f;

    for (int i = 0; i < 3; i++) {
        float xi = x[i];
        float yi = y[i];

        sx += xi;
        sy += yi;
        sxx += xi * xi;
        sxy += xi * yi;
    }

    const float n = 3.0f;

    float denominator = n * sxx - sx * sx;

    if ((denominator > -1.0e-12f) && (denominator < 1.0e-12f)) {
        return false;
    }

    float g = (n * sxy - sx * sy) / denominator;

    float o = (sy - g * sx) / n;

    *gain = g;
    *offset = o;

    for (int i = 0; i < 3; i++) {
        float predicted = g * x[i] + o;

        residual[i] = y[i] - predicted;
    }

    return true;
}

bool smu_calibration_vforce_commit(float gain, float offset) {
    /*
     * Start from the currently active calibration record
     * so we preserve CALBUS calibration and everything else.
     */
    smu_cal_record_t candidate = active;

    candidate.measurement.voltage[SMU_VRANGE_15V].gain = gain;

    candidate.measurement.voltage[SMU_VRANGE_15V].offset = offset;

    /*
     * Increment calibration generation.
     */
    candidate.sequence++;

    return smu_calibration_commit(&candidate);
}
