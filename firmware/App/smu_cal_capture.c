#include "smu_cal_capture.h"
#include <math.h>

smu_cal_capture_config_t smu_cal_capture_default_config(void) {
    return (smu_cal_capture_config_t){.max_noise_codes = 4096.0f, .max_drift_codes = 8192.0f, .minimum_fit_span_codes = 64};
}

bool smu_cal_capture_init(smu_cal_capture_t* s, uint16_t samples) {
    if (!s || samples < 2 || samples > 2048 || samples % 2)
        return false;
    *s = (smu_cal_capture_t){.expected = samples};
    return true;
}

bool smu_cal_capture_add(smu_cal_capture_t* s, int32_t code) {
    if (!s || s->count >= s->expected || code >= 8220000 || code <= -8220000)
        return false;
    if (!s->count)
        s->anchor = s->minimum = s->maximum = code;
    const int64_t delta = (int64_t)code - s->anchor;
    /* Centered integer sums preserve small noise on large DC levels. For
     * <=2048 unclipped 24-bit samples, all accumulators fit signed 64 bits. */
    s->sum += code;
    s->delta_sum += delta;
    s->delta_squared_sum += delta * delta;
    if (s->count < s->expected / 2)
        s->first_half_sum += code;
    if (code < s->minimum)
        s->minimum = code;
    if (code > s->maximum)
        s->maximum = code;
    ++s->count;
    return true;
}

bool smu_cal_capture_finish(const smu_cal_capture_t* s, const smu_cal_capture_config_t* cfg, smu_cal_capture_quality_t* quality) {
    if (!s || !cfg || !quality || !s->expected || s->count != s->expected || !isfinite(cfg->max_noise_codes) || cfg->max_noise_codes <= 0 || !isfinite(cfg->max_drift_codes) ||
        cfg->max_drift_codes <= 0)
        return false;
    const float delta_mean = (float)s->delta_sum / s->count;
    const float variance = (float)s->delta_squared_sum / s->count - delta_mean * delta_mean;
    *quality = (smu_cal_capture_quality_t){.mean_code = (float)s->sum / s->count,
                                           .noise_codes = sqrtf(variance > 0 ? variance : 0),
                                           .drift_codes = (float)(s->sum - 2 * s->first_half_sum) / (s->count / 2),
                                           .span_codes = (float)((int64_t)s->maximum - s->minimum)};
    return quality->noise_codes <= cfg->max_noise_codes && fabsf(quality->drift_codes) <= cfg->max_drift_codes;
}
