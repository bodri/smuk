#include "calibration_store.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

static uint32_t crc32_calc(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;

    while (len--) {
        crc ^= *data++;

        for (uint32_t i = 0; i < 8U; i++) {
            if (crc & 1U)
                crc = (crc >> 1) ^ 0xEDB88320UL;
            else
                crc >>= 1;
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}

/* --------------------------------------------------------------------------
 * Record helpers
 * -------------------------------------------------------------------------- */

void smu_cal_record_defaults(smu_cal_record_t* r) {
    if (r == NULL)
        return;

    memset(r, 0, sizeof(*r));

    r->magic = SMU_CAL_MAGIC;
    r->version = SMU_CAL_VERSION;
    r->size = sizeof(*r);
    r->sequence = 0U;

    for (int i = 0; i < 5; i++) {
        r->measurement.current[i].gain = 1.0f;
        r->measurement.current[i].offset = 0.0f;
    }

    for (int i = 0; i < 2; i++) {
        r->measurement.voltage[i].gain = 1.0f;
        r->measurement.voltage[i].offset = 0.0f;
    }

    r->measurement.calbus.gain = 1.0f;
    r->measurement.calbus.offset = 0.0f;

    r->vforce.gain = 1.0f;
    r->vforce.offset = 0.0f;

    for (int i = 0; i < 5; i++) {
        r->iforce[i].gain = 1.0f;
        r->iforce[i].offset = 0.0f;
    }

    smu_cal_record_finalize(r);
}

void smu_cal_record_finalize(smu_cal_record_t* r) {
    if (r == NULL)
        return;

    r->magic = SMU_CAL_MAGIC;
    r->version = SMU_CAL_VERSION;
    r->size = sizeof(*r);

    r->crc32 = 0U;

    r->crc32 = crc32_calc((const uint8_t*)r, sizeof(*r));
}

static bool usable_source(smu_source_cal_t c, float full_scale) {
    return isfinite(c.gain) && c.gain > 0 && isfinite(c.offset) && isfinite((full_scale - c.offset) / c.gain) && isfinite((-full_scale - c.offset) / c.gain);
}

bool smu_cal_record_usable(const smu_cal_record_t* r) {
    if (!r || !smu_measurement_calibration_usable(&r->measurement) || !usable_source(r->vforce, 15.0f))
        return false;
    for (unsigned i = 0; i < SMU_CAL_IFORCE_RANGES; ++i)
        if (!usable_source(r->iforce[i], 3.0f))
            return false;
    return true;
}

bool smu_cal_record_validate(const smu_cal_record_t* r) {
    if (r == NULL)
        return false;

    if (r->magic != SMU_CAL_MAGIC)
        return false;

    if (r->version != SMU_CAL_VERSION)
        return false;

    if (r->size != sizeof(*r))
        return false;

    smu_cal_record_t temp = *r;

    uint32_t stored_crc = temp.crc32;

    temp.crc32 = 0U;

    uint32_t calc_crc = crc32_calc((const uint8_t*)&temp, sizeof(temp));

    return stored_crc == calc_crc && smu_cal_record_usable(r);
}
