#include "smu_calibration.h"
#include <math.h>
#include <string.h>

static smu_cal_record_t active;
static smu_cal_load_result_t load_result;
static bool (*save_begin)(void);
static void (*save_end)(void);

static float inverse(float y, smu_source_cal_t c) {
    return (c.gain != 0.0f) ? ((y - c.offset) / c.gain) : 0.0f;
}

void smu_calibration_init(void) {
    save_begin = NULL;
    save_end = NULL;
    load_result = smu_cal_store_load(&active);
    if (load_result == SMU_CAL_LOAD_INVALID || !smu_cal_record_usable(&active)) {
        smu_cal_record_defaults(&active);
        load_result = SMU_CAL_LOAD_DEFAULTS;
    }
    smu_measurement_init(&active.measurement, NULL);
}

const smu_cal_record_t* smu_calibration_get(void) {
    return &active;
}

smu_cal_load_result_t smu_calibration_load_result(void) {
    return load_result;
}

void smu_calibration_set_save_hooks(bool (*begin)(void), void (*end)(void)) {
    /* Both hooks are required so an accepted begin always has cleanup. */
    save_begin = begin && end ? begin : NULL;
    save_end = begin && end ? end : NULL;
}

bool smu_calibration_commit(const smu_cal_record_t* c) {
    if (!c)
        return false;
    smu_cal_record_t candidate = *c;
    smu_cal_record_finalize(&candidate);
    if (!smu_cal_record_validate(&candidate) || (save_begin && !save_begin()))
        return false;
    bool ok = false;
    smu_cal_record_t loaded;
    if (!smu_cal_store_save(&candidate))
        goto done;
    if (smu_cal_store_load(&loaded) != SMU_CAL_LOAD_OK || !smu_cal_record_validate(&loaded) || (int32_t)(loaded.sequence - active.sequence) <= 0)
        goto done;
    /* Sequence belongs to storage; all other persisted fields must match. */
    candidate.sequence = loaded.sequence;
    smu_cal_record_finalize(&candidate);
    if (memcmp(&candidate, &loaded, sizeof(candidate)) != 0)
        goto done;
    active = loaded;
    load_result = SMU_CAL_LOAD_OK;
    smu_calibration_apply_to_measurement();
    ok = true;
done:
    if (save_end)
        save_end();
    return ok;
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
    if (!x || !y || !gain || !offset || !residual)
        return false;
    float nominal[3], reference[3];
    for (unsigned i = 0; i < 3; ++i) {
        nominal[i] = x[i];
        reference[i] = y[i];
    }
    smu_linear_cal_t result;
    if (!smu_calibration_fit_linear(nominal, reference, 3, &result))
        return false;
    float errors[3];
    for (unsigned i = 0; i < 3; ++i) {
        errors[i] = reference[i] - (nominal[i] * result.gain + result.offset);
        if (!isfinite(errors[i]))
            return false;
    }
    *gain = result.gain;
    *offset = result.offset;
    for (unsigned i = 0; i < 3; ++i)
        residual[i] = errors[i];
    return true;
}

bool smu_calibration_voltage_measurement_commit(float gain, float offset) {
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
