#include "range_hw.h"
#include "range_hw_port.h"
#include "smu_measurement.h"
#include "smu_range.h"
#include <stdio.h>
#include <stdlib.h>

/* Fake GPIO port: logical switch states. */
static bool gate[6];
static bool v6;

void range_hw_port_current_gate(smu_current_range_t r, bool on) {
    if (r >= SMU_RANGE_1P5A && r <= SMU_RANGE_100UA)
        gate[r] = on;
}
bool range_hw_port_current_gate_is_on(smu_current_range_t r) {
    return r >= SMU_RANGE_1P5A && r <= SMU_RANGE_100UA && gate[r];
}
void range_hw_port_voltage_6v(bool on) {
    v6 = on;
}
bool range_hw_port_voltage_6v_is_on(void) {
    return v6;
}

static int failures;
#define CHECK(c)                                                                                                                                                                                       \
    do {                                                                                                                                                                                               \
        if (!(c)) {                                                                                                                                                                                    \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                                                                                                        \
            failures++;                                                                                                                                                                                \
        }                                                                                                                                                                                              \
    } while (0)

/* Nominal CH0 code for a current in a range: ADC = I * Rshunt * 5. */
static int32_t code_for(float amps, smu_current_range_t r) {
    static const float rsh[] = {0.0f, 0.1f, 2.0f, 20.0f, 200.0f, 2000.0f};
    double c = (double)amps * rsh[r] * 5.0 / 1.2 * 8388608.0;
    if (c > 8388607.0)
        c = 8388607.0;
    return (int32_t)c;
}

static int one_hot_count(void) {
    int n = 0;
    for (int i = 1; i <= 5; i++)
        n += gate[i];
    return n;
}

/* Feed n frames of a constant current; filtered value = the same current. */
static void feed(smu_range_manager_t* rm, float amps, int n) {
    for (int i = 0; i < n; i++) {
        if (smu_range_accept_frame(rm))
            smu_range_autorange_frame(rm, code_for(amps, rm->active), amps);
        smu_range_tick_ms(rm, 0u);
    }
}

/* Run for ms milliseconds at 4 frames/ms. */
static void run(smu_range_manager_t* rm, float amps, int ms) {
    for (int t = 0; t < ms; t++) {
        feed(rm, amps, 4);
        smu_range_tick_ms(rm, 1u);
    }
}

static void boot(smu_range_manager_t* rm) {
    smu_measurement_init(NULL, NULL);
    smu_range_init(rm);
    CHECK(smu_range_request(rm, SMU_RANGE_1P5A, SMU_RANGE_REASON_USER));
    CHECK(smu_range_request_voltage(rm, SMU_VRANGE_15V));
    smu_range_tick_ms(rm, 0u);
    smu_range_set_autorange(rm, true);
}

static void test_boot_and_settle(void) {
    smu_range_manager_t rm;
    v6 = true; /* CubeMX may boot VRANGE high */
    boot(&rm);
    CHECK(rm.active == SMU_RANGE_1P5A);
    CHECK(gate[SMU_RANGE_1P5A] && one_hot_count() == 1);
    CHECK(!v6 && rm.vactive == SMU_VRANGE_15V);
    CHECK(rm.tx_state == SMU_RANGE_TX_SETTLE);
    /* max(8 current, 40 voltage) frames dropped, then accepted */
    int dropped = 0;
    while (!smu_range_accept_frame(&rm))
        dropped++;
    CHECK(dropped == 40);
    CHECK(rm.tx_state == SMU_RANGE_TX_IDLE && rm.measurement_valid);
}

static void test_down_jumps_directly(void) {
    smu_range_manager_t rm;
    boot(&rm);
    run(&rm, 50e-6f, 20); /* settle + less than persistence */
    CHECK(rm.active == SMU_RANGE_1P5A);
    run(&rm, 50e-6f, 60);
    CHECK(rm.active == SMU_RANGE_100UA); /* 1.5 A -> 100 uA in one switch */
    CHECK(rm.switch_count == 2u);        /* boot + one down */
    CHECK(one_hot_count() == 1 && gate[SMU_RANGE_100UA]);
}

