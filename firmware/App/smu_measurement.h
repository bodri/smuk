#ifndef SMU_MEASUREMENT_H
#define SMU_MEASUREMENT_H
#include "ads131m03.h"
#include "smu_types.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float gain;
    float offset;
} smu_linear_cal_t;

typedef struct {
    smu_linear_cal_t current[5]; /* indexed RANGE_1P5A..RANGE_100UA */
    smu_linear_cal_t voltage[2];
    smu_linear_cal_t calbus;
} smu_measurement_cal_t;

#define SMU_MEASUREMENT_SAMPLE_RATE_HZ ADS131M03_SAMPLE_RATE_HZ
#define SMU_PRECISION_MAX_SAMPLES 400u

typedef struct {
    float fast_alpha;     /* EMA alpha, default 0.25 */
    uint16_t precision_n; /* 4 kHz group window, default 32; includes every raw sample */
} smu_filter_config_t;

typedef struct {
    smu_measurement_t fast;
    smu_measurement_t precision;
    uint32_t sample_count;
    uint16_t precision_count, precision_window;
    bool precision_ready; /* full configured window of valid I/V samples */
} smu_measurement_outputs_t;

void smu_measurement_init(const smu_measurement_cal_t* cal, const smu_filter_config_t* cfg);
/* Foreground only. Preserve ranges, flags, configuration and sample count;
 * reset filter history and invalidate cached outputs until the next frame. */
bool smu_measurement_calibration_usable(const smu_measurement_cal_t* cal);
bool smu_measurement_set_calibration(const smu_measurement_cal_t* cal);
/* Foreground only. Drop filter history and invalidate cached outputs, e.g.
 * after a range switch, so samples from different ranges are never mixed. */
void smu_measurement_reset_filters(void);
/* Validated foreground configuration; resets history, preserves sample count. */
bool smu_measurement_set_precision_samples(uint16_t samples);
void smu_measurement_set_current_range(smu_current_range_t range);
void smu_measurement_set_voltage_range(smu_voltage_range_t range);
void smu_measurement_set_valid(bool valid);
/* Foreground acquisition monitor invalidates immediately on stale/gapped data. */
void smu_measurement_set_fresh(bool fresh);
/* Rate mismatch inhibits precision only; fast diagnostics/autorange remain available. */
void smu_measurement_set_rate_valid(bool valid);
void smu_measurement_set_compliance(bool active);
void smu_measurement_set_range_transition(bool active);
void smu_measurement_set_overload(bool active);
bool smu_measurement_process_frame(const ads131m03_frame_t* frame);
/* valid requires enabled, fresh, settled and no I/V overload. Precision also
 * requires a complete configured window. CALBUS quality is independent. */
void smu_measurement_get_outputs(smu_measurement_outputs_t* out);

/* Pure conversion helpers, useful for host tests. PGA=1, internal nominal 1.2 V ref. */
float smu_ads_code_to_volts(int32_t code);
float smu_current_from_adc(float adc_v, smu_current_range_t range);
float smu_voltage_from_adc(float adc_v, smu_voltage_range_t range);
float smu_calbus_from_adc(float adc_v);
/* Active measurement calibration, for instantaneous autorange decisions. */
float smu_measurement_current_from_code(int32_t code, smu_current_range_t range);
float smu_measurement_voltage_from_code(int32_t code, smu_voltage_range_t range);

#endif
