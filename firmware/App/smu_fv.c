#include "smu_fv.h"
#include "safety_hw.h"
#include "smu.h"
#include "smu_fv_port.h"
#include "smu_port.h"
#include <math.h>
#include <string.h>

static smu_fv_status_t status;
static smu_fv_config_t config;
static uint32_t last_ms, started_ms, last_poll_ms;
static unsigned stage;
static smu_current_range_t fixed_range;
static smu_voltage_range_t fixed_vrange;
static bool source_6v, config_valid;
static smu_fv_config_t base_config;
static float neutral[2];
static bool zero_valid[2];
static uint32_t feedback_ms, feedback_sample, zero_started, precision_ms;
static float zero_sum, zero_min, zero_max, candidate;
static unsigned zero_count;
#define CORRECTION_LIMIT_V 0.2f

static void reset_feedback(void) {
    status.correction_V = 0.0f;
    status.limited = false;
    feedback_ms = smu_port_millis();
    feedback_sample = 0;
    precision_ms = feedback_ms;
}

bool smu_fv_busy(void) {
    return status.state == SMU_FV_STARTING || status.state == SMU_FV_ACTIVE || status.state == SMU_FV_ZEROING;
}

static bool write_voltage(float voltage);

/* The selectable sense gain is also in the PA feedback path. Preserve the
 * trimmed DAC neutral; scale DUT gain and reachable limits together. */
static void sync_voltage_range(void) {
    if (!config_valid || smu_fv_busy())
        return;
    const bool range_6v = smu_get_context()->vrange == SMU_VRANGE_6V;
    if (range_6v == source_6v)
        return;
    source_6v = range_6v;
    config = base_config;
    const float scale = source_6v ? 0.4f : 1.0f;
    config.output_per_dac_V *= scale;
    config.zero_dac_V = neutral[source_6v];
    const float margin = config.dac_buffered ? 0.2f : 0.0f;
    const float a = (margin - config.zero_dac_V) * config.output_per_dac_V;
    const float b = (config.reference_V - margin - config.zero_dac_V) * config.output_per_dac_V;
    config.minimum_V = fmaxf(base_config.minimum_V * scale, fminf(a, b));
    config.maximum_V = fminf(base_config.maximum_V * scale, fmaxf(a, b));
    status.minimum_V = config.minimum_V;
    status.maximum_V = config.maximum_V;
    status.zero_valid = zero_valid[source_6v];
    status.requested_V = 0.0f;
    reset_feedback();
    if (status.state != SMU_FV_UNAVAILABLE && !write_voltage(0.0f))
        status.state = SMU_FV_TRIPPED;
}

static bool write_voltage(float voltage) {
    const float dac = config.zero_dac_V + voltage / config.output_per_dac_V;
    /* Buffer mode determines usable endpoints. Never silently clip. */
    const float margin = config.dac_buffered ? 0.2f : 0.0f;
    if (!isfinite(dac) || dac < margin || dac > config.reference_V - margin)
        return false;
    const uint16_t code = (uint16_t)lroundf(dac * 4095.0f / config.reference_V);
    if (!smu_fv_port_write(code))
        return false;
    status.code = code;
    status.applied_V = voltage;
    return true;
}

static bool shutdown(void) {
    /* Real hardware disables first. The always-enabled bench board can only
     * request neutral voltage; a failed DAC write leaves no shutdown path. */
    reset_feedback();
    bool ok = smu_fv_port_pa(false);
    if (config.pa_enable_stub)
        ok = write_voltage(0.0f) && ok;
    safety_hw_disable_pa();
    ok = smu_fv_port_relay(false) && ok;
    return ok;
}

void smu_fv_trip(void) {
    if (!smu_fv_busy())
        return;
    (void)shutdown();
    status.state = SMU_FV_TRIPPED;
}

void smu_fv_init(void) {
    memset(&status, 0, sizeof(status));
    memset(&config, 0, sizeof(config));
    source_6v = false;
    config_valid = false;
    memset(zero_valid, 0, sizeof(zero_valid));
    const bool ready = smu_fv_port_init(&config);
    status.pa_always_enabled = config.pa_enable_stub;
    status.relay_available = !config.relay_stub;
    status.minimum_V = config.minimum_V;
    status.maximum_V = config.maximum_V;
    status.slew_V_per_s = config.slew_V_per_s;
    if (!ready)
        return;
    const bool valid = isfinite(config.reference_V) && config.reference_V > 0.4f && isfinite(config.zero_dac_V) && isfinite(config.output_per_dac_V) && config.output_per_dac_V != 0.0f &&
                       isfinite(config.minimum_V) && isfinite(config.maximum_V) && config.minimum_V <= 0.0f && config.maximum_V >= 0.0f && config.minimum_V < config.maximum_V &&
                       isfinite(config.slew_V_per_s) && config.slew_V_per_s > 0.0f;
    if (!valid || !isfinite(config.zero_offset_V))
        return;
    config.zero_dac_V -= config.zero_offset_V / config.output_per_dac_V;
    base_config = config;
    config_valid = true;
    neutral[0] = neutral[1] = config.zero_dac_V;
    /* Force a refresh so initial bounds include the selected range and trim. */
    source_6v = smu_get_context()->vrange != SMU_VRANGE_6V;
    sync_voltage_range();
    if (config.minimum_V > 0.0f || config.maximum_V < 0.0f || config.minimum_V >= config.maximum_V || !shutdown() || !write_voltage(0.0f))
        return;
    status.state = SMU_FV_OFF;
    sync_voltage_range();
}

