#include "smu_calibration.h"
#include <math.h>

bool smu_calibration_fit_linear(const float* nominal, const float* reference, size_t count, smu_linear_cal_t* out) {
    if (!nominal || !reference || !out || count < 2u || count > 8u)
        return false;
    float mx = 0, my = 0, xx = 0, xy = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!isfinite(nominal[i]) || !isfinite(reference[i]))
            return false;
        mx += nominal[i];
        my += reference[i];
    }
    mx /= count;
    my /= count;
    for (size_t i = 0; i < count; ++i) {
        float dx = nominal[i] - mx;
        xx += dx * dx;
        xy += dx * (reference[i] - my);
    }
    if (!isfinite(xx) || xx <= 1e-20f)
        return false;
    smu_linear_cal_t coefficient = {.gain = xy / xx, .offset = my - (xy / xx) * mx};
    if (!isfinite(coefficient.gain) || coefficient.gain <= 0 || !isfinite(coefficient.offset))
        return false;
    *out = coefficient;
    return true;
}
