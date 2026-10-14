#include "smu.h"
#include "smu_fv.h"
#include "smu_fv_port.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static smu_context_t ctx;
static smu_measurement_outputs_t measurement;
static uint32_t now;
static bool pa, relay, fail_write, fail_relay, available = true;
static uint16_t last_code;
static bool unipolar;

uint32_t smu_port_millis(void) {
    return now;
}

const smu_context_t* smu_get_context(void) {
    return &ctx;
}

void smu_get_measurement(smu_measurement_outputs_t* out) {
    *out = measurement;
}

void safety_hw_disable_pa(void) {
}

bool safety_hw_request_pa_enable(void) {
    return true;
}

bool smu_fv_port_init(smu_fv_config_t* c) {
    pa = relay = false;
    *c = (smu_fv_config_t){3.3f, 1.65f, 10.0f, -2.0f, 2.0f, 1.0f, 0.05f, true, false, false};
    if (unipolar)
        *c = (smu_fv_config_t){3.3f, 0.0f, 5.0f, 0.0f, 1.0f, 1.0f, 0.05f, false, true, true};
    return available;
}

bool smu_fv_port_write(uint16_t code) {
    last_code = code;
    return !fail_write;
}

bool smu_fv_port_pa(bool enabled) {
    pa = enabled;
    return true;
}

bool smu_fv_port_relay(bool enabled) {
    if (!enabled)
        assert(!pa);
    relay = enabled;
    return !fail_relay;
}

static void boot(void) {
    available = true;
    fail_write = fail_relay = false;
    now = 0;
    memset(&ctx, 0, sizeof(ctx));
    memset(&measurement, 0, sizeof(measurement));
    ctx.state = SMU_STATE_NORMAL;
    ctx.range = SMU_RANGE_1P5A;
    ctx.vrange = SMU_VRANGE_15V;
    ctx.measurement_valid = ctx.measurement_fresh = ctx.measurement_settled = true;
    ctx.adc_rate_known = ctx.adc_rate_ok = true;
    smu_fv_init();
    assert(smu_fv_status()->state == SMU_FV_OFF && !pa && !relay);
    assert(last_code == 2048);
}

static void start(void) {
    assert(smu_fv_output(true) == SMU_OK && pa && !relay);
    now += 9;
    smu_fv_process();
    assert(!relay);
    now += 1;
    smu_fv_process();
    assert(relay);
    now += 10;
    smu_fv_process();
    assert(smu_fv_status()->state == SMU_FV_ACTIVE);
}

int main(void) {
    boot();
    assert(smu_fv_set_voltage(NAN) == SMU_ERR_ARG);
    assert(smu_fv_set_voltage(2.01f) == SMU_ERR_ARG);
    ctx.current_autorange = true;
    assert(smu_fv_output(true) == SMU_ERR_STATE);
    ctx.current_autorange = false;
    ctx.input_10m_requested = true;
    assert(smu_fv_output(true) == SMU_ERR_STATE);
    ctx.input_10m_requested = false;
    ctx.adc_rate_known = false;
    assert(smu_fv_output(true) == SMU_ERR_STATE);
    ctx.adc_rate_known = true;
    ctx.range = SMU_RANGE_100MA;
    assert(smu_fv_output(true) == SMU_ERR_STATE);
    ctx.range = SMU_RANGE_1P5A;
    assert(smu_fv_set_voltage(1.0f) == SMU_OK);
    start();
    now += 10;
    smu_fv_process();
    assert(fabsf(smu_fv_status()->applied_V - 0.01f) < 1e-6f);
    now += 1000;
    smu_fv_process();
    assert(fabsf(smu_fv_status()->applied_V - 0.02f) < 1e-6f);
    assert(smu_fv_output(false) == SMU_OK && !pa && !relay && smu_fv_status()->requested_V == 0);
    for (unsigned reason = 0; reason < 7; ++reason) {
        boot();
        start();
        switch (reason) {
        case 0:
            ctx.acquisition_stale = true;
            break;
        case 1:
            ctx.faults = SMU_FAULT_ADC;
            break;
        case 2:
            ctx.vrange = SMU_VRANGE_6V;
            break;
        case 3:
            ctx.input_10m_active = true;
            break;
        case 4:
            measurement.fast.current_A = 0.051f;
            break;
        case 5:
            measurement.fast.overload = true;
            break;
        case 6:
            fail_write = true;
            now += 1;
            break;
        }
        ++now;
        smu_fv_process();
        assert(smu_fv_status()->state == SMU_FV_TRIPPED && !pa && !relay);
        assert(smu_fv_output(true) == SMU_ERR_STATE);
        fail_write = false;
        assert(smu_fv_output(false) == SMU_OK);
    }
    boot();
    fail_relay = true;
    assert(smu_fv_output(true) == SMU_OK);
    now += 10;
    smu_fv_process();
    assert(smu_fv_status()->state == SMU_FV_TRIPPED && !pa && !relay);
    boot();
    start();
    smu_fv_trip();
    assert(!pa && !relay);
    boot();
    available = false;
    smu_fv_init();
    assert(smu_fv_status()->state == SMU_FV_UNAVAILABLE);
    assert(smu_fv_output(true) == SMU_ERR_HW);
    assert(smu_fv_set_voltage(0) == SMU_ERR_HW);
    boot();
    now = UINT32_MAX - 5;
    start();
    assert(smu_fv_output(false) == SMU_OK);
    unipolar = true;
    smu_fv_init();
    assert(smu_fv_status()->state == SMU_FV_OFF && last_code == 0);
    assert(smu_fv_set_voltage(-0.01f) == SMU_ERR_ARG);
    assert(smu_fv_set_voltage(0.5f) == SMU_OK);
    start();
    now += 10;
    smu_fv_process();
    assert(last_code == (uint16_t)lroundf((smu_fv_status()->applied_V / 5.0f) * 4095.0f / 3.3f));
    assert(last_code > 0 && smu_fv_status()->applied_V > 0);
    assert(smu_fv_output(false) == SMU_OK && last_code == 0);
    assert(smu_fv_status()->pa_always_enabled);
    assert(smu_fv_set_voltage(0.5f) == SMU_OK);
    start();
    now += 10;
    smu_fv_process();
    assert(last_code > 0);
    smu_fv_trip();
    assert(last_code == 0 && smu_fv_status()->state == SMU_FV_TRIPPED);
    puts("FV sequencing, limits, slew, unavailable hardware and fault shutdown passed.");
}
