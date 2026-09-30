#include "smu_iforce.h"
#include <math.h>

float smu_current_range_fs_A(smu_current_range_t r) {
    switch(r) {
    case SMU_RANGE_1P5A:  return 1.5f;
    case SMU_RANGE_100MA: return 0.100f;
    case SMU_RANGE_10MA:  return 0.010f;
    case SMU_RANGE_1MA:   return 0.001f;
    case SMU_RANGE_100UA: return 0.000100f;
    default: return 0.0f;
    }
}
float smu_current_range_shunt_ohm(smu_current_range_t r) {
    switch(r) {
    case SMU_RANGE_1P5A:  return 0.1f;
    case SMU_RANGE_100MA: return 2.0f;
    case SMU_RANGE_10MA:  return 20.0f;
    case SMU_RANGE_1MA:   return 200.0f;
    case SMU_RANGE_100UA: return 2000.0f;
    default: return 0.0f;
    }
}
bool smu_iforce_from_current(float i, smu_current_range_t r, float *v) {
    float rs=smu_current_range_shunt_ohm(r);
    if (!v || rs<=0.0f) return false;
    *v=i*rs*15.0f;
    return (*v>=-3.00001f && *v<=3.00001f);
}
bool smu_current_from_iforce(float v, smu_current_range_t r, float *i) {
    float rs=smu_current_range_shunt_ohm(r);
    if (!i || rs<=0.0f) return false;
    *i=v/(rs*15.0f);
    return true;
}
bool smu_current_fits_range(float i, smu_current_range_t r, float frac) {
    float fs=smu_current_range_fs_A(r);
    return fs>0.0f && frac>0.0f && fabsf(i)<=fs*frac;
}
