#include "../Drivers/SMU/ad5686.h"
#include "../Drivers/SMU/ads131m03.h"
#include "../Drivers/SMU/range_hw.h"
#include "../Drivers/SMU/safety_hw.h"
#include "cal_hw.h"
#include <string.h>
static bool gates[6], pa = false;
void safety_hw_init_safe(void) {
    pa = false;
    memset(gates, 0, sizeof(gates));
}
void safety_hw_disable_pa(void) {
    pa = false;
}
bool safety_hw_request_pa_enable(void) {
    pa = true;
    return true;
}
bool safety_hw_power_good(void) {
    return true;
}
bool safety_hw_compliance_active(void) {
    return false;
}
void safety_hw_watchdog_heartbeat(void) {
}
void range_hw_all_off(void) {
    memset(gates, 0, sizeof(gates));
}
void range_hw_command(smu_current_range_t r, bool on) {
    if (r > SMU_RANGE_NONE && r <= SMU_RANGE_100UA)
        gates[r] = on;
}
bool range_hw_gate_is_on(smu_current_range_t r) {
    return (r > SMU_RANGE_NONE && r <= SMU_RANGE_100UA) ? gates[r] : false;
}
bool range_hw_verify_one_hot(smu_current_range_t expected) {
    int n = 0, last = 0;
    for (int i = 1; i <= 5; i++)
        if (gates[i]) {
            n++;
            last = i;
        }
    return expected == SMU_RANGE_NONE ? n == 0 : (n == 1 && last == (int)expected);
}
bool ad5686_init(void) {
    return true;
}
bool ad5686_write_input(ad5686_channel_t c, uint16_t x) {
    (void)c;
    (void)x;
    return true;
}
bool ad5686_write_and_update(ad5686_channel_t c, uint16_t x) {
    (void)c;
    (void)x;
    return true;
}
void ad5686_ldac_pulse(void) {
}
void ad5686_reset_pulse(void) {
}
bool ads131m03_init(void) {
    return true;
}
bool ads131m03_start(void) {
    return true;
}
void ads131m03_drdy_isr(void) {
}
void ads131m03_spi_dma_complete_isr(void) {
}
bool ads131m03_pop_frame(ads131m03_frame_t* out) {
    (void)out;
    return false;
}

/* ---- Phase-4 range readback mock ---- */
static smu_current_range_t mock_range_cmd = SMU_RANGE_NONE;
static smu_current_range_t mock_range_fb = SMU_RANGE_NONE;
static unsigned mock_gate_delay = 1u;
static unsigned mock_gate_countdown = 0u;
static bool mock_gate_invalid = false;
static bool mock_current_implausible = false;

void range_hw_begin_transition(smu_current_range_t old_range, smu_current_range_t new_range) {
    (void)old_range;
    mock_range_cmd = new_range;
    mock_gate_countdown = mock_gate_delay;
    if (mock_gate_countdown == 0u)
        mock_range_fb = new_range;
}

bool range_hw_gate_state_valid(smu_current_range_t expected) {
    return !mock_gate_invalid && mock_range_fb == expected;
}

bool range_hw_gate_state_invalid(void) {
    return mock_gate_invalid;
}

bool range_hw_current_plausible(smu_current_range_t expected) {
    (void)expected;
    return !mock_current_implausible;
}

void range_hw_mock_set_gate_delay_ms(unsigned ms) {
    mock_gate_delay = ms;
}
void range_hw_mock_force_invalid(bool v) {
    mock_gate_invalid = v;
}
void range_hw_mock_force_implausible(bool v) {
    mock_current_implausible = v;
}

void range_hw_mock_tick_1ms(void) {
    if (mock_gate_countdown) {
        mock_gate_countdown--;
        if (mock_gate_countdown == 0u)
            mock_range_fb = mock_range_cmd;
    }
}

void safety_hw_enable_pa_request(void) { /* host mock: request accepted */
}

static calbus_sel_t mock_calbus = CALBUS_0V;
static bool mock_vcal_relay = false, mock_ical_relay = false;
void cal_hw_select_bus(calbus_sel_t s) {
    mock_calbus = s;
}
void cal_hw_voltage_relay(bool on) {
    mock_vcal_relay = on;
}
void cal_hw_current_inject_relay(bool on) {
    mock_ical_relay = on;
}
bool cal_hw_pa_interlock_ok(void) {
    return true;
}
