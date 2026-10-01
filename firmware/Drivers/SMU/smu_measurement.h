#ifndef SMU_MEASUREMENT_H
#define SMU_MEASUREMENT_H
#include "../Drivers/SMU/ads131m03.h"
#include "smu_types.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum { SMU_VRANGE_15V = 0, SMU_VRANGE_6V } smu_voltage_range_t;

typedef struct {
    float gain;
    float offset;
} smu_linear_cal_t;

typedef struct {
    smu_linear_cal_t current[5]; /* indexed RANGE_1P5A..RANGE_100UA */
    smu_linear_cal_t voltage[2];
    smu_linear_cal_t calbus;
} smu_measurement_cal_t;

typedef struct {
    float fast_alpha;     /* EMA alpha, default 0.25 */
    uint16_t precision_n; /* boxcar length, default 32 */
} smu_filter_config_t;

typedef struct {
    smu_measurement_t fast;
    smu_measurement_t precision;
    uint32_t sample_count;
} smu_measurement_outputs_t;

void smu_measurement_init(const smu_measurement_cal_t* cal, const smu_filter_config_t* cfg);
/* Foreground only. Preserve ranges, flags, configuration and sample count;
 * reset filter history and invalidate cached outputs until the next frame. */
void smu_measurement_set_calibration(const smu_measurement_cal_t* cal);
void smu_measurement_set_current_range(smu_current_range_t range);
void smu_measurement_set_voltage_range(smu_voltage_range_t range);
void smu_measurement_set_valid(bool valid);
void smu_measurement_set_compliance(bool active);
void smu_measurement_set_range_transition(bool active);
bool smu_measurement_process_frame(const ads131m03_frame_t* frame);
void smu_measurement_get_outputs(smu_measurement_outputs_t* out);

/* Pure conversion helpers, useful for host tests. PGA=1, internal nominal 1.2 V ref. */
float smu_ads_code_to_volts(int32_t code);
float smu_current_from_adc(float adc_v, smu_current_range_t range);
float smu_voltage_from_adc(float adc_v, smu_voltage_range_t range);
float smu_calbus_from_adc(float adc_v);

#endif
