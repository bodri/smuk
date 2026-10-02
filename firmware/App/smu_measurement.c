#include "smu_measurement.h"
#include <string.h>

#define ADC_POS_FS_CODES 8388608.0f
#define ADC_FSR_V 1.2f
#define PREC_MAX 64u

typedef struct {
    smu_measurement_cal_t cal;
    smu_filter_config_t cfg;
    smu_current_range_t irange;
    smu_voltage_range_t vrange;
    bool valid, fresh, compliance, transition, overload;
    bool fast_started, has_sample, calbus_fast_started;
    bool pc_clipped[PREC_MAX];
    uint16_t pc_clipped_count;
    float fi, fv, fc;
    float pi[PREC_MAX], pv[PREC_MAX], pc[PREC_MAX];
    float si, sv, sc;
    uint16_t pidx, pcount;
    smu_measurement_outputs_t out;
} meas_ctx_t;

static meas_ctx_t g;

static float apply_cal(float x, smu_linear_cal_t c) {
    return x * c.gain + c.offset;
}

static int cal_i_index(smu_current_range_t r) {
    return (r >= SMU_RANGE_1P5A && r <= SMU_RANGE_100UA) ? (int)r - 1 : -1;
}

float smu_ads_code_to_volts(int32_t code) {
    return ((float)code * ADC_FSR_V) / ADC_POS_FS_CODES;
}

float smu_current_from_adc(float v, smu_current_range_t r) {
    /* Current AFE: shunt * 15, then /3 to ADC => ADC = I * Rshunt * 5. */
    static const float rsh[] = {0.0f, 0.1f, 2.0f, 20.0f, 200.0f, 2000.0f};
    if (r < SMU_RANGE_1P5A || r > SMU_RANGE_100UA)
        return 0.0f;
    return v / (5.0f * rsh[r]);
}

float smu_voltage_from_adc(float v, smu_voltage_range_t r) {
    /* Actual measured divider scales from traced Rev-A topology. */
    const float k15 = 0.1993594876f / 3.0f; /* R28/(R27+R28), then /3 */
    const float k6 = (0.1993594876f * 2.5f) / 3.0f;
    return v / ((r == SMU_VRANGE_6V) ? k6 : k15);
}

float smu_calbus_from_adc(float v) {
    return v * 3.0f;
}

static void refresh_quality(smu_measurement_t* m) {
    m->fresh = g.fresh && g.has_sample;
    m->settled = g.has_sample && !g.transition;
    m->range_transition = g.transition;
    m->compliance = g.compliance;
    m->current_overload = g.overload || m->current_clipped;
    m->voltage_overload = m->voltage_clipped;
    m->overload = m->current_overload || m->voltage_overload;
    m->valid = g.valid && g.fast_started && m->fresh && m->settled && !m->overload;
}

static void refresh_outputs(void) {
    refresh_quality(&g.out.fast);
    refresh_quality(&g.out.precision);
    g.out.precision_ready = g.out.precision.valid && g.pcount == g.cfg.precision_n;
}

static bool clipped(int32_t code) {
    return code >= 8220000 || code <= -8220000;
}

static void fill(smu_measurement_t* m, const ads131m03_frame_t* f, float i, float v, float c) {
    m->adc_i_raw = f->ch[0];
    m->adc_v_raw = f->ch[1];
    m->adc_cal_raw = f->ch[2];
    m->current_A = i;
    m->voltage_V = v;
    m->calbus_V = c;
    m->range = g.irange;
    m->vrange = g.vrange;
    m->current_clipped = clipped(f->ch[0]);
    m->voltage_clipped = clipped(f->ch[1]);
    m->calbus_clipped = clipped(f->ch[2]);
    refresh_quality(m);
}

void smu_measurement_init(const smu_measurement_cal_t* cal, const smu_filter_config_t* cfg) {
    memset(&g, 0, sizeof(g));
    g.irange = SMU_RANGE_10MA;
    g.vrange = SMU_VRANGE_15V;
    for (int i = 0; i < 5; i++)
        g.cal.current[i].gain = 1.0f;
    for (int i = 0; i < 2; i++)
        g.cal.voltage[i].gain = 1.0f;
    g.cal.calbus.gain = 1.0f;
    if (cal)
        g.cal = *cal;
    g.fresh = true; /* standalone caller supplies fresh frames; SMU monitors acquisition */
    g.cfg.fast_alpha = 0.25f;
    g.cfg.precision_n = 32u;
    if (cfg)
        g.cfg = *cfg;
    if (g.cfg.fast_alpha <= 0.0f || g.cfg.fast_alpha > 1.0f)
        g.cfg.fast_alpha = 0.25f;
    if (g.cfg.precision_n < 1u)
        g.cfg.precision_n = 1u;
    if (g.cfg.precision_n > PREC_MAX)
        g.cfg.precision_n = PREC_MAX;
}

