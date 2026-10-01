#ifndef SMU_IFORCE_H
#define SMU_IFORCE_H
#include "smu_types.h"
#include <stdbool.h>

/*
 * Range-relative Force-I mapping.
 * The analog current loop compares IFORCE against the +/-3 V IFB domain.
 * Therefore command_domain_V = I_requested * Rshunt * 15.
 */
float smu_current_range_fs_A(smu_current_range_t r);
float smu_current_range_shunt_ohm(smu_current_range_t r);
bool smu_iforce_from_current(float current_A, smu_current_range_t r, float* domain_V);
bool smu_current_from_iforce(float domain_V, smu_current_range_t r, float* current_A);
bool smu_current_fits_range(float current_A, smu_current_range_t r, float fraction_fs);
#endif
