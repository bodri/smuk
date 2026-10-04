#include "ads131m03_port.h"
#include "calibration_store.h"
#include "range_hw_port.h"
#include "safety_hw.h"
#include "smu.h"
#include "smu_calibration.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t now;
static ads131m03_dma_status_t adc;
static ads131m03_dma_frame_t queued[128];
static unsigned head, tail, capture_gaps;
static bool gates[6], v6, input_10m;
static bool drdy_enabled, force_busy, saved_record, save_failure, readback_failure;
static unsigned storage_writes;
static smu_cal_record_t stored_record;

uint32_t smu_port_millis(void) {
    return now;
}

bool ads131m03_port_spi_dma_busy(void) {
    if (force_busy)
        ++now;
    return force_busy;
}

void ads131m03_port_drdy_enable(bool enabled) {
    drdy_enabled = enabled;
}

bool ads131m03_bringup_run(ads131m03_bringup_result_t* out) {
    memset(out, 0, sizeof(*out));
    return true;
}

void ads131m03_dma_init(void) {
    memset(&adc, 0, sizeof(adc));
    head = tail = 0;
}

const ads131m03_dma_status_t* ads131m03_dma_get_status(void) {
    return &adc;
}

bool ads131m03_dma_pop(ads131m03_dma_frame_t* out) {
    if (tail == head)
        return false;
    *out = queued[tail++ % 128];
    return true;
}

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

void smu_cal_store_init(void) {
}

smu_cal_load_result_t smu_cal_store_load(smu_cal_record_t* out) {
    if (readback_failure)
        return SMU_CAL_LOAD_INVALID;
    if (saved_record) {
        *out = stored_record;
        return SMU_CAL_LOAD_OK;
    }
    smu_cal_record_defaults(out);
    return SMU_CAL_LOAD_DEFAULTS;
}

bool smu_cal_store_save(const smu_cal_record_t* r) {
    assert(!drdy_enabled && !adc.dma_active && head == tail);
    ++storage_writes;
    now += 1500; /* expected pause exceeds the normal stopped-acquisition limit */
    if (save_failure)
        return false;
    stored_record = *r;
    ++stored_record.sequence;
    smu_cal_record_finalize(&stored_record);
    saved_record = true;
    return true;
}

void smu_cal_debug_init(void) {
}

void smu_cal_debug_frame(const ads131m03_dma_frame_t* f) {
    (void)f;
}

void smu_cal_debug_process(uint32_t elapsed) {
    (void)elapsed;
}

bool smu_cal_debug_active(void) {
    return false;
}

bool smu_cal_debug_faulted(void) {
    return false;
}

void smu_cal_debug_acquisition_gap(void) {
}

void smu_console_acquisition_gap(void) {
    ++capture_gaps;
}

void smu_console_frame(const ads131m03_dma_frame_t* f, smu_current_range_t i, smu_voltage_range_t v) {
    (void)f;
    (void)i;
    (void)v;
}

static void enqueue(int32_t i, int32_t v, int32_t bus) {
    assert(head - tail < 128);
    queued[head++ % 128] = (ads131m03_dma_frame_t){.ch0 = i, .ch1 = v, .ch2 = bus};
    ++adc.frame_count;
}

static void sample(int32_t i, int32_t v, int32_t bus) {
    ++now;
    enqueue(i, v, bus);
    smu_process();
}

static smu_measurement_outputs_t output(void) {
    smu_measurement_outputs_t out;
    smu_get_measurement(&out);
    return out;
}

static void boot(void) {
    now = capture_gaps = storage_writes = 0;
    saved_record = force_busy = save_failure = readback_failure = false;
    safety_hw_init_safe();
    assert(smu_init());
    assert(output().precision_window == 8);
    /* The legacy health fixture exercises 8 ms after checking the boot default. */
    assert(smu_measurement_set_precision_samples(32));
    smu_set_current_autorange(false);
    smu_set_voltage_autorange(false);
    assert(!output().fast.valid);
    for (unsigned i = 0; i < 41; ++i)
        sample(1000, 1000, 1000);
    assert(output().fast.valid && smu_get_context()->measurement_valid);
    assert(!output().precision_ready);
}

