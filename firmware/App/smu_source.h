#ifndef SMU_SOURCE_H
#define SMU_SOURCE_H
#include <stdbool.h>
#include <stdint.h>
#include "smu_types.h"
#include "../Storage/calibration_store.h"

typedef struct {
    float voltage_V;
    float current_command; /* physical amperes; converted to range-relative IFORCE domain */
    float limit_hi_domain;
    float limit_lo_domain;
    smu_force_mode_t mode;
    smu_current_range_t current_range;
} smu_source_request_t;

typedef struct {
    uint16_t vforce, iforce, lim_hi_ref, lim_lo_ref;
} smu_dac_codes_t;

bool smu_source_build_codes(const smu_source_request_t *req,
                            const smu_cal_record_t *cal,
                            smu_dac_codes_t *out);
bool smu_source_commit(const smu_dac_codes_t *codes);
#endif
