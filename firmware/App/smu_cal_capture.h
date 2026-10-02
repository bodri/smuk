#ifndef SMU_CAL_CAPTURE_H
#define SMU_CAL_CAPTURE_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float max_noise_codes, max_drift_codes;
    uint16_t minimum_fit_span_codes;
} smu_cal_capture_config_t;

typedef struct {
    uint16_t expected, count;
    int32_t anchor, minimum, maximum;
    int64_t sum, delta_sum, delta_squared_sum, first_half_sum;
} smu_cal_capture_t;

typedef struct {
    float mean_code, noise_codes, drift_codes, span_codes;
} smu_cal_capture_quality_t;

smu_cal_capture_config_t smu_cal_capture_default_config(void);
bool smu_cal_capture_init(smu_cal_capture_t* s, uint16_t samples);
/* Rejects clipped codes; caller aborts acquisition on failure. */
bool smu_cal_capture_add(smu_cal_capture_t* s, int32_t code);
/* Always reports quality for a complete capture, even if unstable. */
bool smu_cal_capture_finish(const smu_cal_capture_t* s, const smu_cal_capture_config_t* cfg, smu_cal_capture_quality_t* quality);
#endif
