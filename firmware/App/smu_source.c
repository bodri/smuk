#include "smu_source.h"
#include "ad5686.h"
#include "smu_iforce.h"
#include <math.h>

static uint16_t bipolar3_to_code(float x)
{
    if (x < -3.0f) x = -3.0f;
    if (x >  3.0f) x =  3.0f;
    float vdac = 0.5f * (x + 3.0f);
    long c = lroundf(vdac * (65535.0f / 3.0f));
    if (c < 0) c = 0;
    if (c > 65535) c = 65535;
    return (uint16_t)c;
}

static float inverse_cal(float physical, smu_source_cal_t c)
{
    return (fabsf(c.gain) > 1e-12f) ? ((physical - c.offset) / c.gain) : 0.0f;
}

bool smu_source_build_codes(const smu_source_request_t *r,
                            const smu_cal_record_t *cal,
                            smu_dac_codes_t *o)
{
    if (!r || !cal || !o) return false;
    if (r->current_range < SMU_RANGE_1P5A || r->current_range > SMU_RANGE_100UA)
        return false;

    unsigned ri = (unsigned)r->current_range - (unsigned)SMU_RANGE_1P5A;
    float vf  = inverse_cal(r->voltage_V, cal->vforce);
    float iforce_nominal;
    if (!smu_iforce_from_current(r->current_command, r->current_range, &iforce_nominal))
        return false;
    float ifc = inverse_cal(iforce_nominal, cal->iforce[ri]);

    /*
     * Analog limiter convention from the frozen controller:
     * high physical boundary -> negative LIM_HI_REF
     * low physical boundary  -> positive LIM_LO_REF.
     *
     * limit_*_domain are deliberately already in the +/-3 V limiter domain.
     * Exact physical A/V-to-limiter-domain mapping remains board calibration.
     */
    o->vforce     = bipolar3_to_code(vf);
    o->iforce     = bipolar3_to_code(ifc);
    o->lim_hi_ref = bipolar3_to_code(-r->limit_hi_domain);
    o->lim_lo_ref = bipolar3_to_code(-r->limit_lo_domain);
    return true;
}

bool smu_source_commit(const smu_dac_codes_t *c)
{
    if (!c) return false;
    bool ok = true;
    ok = ad5686_write_input(AD5686_CH_A, c->vforce) && ok;
    ok = ad5686_write_input(AD5686_CH_B, c->iforce) && ok;
    ok = ad5686_write_input(AD5686_CH_C, c->lim_hi_ref) && ok;
    ok = ad5686_write_input(AD5686_CH_D, c->lim_lo_ref) && ok;
    if (ok) ad5686_ldac_pulse();
    return ok;
}
