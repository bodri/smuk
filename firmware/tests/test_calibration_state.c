#include "range_hw.h"
#include "safety_hw.h"
#include "smu_cal_seq.h"
#include "smu_calibration.h"
#include "smu_measurement.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

/* RAM persistence stub: exercise the actual calibration application path. */
static smu_cal_record_t stored;

bool smu_cal_store_save(const smu_cal_record_t* record) {
    stored = *record;
    ++stored.sequence;
    smu_cal_record_finalize(&stored);
    return true;
}

smu_cal_load_result_t smu_cal_store_load(smu_cal_record_t* out) {
    *out = stored;
    return SMU_CAL_LOAD_OK;
}

static bool voltage_relay, current_relay;
static calbus_sel_t bus;
static unsigned disable_count, off_count;

void safety_hw_disable_pa(void) {
    ++disable_count;
}

void range_hw_all_off(void) {
    ++off_count;
}

void cal_hw_select_bus(calbus_sel_t value) {
    bus = value;
}

void cal_hw_voltage_relay(bool on) {
    voltage_relay = on;
}

void cal_hw_current_inject_relay(bool on) {
    current_relay = on;
}

bool cal_hw_pa_interlock_ok(void) {
    return true;
}

static void start(smu_cal_seq_t* s, smu_cal_target_t target) {
    smu_cal_seq_init(s);
    s->acquisition_timeout_ms = 10;
    assert(smu_cal_seq_start(s, target, CALBUS_P1V5));
    while (s->state != CAL_SEQ_DISCARD)
        smu_cal_seq_tick_1ms(s);
}

static void assert_fault(const smu_cal_seq_t* s) {
    assert(s->state == CAL_SEQ_FAULT && s->fault && !s->result_ready);
    assert(!s->measurement_valid && !s->servo_allowed);
    assert(!voltage_relay && !current_relay && bus == CALBUS_0V);
    assert(disable_count && off_count);
}

static void test_timeout(void) {
    smu_cal_seq_t s;
    start(&s, CAL_TARGET_VOLTAGE);
    assert(voltage_relay);
    smu_cal_seq_tick_elapsed_ms(&s, 9);
    assert(s.state == CAL_SEQ_DISCARD && voltage_relay);
    smu_cal_seq_tick_1ms(&s);
    assert_fault(&s);
    assert(!smu_cal_seq_start(&s, CAL_TARGET_VOLTAGE, CALBUS_0V));
    start(&s, CAL_TARGET_CURRENT);
    for (unsigned i = 0; i < s.discard_required; ++i)
        smu_cal_seq_adc_frame(&s, 0, 0);
    smu_cal_seq_tick_1ms(&s);
    assert(s.state == CAL_SEQ_ACQUIRE && current_relay);
    smu_cal_seq_adc_frame(&s, 100, 50);
    smu_cal_seq_tick_elapsed_ms(&s, 1000);
    assert_fault(&s);
    start(&s, CAL_TARGET_VOLTAGE);
    for (unsigned i = 0; i < s.discard_required; ++i)
        smu_cal_seq_adc_frame(&s, 0, 0);
    smu_cal_seq_tick_1ms(&s);
    for (unsigned i = 0; i < s.acquire_required; ++i)
        smu_cal_seq_adc_frame(&s, 100, 0);
    smu_cal_seq_tick_1ms(&s);
    assert(s.result_ready && !s.fault && s.target_average == 100 && s.ratio == 0);
    while (s.state != CAL_SEQ_IDLE)
        smu_cal_seq_tick_1ms(&s);
    assert(!voltage_relay && !current_relay && bus == CALBUS_0V);
    start(&s, CAL_TARGET_VOLTAGE);
    s.discarded = s.discard_required;
    smu_cal_seq_tick_1ms(&s);
    s.acquire_required = 0;
    smu_cal_seq_tick_1ms(&s);
    assert_fault(&s);
    start(&s, CAL_TARGET_VOLTAGE);
    smu_cal_seq_abort(&s);
    assert_fault(&s);
}

static void close_to(float actual, float expected) {
    assert(fabsf(actual - expected) < 0.0001f);
}

static void test_measurement(void) {
    smu_filter_config_t cfg = {.fast_alpha = 0.5f, .precision_n = 2};
    smu_measurement_init(NULL, &cfg);
    smu_measurement_set_current_range(SMU_RANGE_100UA);
    smu_measurement_set_voltage_range(SMU_VRANGE_6V);
    smu_measurement_set_valid(true);
    smu_measurement_set_compliance(true);
    ads131m03_frame_t frame = {.ch = {1000, 1000, 1000}, .crc_ok = true};
    assert(smu_measurement_process_frame(&frame));
    smu_measurement_cal_t cal = {0};
    for (unsigned i = 0; i < 5; ++i)
        cal.current[i].gain = 2;
    for (unsigned i = 0; i < 2; ++i)
        cal.voltage[i].gain = 3;
    cal.calbus.gain = 4;
    smu_cal_record_t candidate;
    smu_cal_record_defaults(&candidate);
    candidate.measurement = cal;
    assert(smu_calibration_commit(&candidate));
    smu_measurement_outputs_t out;
    smu_measurement_get_outputs(&out);
    assert(out.sample_count == 1 && !out.fast.valid && !out.precision.valid);
    assert(smu_measurement_process_frame(&frame));
    smu_measurement_get_outputs(&out);
    float v = smu_ads_code_to_volts(1000);
    float expected_v = 3 * smu_voltage_from_adc(v, SMU_VRANGE_6V);
    assert(out.sample_count == 2 && out.fast.range == SMU_RANGE_100UA);
    assert(out.fast.valid && out.fast.compliance);
    close_to(out.fast.current_A, 2 * smu_current_from_adc(v, SMU_RANGE_100UA));
    close_to(out.fast.voltage_V, expected_v);
    close_to(out.precision.voltage_V, expected_v);
    frame.ch[1] = 2000;
    assert(smu_measurement_process_frame(&frame));
    frame.ch[1] = 3000;
    assert(smu_measurement_process_frame(&frame));
    smu_measurement_get_outputs(&out);
    close_to(out.fast.voltage_V, 2.25f * expected_v);
    close_to(out.precision.voltage_V, 2.5f * expected_v);
    smu_measurement_set_range_transition(true);
    smu_measurement_set_calibration(&cal);
    assert(smu_measurement_process_frame(&frame));
    smu_measurement_get_outputs(&out);
    assert(out.sample_count == 5 && out.fast.range_transition && !out.fast.valid);
    assert(out.fast.compliance);
}

int main(void) {
    test_timeout();
    test_measurement();
    puts("Calibration timeout and measurement state tests passed.");
}
