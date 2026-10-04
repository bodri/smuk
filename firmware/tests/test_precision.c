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
    for (unsigned j = 0; j < SMU_PRECISION_GROUP_SAMPLES; ++j)
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

static void test_raw_groups_and_reset(void) {
    smu_measurement_init(NULL, NULL);
    smu_measurement_set_valid(true);
    assert(smu_measurement_set_precision_samples(80));
    const unsigned raw_window = 80u * SMU_PRECISION_GROUP_SAMPLES;
    for (unsigned i = 0; i < raw_window; ++i) {
        const ads131m03_frame_t f = {.ch = {(int32_t)i, (int32_t)i, (int32_t)i}, .crc_ok = true};
        assert(smu_measurement_process_frame(&f));
        assert(out().precision.valid == (i + 1 == raw_window));
    }
    const float expected = smu_voltage_from_adc((raw_window - 1) * 0.5f * (1.2f / 8388608.0f), SMU_VRANGE_15V);
    assert(fabsf(out().precision.voltage_V - expected) < 1e-7f);
    assert(out().sample_count == raw_window);
    smu_measurement_set_rate_valid(false);
    for (unsigned i = 0; i < 80; ++i)
        feed(1000);
    assert(out().fast.valid && !out().precision.valid);
    smu_measurement_set_rate_valid(true);
    assert(out().precision_count == 0);
    for (unsigned i = 0; i < 79; ++i)
        feed(1000);
    assert(!out().precision.valid);
    feed(1000);
    assert(out().precision.valid);
    /* A clipped sample anywhere in a partial group invalidates the history. */
    const ads131m03_frame_t clipped_frame = {.ch = {8220000, 0, 0}, .crc_ok = true};
    assert(smu_measurement_process_frame(&clipped_frame));
    assert(!out().precision.valid && out().precision_count == 0);
    for (unsigned i = 0; i < 79; ++i)
        feed(1000);
    assert(!out().precision.valid);
    feed(1000);
    assert(out().precision.valid);
    const ads131m03_frame_t bus_clip = {.ch = {0, 0, 8220000}, .crc_ok = true};
    assert(smu_measurement_process_frame(&bus_clip));
    for (unsigned j = 1; j < SMU_PRECISION_GROUP_SAMPLES; ++j) {
        const ads131m03_frame_t clean = {.crc_ok = true};
        assert(smu_measurement_process_frame(&clean));
    }
    assert(out().precision.calbus_clipped);
    for (unsigned i = 0; i < 80; ++i)
        feed(1000);
    assert(!out().precision.calbus_clipped);
}

static void test_history_not_reused_after_window_changes(void) {
    smu_measurement_init(NULL, NULL);
    smu_measurement_set_valid(true);
    const unsigned sizes[] = {400, 4, 200, 80, 400};
    for (unsigned trial = 0; trial < sizeof(sizes) / sizeof(sizes[0]); ++trial) {
        const unsigned n = sizes[trial];
        const int32_t code = (int32_t)(100000 + trial * 20000);
        assert(smu_measurement_set_precision_samples((uint16_t)n));
        smu_measurement_reset_filters();
        for (unsigned i = 0; i < n; ++i) {
            feed(code);
            assert(out().precision.valid == (i + 1 == n));
        }
        const float expected = smu_voltage_from_adc(smu_ads_code_to_volts(code), SMU_VRANGE_15V);
        assert(fabsf(out().precision.voltage_V - expected) < 1e-6f);
        /* Repeated clipping invalidates history; no stale sum/clip flag survives. */
        for (unsigned i = 0; i < 100; ++i) {
            const ads131m03_frame_t clipped_frame = {.ch = {8220000, 0, 0}, .crc_ok = true};
            assert(smu_measurement_process_frame(&clipped_frame));
        }
        for (unsigned i = 0; i < n; ++i)
            feed(-code);
        assert(out().precision.valid && !out().precision.calbus_clipped);
        assert(fabsf(out().precision.voltage_V + expected) < 1e-6f);
    }
}

int main(void) {
    test_window_validity();
    test_line_rejection(80, 50);
    test_line_rejection(200, 60);
    test_line_rejection(400, 50);
    test_line_rejection(400, 60);
    test_long_running_sum();
    test_raw_groups_and_reset();
    test_history_not_reused_after_window_changes();
    puts("Precision windows, mains rejection and running-sum tests passed.");
}
