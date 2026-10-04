#include "ads131m03.h"
#include "ads131m03_port.h"
#include "range_hw_port.h"
#include "smu_acquisition.h"
#include "smu_cal_capture.h"
#include "smu_range.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t pending, clock_register;
static bool reject_ack, wrong_readback;
static unsigned writes;
static bool gates[6], v6, input_10m;

void range_hw_port_current_gate(smu_current_range_t r, bool on) {
    gates[r] = on;
}

bool range_hw_port_current_gate_is_on(smu_current_range_t r) {
    return gates[r];
}

void range_hw_port_voltage_6v(bool on) {
    v6 = on;
}

bool range_hw_port_voltage_6v_is_on(void) {
    return v6;
}

void range_hw_port_input_10m(bool on) {
    input_10m = on;
}

bool range_hw_port_input_10m_is_on(void) {
    return input_10m;
}

bool ads131m03_port_init(void) {
    return true;
}

void ads131m03_port_reset(bool asserted) {
    (void)asserted;
}

void ads131m03_port_delay_ms(uint32_t ms) {
    (void)ms;
}

bool ads131m03_port_wait_ready(uint32_t ms) {
    (void)ms;
    return true;
}

void ads131m03_port_cs_assert(void) {
}

void ads131m03_port_cs_deassert(void) {
}

bool ads131m03_port_spi_dma_start(const uint8_t* tx, uint8_t* rx, size_t n) {
    (void)tx;
    (void)rx;
    (void)n;
    return true;
}

bool ads131m03_port_transfer(const uint8_t* tx, uint8_t* rx, size_t n) {
    assert(n == 15);
    memset(rx, 0, n);
    rx[0] = (uint8_t)(pending >> 8);
    rx[1] = (uint8_t)pending;
    const uint16_t cmd = ((uint16_t)tx[0] << 8) | tx[1];
    assert(tx[2] == 0);
    if (cmd == 0x0011) {
        clock_register = 0x070e;
        pending = 0xff23;
    } else if (cmd == 0xa000)
        pending = 0x2300;
    else if (cmd == 0xa100)
        pending = 0x0510;
    else if (cmd == 0xa180)
        pending = wrong_readback ? 0xffff : clock_register;
    else if (cmd == 0x6180) {
        ++writes;
        clock_register = ((uint16_t)tx[3] << 8) | tx[4];
        assert(clock_register == (0x0702u | ADS131M03_CLOCK_OSR_BITS));
        assert(tx[5] == 0);
        pending = reject_ack ? 0 : 0x4180;
    } else {
        assert(cmd == 0);
        pending = 0;
    }
    return true;
}

static void test_configuration(void) {
    ads131m03_bringup_result_t result;
    assert(ads131m03_bringup_run(&result));
    assert(writes == 1 && result.clock_ok);
    reject_ack = true;
    assert(!ads131m03_bringup_run(&result));
    reject_ack = false;
    wrong_readback = true;
    assert(!ads131m03_bringup_run(&result));
}

static void test_observed_rate(void) {
    smu_acquisition_t acq;
    smu_acquisition_counters_t counters = {0};
    smu_acquisition_init(&acq, 0, counters);
    for (unsigned ms = 1; ms <= 1000; ++ms) {
        counters.frames += ADS131M03_SAMPLE_RATE_HZ / 1000;
        counters.drdy = counters.frames;
        smu_acquisition_update(&acq, ms, counters);
    }
    assert(acq.rate_known && acq.rate_ok && !acq.stale && !acq.fault);
    assert(acq.frame_hz == ADS131M03_SAMPLE_RATE_HZ);
    for (unsigned ms = 1001; ms <= 2000; ++ms) {
        counters.frames += ADS131M03_SAMPLE_RATE_HZ / 2000;
        counters.drdy += ADS131M03_SAMPLE_RATE_HZ / 1000;
        smu_acquisition_update(&acq, ms, counters);
    }
    assert(acq.rate_known && !acq.rate_ok && acq.frame_hz == ADS131M03_SAMPLE_RATE_HZ / 2);
    smu_acquisition_resume(&acq, 2100, counters);
    assert(!acq.rate_known);
    for (unsigned ms = 2101; ms <= 3100; ++ms) {
        counters.frames += ADS131M03_SAMPLE_RATE_HZ / 1000;
        smu_acquisition_update(&acq, ms, counters);
    }
    assert(acq.rate_known && acq.rate_ok);
}

static void test_capture_size(void) {
    smu_cal_capture_t capture;
    smu_cal_capture_quality_t quality;
    smu_cal_capture_config_t cfg = smu_cal_capture_default_config();
    assert(smu_cal_capture_init(&capture, SMU_CAL_CAPTURE_SAMPLES));
    for (unsigned i = 0; i < SMU_CAL_CAPTURE_SAMPLES; ++i)
        assert(smu_cal_capture_add(&capture, 5000000 + (i % 2 ? 1 : -1)));
    assert(smu_cal_capture_finish(&capture, &cfg, &quality));
    assert(quality.mean_code == 5000000 && quality.noise_codes == 1);
    assert(!smu_cal_capture_init(&capture, 2050));
    assert(smu_cal_capture_init(&capture, 2048));
    for (unsigned i = 0; i < 2048; ++i)
        assert(smu_cal_capture_add(&capture, i % 2 ? 8219999 : -8219999));
    assert(!smu_cal_capture_finish(&capture, &cfg, &quality));
}

static void test_settling_and_confirmation(void) {
    smu_range_manager_t rm;
    smu_range_init(&rm);
    assert(rm.cfg.current_discard_frames[SMU_RANGE_1P5A] == 8u * SMU_RATE_SCALE);
    assert(rm.cfg.current_discard_frames[SMU_RANGE_1MA] == 16u * SMU_RATE_SCALE);
    assert(rm.cfg.current_discard_frames[SMU_RANGE_100UA] == 40u * SMU_RATE_SCALE);
    assert(rm.cfg.resume_discard_frames == 40u * SMU_RATE_SCALE);
    assert(rm.cfg.current_up_confirm_frames == 2u * SMU_RATE_SCALE);
    assert(smu_range_request(&rm, SMU_RANGE_100UA, SMU_RANGE_REASON_USER));
    assert(smu_range_request_voltage(&rm, SMU_VRANGE_6V));
    smu_range_tick_ms(&rm, 0);
    for (unsigned i = 0; i < 40u * SMU_RATE_SCALE; ++i)
        assert(!smu_range_accept_frame(&rm));
    assert(smu_range_accept_frame(&rm));
    smu_range_set_voltage_autorange(&rm, true);
    for (unsigned i = 1; i < 2u * SMU_RATE_SCALE; ++i) {
        smu_range_voltage_autorange_frame(&rm, 0, 6.3f, 6.3f);
        assert(!rm.vpending);
    }
    smu_range_voltage_autorange_frame(&rm, 0, 6.3f, 6.3f);
    assert(rm.vpending && rm.vrequested == SMU_VRANGE_15V);
}

int main(void) {
    test_configuration();
    test_observed_rate();
    test_capture_size();
    test_settling_and_confirmation();
    puts("ADC register setup, observed rate and rate-aware capture tests passed.");
}
