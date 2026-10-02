#include "smu_measurement.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static smu_measurement_outputs_t out(void) {
    smu_measurement_outputs_t result;
    smu_measurement_get_outputs(&result);
    return result;
}

static void feed(int32_t code) {
    const ads131m03_frame_t f = {.ch = {code, code, code}, .crc_ok = true};
    assert(smu_measurement_process_frame(&f));
}

static void test_window_validity(void) {
    smu_measurement_init(NULL, NULL);
    smu_measurement_set_valid(true);
    for (unsigned i = 1; i <= 32; ++i) {
        feed(1000);
        assert(out().fast.valid);
        assert(out().precision_count == i && out().precision_window == 32);
        assert(out().precision.valid == (i == 32));
    }
    assert(!smu_measurement_set_precision_samples(0));
    assert(!smu_measurement_set_precision_samples(401));
    assert(out().precision_ready);
    const uint32_t count = out().sample_count;
    assert(smu_measurement_set_precision_samples(400));
    assert(!out().fast.valid && !out().precision.valid && out().precision_count == 0);
    assert(out().sample_count == count);
    for (unsigned i = 1; i <= 400; ++i) {
        feed(1000);
        assert(out().precision.valid == (i == 400));
    }
    smu_measurement_set_fresh(false);
    assert(!out().precision_ready && !out().precision.valid);
    smu_measurement_reset_filters();
    smu_measurement_set_fresh(true);
    feed(1000);
    assert(out().fast.valid && !out().precision.valid);
}

static void test_line_rejection(unsigned window, double frequency) {
    smu_measurement_init(NULL, NULL);
    smu_measurement_set_valid(true);
    assert(smu_measurement_set_precision_samples((uint16_t)window));
    const float volts_per_code = smu_voltage_from_adc(smu_ads_code_to_volts(1), SMU_VRANGE_15V);
    for (unsigned i = 0; i < window * 3; ++i) {
        const double phase = 0.37 + 6.283185307179586 * frequency * i / 4000.0;
        feed((int32_t)lround(1000000.0 + 500000.0 * sin(phase)));
        if (i + 1 >= window)
            assert(fabsf(out().precision.voltage_V / volts_per_code - 1000000.0f) < 3.0f);
    }
}

static void test_long_running_sum(void) {
    smu_measurement_init(NULL, NULL);
    smu_measurement_set_valid(true);
    assert(smu_measurement_set_precision_samples(400));
    int32_t history[400] = {0};
    int64_t sum = 0;
    const float volts_per_code = smu_voltage_from_adc(smu_ads_code_to_volts(1), SMU_VRANGE_15V);
    for (unsigned i = 0; i < 200000; ++i) {
        const int32_t code = 4000000 + (int32_t)((i * 7919u) % 2001u) - 1000;
        sum -= history[i % 400];
        history[i % 400] = code;
        sum += code;
        feed(code);
        if (i >= 399) {
            const double expected = (double)sum / 400;
            assert(fabs(out().precision.voltage_V / volts_per_code - expected) < 3.0);
        }
    }
}

int main(void) {
    test_window_validity();
    test_line_rejection(80, 50);
    test_line_rejection(200, 60);
    test_line_rejection(400, 50);
    test_line_rejection(400, 60);
    test_long_running_sum();
    puts("Precision windows, mains rejection and running-sum tests passed.");
}