static bool healthy(void) {
    const smu_context_t* c = smu_get_context();
    return c->state == SMU_STATE_NORMAL && !c->faults && c->measurement_valid && c->measurement_fresh && c->measurement_settled && !c->acquisition_stale && c->adc_rate_known && c->adc_rate_ok &&
           !c->current_autorange && !c->voltage_autorange && !c->input_10m_active && !c->input_10m_requested && c->range >= SMU_RANGE_1P5A && c->range <= SMU_RANGE_100UA &&
           (c->vrange == SMU_VRANGE_15V || c->vrange == SMU_VRANGE_6V);
}

smu_status_t smu_fv_set_voltage(float voltage) {
    sync_voltage_range();
    if (status.state == SMU_FV_UNAVAILABLE)
        return SMU_ERR_HW;
    if (!isfinite(voltage) || voltage < config.minimum_V || voltage > config.maximum_V)
        return SMU_ERR_ARG;
    const float dac = config.zero_dac_V + voltage / config.output_per_dac_V;
    const float margin = config.dac_buffered ? 0.2f : 0.0f;
    if (!isfinite(dac) || dac < margin || dac > config.reference_V - margin)
        return SMU_ERR_ARG;
    if (status.state == SMU_FV_TRIPPED)
        return SMU_ERR_STATE;
    if (status.state == SMU_FV_ZEROING)
        return SMU_ERR_STATE;
    reset_feedback();
    status.requested_V = voltage;
    return SMU_OK;
}

smu_status_t smu_fv_output(bool enabled) {
    sync_voltage_range();
    if (status.state == SMU_FV_UNAVAILABLE)
        return SMU_ERR_HW;
    if (!enabled) {
        if (!shutdown()) {
            status.state = SMU_FV_TRIPPED;
            return SMU_ERR_HW;
        }
        status.requested_V = 0.0f;
        if (!write_voltage(0.0f)) {
            status.state = SMU_FV_TRIPPED;
            return SMU_ERR_HW;
        }
        status.state = SMU_FV_OFF;
        return SMU_OK;
    }
    if (status.state != SMU_FV_OFF || !healthy())
        return SMU_ERR_STATE;
    if (!write_voltage(0.0f) || !safety_hw_request_pa_enable() || !smu_fv_port_pa(true)) {
        (void)shutdown();
        status.state = SMU_FV_TRIPPED;
        return SMU_ERR_HW;
    }
    fixed_range = smu_get_context()->range;
    fixed_vrange = smu_get_context()->vrange;
    started_ms = last_ms = smu_port_millis();
    last_poll_ms = started_ms - 1u;
    stage = 0;
    status.state = SMU_FV_STARTING;
    return SMU_OK;
}

smu_status_t smu_fv_regulation(bool enabled) {
    if (status.state != SMU_FV_OFF)
        return SMU_ERR_STATE;
    status.regulation = enabled;
    reset_feedback();
    return SMU_OK;
}

smu_status_t smu_fv_zero(void) {
    sync_voltage_range();
    /* Only the connected, always-enabled bench hardware can observe OFF zero.
     * Real ENABLE/relay hardware needs a dedicated connected learning phase. */
    if (status.state != SMU_FV_OFF || !healthy() || !config.pa_enable_stub || !config.relay_stub)
        return SMU_ERR_STATE;
    status.requested_V = 0.0f;
    reset_feedback();
    if (!write_voltage(0.0f)) {
        status.state = SMU_FV_TRIPPED;
        return SMU_ERR_HW;
    }
    fixed_range = smu_get_context()->range;
    fixed_vrange = smu_get_context()->vrange;
    candidate = config.zero_dac_V;
    zero_started = feedback_ms = smu_port_millis();
    zero_count = 0;
    zero_sum = 0;
    zero_min = INFINITY;
    zero_max = -INFINITY;
    status.state = SMU_FV_ZEROING;
    return SMU_OK;
}