static void test_flags_and_clipping(void) {
    boot();
    for (unsigned i = 0; i < 31; ++i)
        sample(1000, 1000, 1000);
    assert(output().precision_ready);
    smu_measurement_set_valid(false);
    assert(!output().precision_ready);
    assert(!output().fast.valid && !output().precision.valid);
    smu_measurement_set_valid(true);
    assert(output().fast.valid);
    smu_measurement_reset_filters();
    assert(!smu_get_context()->measurement_valid && !output().precision_ready);
    sample(1000, 1000, 1000);
    assert(smu_get_context()->measurement_valid);
    smu_measurement_set_compliance(true);
    assert(output().fast.compliance && output().precision.compliance);
    smu_measurement_set_range_transition(true);
    assert(!output().fast.valid && output().fast.range_transition && !output().fast.settled);
    smu_measurement_set_range_transition(false);
    assert(output().fast.valid);
    for (int sign = -1; sign <= 1; sign += 2) {
        sample(1000, sign * 8220000, 1000);
        smu_measurement_outputs_t out = output();
        assert(out.fast.fresh && out.fast.settled && !out.fast.valid);
        assert(out.fast.voltage_clipped && out.fast.voltage_overload && !out.fast.current_overload);
        assert(!out.precision.valid && smu_get_context()->voltage_overload);
        assert(smu_get_context()->vrange == SMU_VRANGE_15V); /* fixed range */
        sample(1000, 2000, 1000);
        assert(output().fast.valid && !output().fast.voltage_clipped);
        assert(output().fast.voltage_V == smu_voltage_from_adc(smu_ads_code_to_volts(2000), SMU_VRANGE_15V));
        sample(sign * 8220000, 1000, 1000);
        assert(!output().fast.valid && output().fast.current_clipped && output().fast.current_overload);
        sample(1000, 1000, 1000);
        assert(output().fast.valid && !output().fast.current_overload);
    }
    sample(6000000, 1000, 1000); /* exceeds 1.5 A nominal limit without ADC clipping */
    assert(output().fast.current_overload && !output().fast.current_clipped && !output().fast.valid);
    sample(1000, 1000, 8220000);
    assert(output().fast.valid && output().fast.calbus_clipped);
    sample(1000, 1000, 2000);
    assert(!output().fast.calbus_clipped && output().precision.calbus_clipped);
    assert(output().fast.calbus_V == smu_calbus_from_adc(smu_ads_code_to_volts(2000)));
    for (unsigned i = 0; i < 32; ++i)
        sample(1000, 1000, 2000);
    assert(!output().precision.calbus_clipped);
}

static void test_stale_and_recovery(void) {
    boot();
    for (unsigned i = 0; i < 19; ++i) {
        ++now;
        smu_process();
    }
    assert(output().fast.valid && smu_get_context()->measurement_age_ms == 19);
    ++now;
    smu_process();
    assert(!output().fast.valid && !output().precision.valid && !output().fast.fresh);
    assert(smu_get_context()->acquisition_stale && capture_gaps == 1);
    assert(!smu_get_context()->faults);
    sample(1000, 3000, 1000);
    assert(output().fast.valid && !smu_get_context()->acquisition_stale);
    assert(smu_get_context()->measurement_age_ms == 0);
    for (unsigned i = 0; i < 1000; ++i) {
        ++now;
        smu_process();
    }
    assert(smu_get_context()->state == SMU_STATE_FAULT && (smu_get_context()->faults & SMU_FAULT_ADC));
    assert(!output().fast.valid && !input_10m);
    sample(1000, 1000, 1000);
    assert(!output().fast.valid && !smu_get_context()->measurement_valid); /* latched */
}

