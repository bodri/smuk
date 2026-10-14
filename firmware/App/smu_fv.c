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

bool smu_fv_busy(void) {
    return status.state == SMU_FV_STARTING || status.state == SMU_FV_ACTIVE;
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
    const bool ready = smu_fv_port_init(&config);
    status.pa_always_enabled = config.pa_enable_stub;
    if (!ready)
        return;
    const bool valid = isfinite(config.reference_V) && config.reference_V > 0.4f && isfinite(config.zero_dac_V) && isfinite(config.output_per_dac_V) && config.output_per_dac_V != 0.0f &&
                       isfinite(config.minimum_V) && isfinite(config.maximum_V) && config.minimum_V <= 0.0f && config.maximum_V >= 0.0f && config.minimum_V < config.maximum_V &&
                       isfinite(config.slew_V_per_s) && config.slew_V_per_s > 0.0f && isfinite(config.maximum_current_A) && config.maximum_current_A > 0.0f;
    if (!valid || !shutdown() || !write_voltage(0.0f))
        return;
    status.state = SMU_FV_OFF;
}

static bool healthy(void) {
    const smu_context_t* c = smu_get_context();
    return c->state == SMU_STATE_NORMAL && !c->faults && c->measurement_valid && c->measurement_fresh && c->measurement_settled && !c->acquisition_stale && c->adc_rate_known && c->adc_rate_ok &&
           !c->current_autorange && !c->voltage_autorange && !c->input_10m_active && !c->input_10m_requested && c->range == SMU_RANGE_1P5A;
}

smu_status_t smu_fv_set_voltage(float voltage) {
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
    status.requested_V = voltage;
    return SMU_OK;
}

smu_status_t smu_fv_output(bool enabled) {
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
    if (!healthy() || c->range != fixed_range || c->vrange != fixed_vrange || out.fast.overload || !isfinite(out.fast.current_A) || fabsf(out.fast.current_A) > config.maximum_current_A) {
        smu_fv_trip();
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
    if (!write_voltage(next))
        smu_fv_trip();
}

const smu_fv_status_t* smu_fv_status(void) {
    return &status;
}