static void feedback(const smu_measurement_outputs_t* out, uint32_t now) {
    const bool learning = status.state == SMU_FV_ZEROING;
    const uint32_t wait_ms = 100u + (out->precision_window + 3u) / 4u;
    if (learning && now - zero_started > 60000u) {
        smu_fv_trip();
        return;
    }
    if (now - feedback_ms < wait_ms || out->sample_count == feedback_sample || !out->precision_ready || !out->precision.valid || !out->precision.fresh || !out->precision.settled ||
        out->precision.range_transition || out->precision.overload || out->precision.range != fixed_range || out->precision.vrange != fixed_vrange || !isfinite(out->precision.voltage_V))
        return;
    /* Average non-overlapping precision windows during zero learning. */
    if (now - precision_ms < (out->precision_window + 3u) / 4u)
        return;
    precision_ms = now;
    feedback_sample = out->sample_count;
    /* Freeze correction on reported compliance; no fixed bench current cutoff. */
    if (out->precision.compliance || out->fast.compliance) {
        status.limited = true;
        if (learning)
            smu_fv_trip();
        return;
    }
    const float lsb = fabsf(config.output_per_dac_V) * config.reference_V / 4095.0f;
    float error = status.requested_V - out->precision.voltage_V;
    if (learning) {
        zero_sum += out->precision.voltage_V;
        zero_min = fminf(zero_min, out->precision.voltage_V);
        zero_max = fmaxf(zero_max, out->precision.voltage_V);
        if (++zero_count < 16u)
            return;
        error = -zero_sum / (float)zero_count;
        const bool stable = zero_max - zero_min <= lsb;
        zero_count = 0;
        zero_sum = 0;
        zero_min = INFINITY;
        zero_max = -INFINITY;
        if (!stable) {
            feedback_ms = now;
            return;
        }
        if (fabsf(error) <= lsb * 0.65f) {
            neutral[source_6v] = candidate;
            zero_valid[source_6v] = true;
            status.state = SMU_FV_OFF;
            /* Restore cached config then refresh learned bounds without drift. */
            source_6v = !source_6v;
            sync_voltage_range();
            if (!write_voltage(0.0f))
                status.state = SMU_FV_TRIPPED;
            return;
        }
        if (fabsf(error) > CORRECTION_LIMIT_V) {
            smu_fv_trip();
            return;
        }
    } else if (fabsf(error) <= lsb * 0.65f) {
        status.limited = false;
        feedback_ms = now;
        return;
    }
    const float adjustment = fmaxf(-lsb, fminf(lsb, error * 0.5f));
    const float next = status.correction_V + adjustment;
    const float command = status.requested_V + next;
    if (fabsf(next) > CORRECTION_LIMIT_V || command < config.minimum_V || command > config.maximum_V) {
        status.limited = true;
        if (learning)
            smu_fv_trip();
        return;
    }
    if (!write_voltage(command)) {
        smu_fv_trip();
        return;
    }
    status.applied_V = status.requested_V;
    status.correction_V = next;
    status.limited = false;
    if (learning)
        candidate = config.zero_dac_V + next / config.output_per_dac_V;
    feedback_ms = now;
}

void smu_fv_process(void) {
    if (!smu_fv_busy())
        return;
    /* Setpoint/supervision work is bounded to 1 kHz; acquisition-gap and
     * instrument-fault hooks disable immediately without waiting for this tick. */
    const uint32_t now = smu_port_millis();
    if (now == last_poll_ms)
        return;
    last_poll_ms = now;
    smu_measurement_outputs_t out;
    smu_get_measurement(&out);
    const smu_context_t* c = smu_get_context();
    if (!healthy() || c->range != fixed_range || c->vrange != fixed_vrange || out.fast.overload || !isfinite(out.fast.current_A)) {
        smu_fv_trip();
        return;
    }
    if (status.state == SMU_FV_ZEROING) {
        feedback(&out, now);
        return;
    }
    if (status.state == SMU_FV_STARTING) {
        if (now - started_ms < 10u)
            return;
        if (!stage) {
            if (!smu_fv_port_relay(true)) {
                smu_fv_trip();
                return;
            }
            stage = 1;
            started_ms = now;
            return;
        }
        status.state = SMU_FV_ACTIVE;
        last_ms = now;
    }
    uint32_t elapsed = now - last_ms;
    if (!elapsed)
        return;
    last_ms = now;
    if (elapsed > 10u)
        elapsed = 10u;
    const float step = config.slew_V_per_s * (float)elapsed / 1000.0f;
    const float delta = status.requested_V - status.applied_V;
    const float next = fabsf(delta) <= step ? status.requested_V : status.applied_V + (delta > 0.0f ? step : -step);
    if (next != status.requested_V) {
        reset_feedback();
    } else if (status.applied_V == status.requested_V && status.regulation) {
        feedback(&out, now);
        return;
    } else {
        feedback_ms = now;
    }
    if (!write_voltage(next + status.correction_V))
        smu_fv_trip();
    else
        status.applied_V = next;
}

const smu_fv_status_t* smu_fv_status(void) {
    sync_voltage_range();
    return &status;
}