static void test_gap_and_errors(void) {
    boot();
    enqueue(1000, 1000000, 1000);
    ++adc.crc_error_count;
    ++now;
    smu_process();
    assert(head == tail && !output().fast.valid && capture_gaps == 1);
    assert(smu_get_context()->adc_crc_errors == 1 && smu_get_context()->acquisition_gap_count == 1);
    sample(1000, 2000, 1000);
    assert(output().fast.valid);
    ++adc.ring_overrun_count;
    ++now;
    smu_process();
    assert(!output().fast.valid && smu_get_context()->adc_overruns == 1);
    sample(1000, 2000, 1000);
    ++adc.dma_busy_count;
    ++now;
    smu_process();
    assert(output().fast.valid && smu_get_context()->adc_busy_count == 1);
    sample(1000, 2000, 1000);
    ++adc.spi_error_count;
    ++now;
    smu_process();
    assert(!output().fast.valid && smu_get_context()->adc_spi_errors == 1);
    sample(1000, 2000, 1000);
    enqueue(1000, 1000000, 1000);
    now += 20;
    smu_process(); /* queued data of unknown age */
    assert(head == tail && !output().fast.valid);
    sample(1000, 2000, 1000);
    assert(output().fast.valid);
    adc.crc_error_count += 7;
    ++now;
    smu_process(); /* total ten transport errors */
    assert(smu_get_context()->faults & SMU_FAULT_ADC);
}

static void test_autorange_gap_persistence(void) {
    boot();
    smu_set_voltage_autorange(true);
    for (unsigned i = 0; i < 90; ++i)
        sample(1000, 1000, 1000);
    assert(smu_get_context()->vrange == SMU_VRANGE_15V);
    ++adc.crc_error_count;
    ++now;
    smu_process();
    for (unsigned i = 0; i < 99; ++i)
        sample(1000, 1000, 1000);
    assert(smu_get_context()->vrange == SMU_VRANGE_15V);
    sample(1000, 1000, 1000);
    sample(1000, 1000, 1000);
    assert(smu_get_context()->vrange == SMU_VRANGE_6V);
    assert(!output().fast.valid && output().fast.range_transition);
}

static void test_flash_save_interruptions(void) {
    boot();
    assert(smu_set_integration_ms(20) == SMU_OK);
    for (unsigned i = 0; i < 80; ++i)
        sample(1000, 1000, 1000);
    assert(output().precision.valid);
    smu_cal_record_t candidate = *smu_calibration_get();
    candidate.measurement.voltage[0].gain = 1.1f;
    enqueue(1000, 7000000, 1000); /* unread pre-save sample must be discarded */
    assert(smu_calibration_commit(&candidate));
    assert(storage_writes == 1 && head == tail && drdy_enabled);
    assert(!output().fast.valid && !output().precision.valid);
    assert(output().precision_window == 80 && smu_get_context()->acquisition_pause_count == 1);
    smu_process();
    assert(!smu_get_context()->faults && smu_get_context()->acquisition_stale);
    for (unsigned i = 0; i < 40; ++i)
        sample(1000, 2000, 1000);
    assert(!output().fast.valid);
    sample(1000, 2000, 1000);
    assert(output().fast.valid && !output().precision.valid);
    assert(fabsf(output().fast.voltage_V - 1.1f * smu_voltage_from_adc(smu_ads_code_to_volts(2000), SMU_VRANGE_15V)) < 1e-7f);
    for (unsigned i = 0; i < 79; ++i)
        sample(1000, 2000, 1000);
    assert(output().precision.valid && !smu_get_context()->faults);
    assert(!smu_get_context()->acquisition_gap_count);
    candidate = *smu_calibration_get();
    candidate.measurement.voltage[0].gain = 1.2f;
    save_failure = true;
    assert(!smu_calibration_commit(&candidate));
    assert(drdy_enabled && smu_calibration_get()->measurement.voltage[0].gain == 1.1f);
    for (unsigned i = 0; i < 41; ++i)
        sample(1000, 2000, 1000);
    assert(output().fast.valid && !smu_get_context()->faults);
    save_failure = false;
    readback_failure = true;
    assert(!smu_calibration_commit(&candidate));
    assert(drdy_enabled && smu_calibration_get()->measurement.voltage[0].gain == 1.1f);
    readback_failure = false;
    for (unsigned i = 0; i < 41; ++i)
        sample(1000, 2000, 1000);
    assert(output().fast.valid && !smu_get_context()->faults);
    force_busy = true;
    const unsigned writes_before = storage_writes;
    assert(!smu_calibration_commit(&candidate));
    assert(storage_writes == writes_before && !drdy_enabled);
    assert(smu_get_context()->faults & SMU_FAULT_ADC);
    assert(!output().fast.valid);
    force_busy = false;
}