void smu_measurement_set_calibration(const smu_measurement_cal_t* cal) {
    if (!cal)
        return;
    g.cal = *cal;
    smu_measurement_reset_filters();
}

void smu_measurement_reset_filters(void) {
    g.fast_started = g.has_sample = g.calbus_fast_started = false;
    memset(g.pc_clipped, 0, sizeof(g.pc_clipped));
    g.pc_clipped_count = 0;
    g.fi = g.fv = g.fc = 0.0f;
    memset(g.pi, 0, sizeof(g.pi));
    memset(g.pv, 0, sizeof(g.pv));
    memset(g.pc, 0, sizeof(g.pc));
    g.si = g.sv = g.sc = 0.0f;
    g.pidx = g.pcount = 0u;
    refresh_outputs();
}

void smu_measurement_set_current_range(smu_current_range_t r) {
    g.irange = r;
}

void smu_measurement_set_voltage_range(smu_voltage_range_t r) {
    g.vrange = r;
}

void smu_measurement_set_valid(bool v) {
    g.valid = v;
    refresh_outputs();
}

void smu_measurement_set_fresh(bool fresh) {
    g.fresh = fresh;
    refresh_outputs();
}

void smu_measurement_set_compliance(bool v) {
    g.compliance = v;
    refresh_outputs();
}

void smu_measurement_set_range_transition(bool v) {
    g.transition = v;
    refresh_outputs();
}

void smu_measurement_set_overload(bool v) {
    g.overload = v;
    refresh_outputs();
}

bool smu_measurement_process_frame(const ads131m03_frame_t* f) {
    if (!f || !f->crc_ok) {
        smu_measurement_reset_filters();
        smu_measurement_set_fresh(false);
        return false;
    }
    float av_i = smu_ads_code_to_volts(f->ch[0]), av_v = smu_ads_code_to_volts(f->ch[1]), av_c = smu_ads_code_to_volts(f->ch[2]);
    int ix = cal_i_index(g.irange);
    float i = smu_current_from_adc(av_i, g.irange);
    float v = smu_voltage_from_adc(av_v, g.vrange);
    float c = smu_calbus_from_adc(av_c);

    if (ix >= 0)
        i = apply_cal(i, g.cal.current[ix]);
    v = apply_cal(v, g.cal.voltage[g.vrange]);
    c = apply_cal(c, g.cal.calbus);
    if (clipped(f->ch[0]) || clipped(f->ch[1]) || g.overload) {
        /* Retain diagnostic values, but never blend clipped samples into filters. */
        smu_measurement_reset_filters();
        g.has_sample = true;
        fill(&g.out.fast, f, i, v, c);
        fill(&g.out.precision, f, i, v, c);
        g.out.sample_count++;
        return true;
    }
    if (!g.fast_started) {
        g.fi = i;
        g.fv = v;
        g.fast_started = true;
    } else {
        float a = g.cfg.fast_alpha;
        g.fi += a * (i - g.fi);
        g.fv += a * (v - g.fv);
    }
    if (clipped(f->ch[2])) {
        g.fc = c;
        g.calbus_fast_started = false;
    } else if (!g.calbus_fast_started) {
        g.fc = c;
        g.calbus_fast_started = true;
    } else
        g.fc += g.cfg.fast_alpha * (c - g.fc);
    g.has_sample = true;
    fill(&g.out.fast, f, g.fi, g.fv, g.fc);
    uint16_t n = g.cfg.precision_n, x = g.pidx;
    if (g.pcount == n) {
        g.si -= g.pi[x];
        g.sv -= g.pv[x];
        g.sc -= g.pc[x];
        if (g.pc_clipped[x])
            --g.pc_clipped_count;
    } else
        g.pcount++;
    g.pi[x] = i;
    g.pv[x] = v;
    g.pc[x] = c;
    g.pc_clipped[x] = clipped(f->ch[2]);
    if (g.pc_clipped[x])
        ++g.pc_clipped_count;
    g.si += i;
    g.sv += v;
    g.sc += c;
    g.pidx = (uint16_t)((x + 1u) % n);
    fill(&g.out.precision, f, g.si / g.pcount, g.sv / g.pcount, g.sc / g.pcount);
    g.out.precision.calbus_clipped = g.pc_clipped_count != 0;
    refresh_outputs();
    g.out.sample_count++;
    return true;
}

void smu_measurement_get_outputs(smu_measurement_outputs_t* out) {
    if (out)
        *out = g.out;
}