static void test_up_to_smallest_fit(void) {
    smu_range_manager_t rm;
    boot(&rm);
    run(&rm, 50e-6f, 100);
    CHECK(rm.active == SMU_RANGE_100UA);
    run(&rm, 95e-6f, 5); /* above 90% of 100 uA */
    CHECK(rm.active == SMU_RANGE_1MA);
    run(&rm, 5e-3f, 20); /* 5 mA in the 1 mA range saturates */
    CHECK(rm.active == SMU_RANGE_1P5A || rm.active == SMU_RANGE_10MA);
    run(&rm, 5e-3f, 100);
    CHECK(rm.active == SMU_RANGE_10MA); /* 5 mA < 70% of 10 mA */
}

static void test_saturation_goes_to_top(void) {
    smu_range_manager_t rm;
    boot(&rm);
    run(&rm, 50e-6f, 100);
    CHECK(rm.active == SMU_RANGE_100UA);
    const uint32_t before = rm.switch_count;
    feed(&rm, 0.5f, 1); /* one saturated frame */
    smu_range_tick_ms(&rm, 1u);
    CHECK(rm.active == SMU_RANGE_1P5A);
    CHECK(rm.switch_count == before + 1u);
    CHECK(rm.reason == SMU_RANGE_REASON_OVERLOAD);
}

static void test_single_spike_ignored(void) {
    smu_range_manager_t rm;
    boot(&rm);
    run(&rm, 50e-6f, 100);
    CHECK(rm.active == SMU_RANGE_100UA);
    feed(&rm, 95e-6f, 1); /* one frame above 90%, not saturated */
    feed(&rm, 50e-6f, 1);
    smu_range_tick_ms(&rm, 1u);
    CHECK(rm.active == SMU_RANGE_100UA);
}

static void test_hysteresis_band_stable(void) {
    smu_range_manager_t rm;
    boot(&rm);
    run(&rm, 0.5e-3f, 100);
    CHECK(rm.active == SMU_RANGE_1MA);
    const uint32_t before = rm.switch_count;
    run(&rm, 80e-6f, 500); /* between 70% of 100 uA and 90% of 1 mA */
    CHECK(rm.active == SMU_RANGE_1MA);
    CHECK(rm.switch_count == before);
}

static void test_manual_disables_autorange(void) {
    smu_range_manager_t rm;
    boot(&rm);
    smu_range_set_autorange(&rm, false);
    run(&rm, 50e-6f, 200);
    CHECK(rm.active == SMU_RANGE_1P5A);
    CHECK(smu_range_request(&rm, SMU_RANGE_10MA, SMU_RANGE_REASON_USER));
    run(&rm, 50e-3f, 10); /* overloads 10 mA, but no autorange */
    CHECK(rm.active == SMU_RANGE_10MA);
    CHECK(rm.overload);
}

static void test_reassert_after_all_off(void) {
    smu_range_manager_t rm;
    boot(&rm);
    run(&rm, 5e-3f, 100);
    CHECK(rm.active == SMU_RANGE_10MA);
    range_hw_all_off(); /* what the calibration SAFE state does */
    CHECK(one_hot_count() == 0);
    smu_range_reassert(&rm);
    smu_range_tick_ms(&rm, 0u);
    CHECK(gate[SMU_RANGE_10MA] && one_hot_count() == 1);
}

int main(void) {
    test_boot_and_settle();
    test_down_jumps_directly();
    test_up_to_smallest_fit();
    test_saturation_goes_to_top();
    test_single_spike_ignored();
    test_hysteresis_band_stable();
    test_manual_disables_autorange();
    test_reassert_after_all_off();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("autorange: all tests passed\n");
    return EXIT_SUCCESS;
}