static void test_failed_range_preserves_autorange(void) {
    boot();
    smu_set_current_autorange(true);
    assert(smu_set_current_range(SMU_RANGE_NONE) == SMU_ERR_ARG);
    assert(smu_get_context()->current_autorange);
    assert(smu_set_current_range(SMU_RANGE_10MA) == SMU_OK);
    smu_set_current_autorange(true);
    assert(smu_set_current_range(SMU_RANGE_1MA) == SMU_ERR_STATE);
    assert(smu_get_context()->current_autorange);
    assert(!output().fast.valid);
}

static void test_no_initial_frames(void) {
    now = 0;
    safety_hw_init_safe();
    assert(smu_init());
    assert(output().precision_window == 8);
    for (unsigned i = 0; i < 999; ++i) {
        ++now;
        smu_process();
    }
    assert(!smu_get_context()->measurement_valid && !smu_get_context()->faults);
    ++now;
    smu_process();
    assert(smu_get_context()->faults & SMU_FAULT_ADC);
    assert(!output().fast.valid && !input_10m);
}

static void test_health_wrap_and_windows(void) {
    smu_acquisition_t h;
    smu_acquisition_counters_t counters = {.frames = UINT32_MAX, .crc_errors = UINT32_MAX};
    smu_acquisition_init(&h, UINT32_MAX - 10, counters);
    counters.frames = 0;
    smu_acquisition_update(&h, UINT32_MAX - 9, counters);
    assert(!h.stale && !h.gap);
    smu_acquisition_update(&h, 9, counters);
    assert(h.age_ms == 19 && !h.stale);
    smu_acquisition_update(&h, 10, counters);
    assert(h.age_ms == 20 && h.stale && h.gap);
    counters.crc_errors = 0;
    ++counters.frames;
    smu_acquisition_update(&h, 11, counters);
    assert(h.window_errors == 1 && !h.fault);
    smu_acquisition_init(&h, 0, (smu_acquisition_counters_t){0});
    counters = (smu_acquisition_counters_t){0};
    for (uint32_t t = 1; t <= 2000; ++t) {
        ++counters.frames;
        counters.busy += 100;
        smu_acquisition_update(&h, t, counters);
        assert(!h.stale && !h.gap && !h.fault && h.window_errors == 0);
    }
    /* Busy interrupts without completed frames still fault as stopped ADC. */
    for (uint32_t t = 2001; t <= 3000; ++t) {
        ++counters.busy;
        smu_acquisition_update(&h, t, counters);
    }
    assert(h.stale && h.fault);
    smu_acquisition_init(&h, 0, (smu_acquisition_counters_t){0});
    counters = (smu_acquisition_counters_t){.frames = 1, .crc_errors = 9};
    smu_acquisition_update(&h, 1, counters);
    assert(!h.fault);
    for (uint32_t t = 2; t <= 1000; ++t) {
        ++counters.frames;
        smu_acquisition_update(&h, t, counters);
    }
    ++counters.crc_errors;
    smu_acquisition_update(&h, 1001, counters);
    assert(h.window_errors == 1 && !h.fault);
}

int main(void) {
    boot();
    assert(smu_set_integration_us(500) == SMU_OK);
    assert(output().precision_window == 2 && !output().precision.valid);
    assert(smu_set_integration_ms(2) == SMU_OK);
    assert(output().precision_window == 8);
    assert(smu_set_integration_ms(5) == SMU_OK);
    assert(output().precision_window == 20);
    assert(smu_set_integration_ms(10) == SMU_OK);
    assert(output().precision_window == 40);
    assert(smu_set_integration_us(501) == SMU_ERR_ARG);
    assert(output().precision_window == 40);
    test_flags_and_clipping();
    test_stale_and_recovery();
    test_gap_and_errors();
    test_autorange_gap_persistence();
    test_flash_save_interruptions();
    test_failed_range_preserves_autorange();
    saved_record = false;
    test_no_initial_frames();
    test_health_wrap_and_windows();
    puts("Measurement quality and acquisition health tests passed.");
}

/* These suites exercise the instrument without starting physical IWDG. */
bool smu_watchdog_port_was_reset(void) {
    return false;
}

bool smu_watchdog_port_start(void) {
    return true;
}

bool smu_watchdog_port_refresh(void) {
    return true;
}
