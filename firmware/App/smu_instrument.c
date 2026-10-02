#include "smu_instrument.h"
#include "range_hw.h"
#include "safety_hw.h"

static void safe_shutdown(smu_instrument_t* s) {
    /* Safety ordering is intentional: PA first, cleanup second. */
    safety_hw_disable_pa();
    range_hw_all_off();
    s->measurement_valid = false;
    s->precision_servo_allowed = false;
    s->output_requested = false;
}

void smu_instrument_init(smu_instrument_t* s) {
    *s = (smu_instrument_t){0};
    s->state = SMU_STATE_POWER_UP;
    smu_fault_init(&s->faults);
    smu_range_init(&s->range);
    smu_range_disconnect_input(&s->range);
    smu_compliance_init(&s->compliance, 4);
    safe_shutdown(s);
}

bool smu_instrument_output_enable(smu_instrument_t* s) {
    if (s->state != SMU_STATE_OUTPUT_OFF || smu_fault_any(&s->faults))
        return false;
    smu_range_inhibit_input_10m(&s->range, true);
    s->output_requested = true;
    s->measurement_valid = false;
    s->precision_servo_allowed = false;
    s->state = SMU_STATE_OUTPUT_STARTING;
    s->state_ms = 0;
    return smu_range_request(&s->range, SMU_RANGE_1P5A, SMU_RANGE_REASON_USER);
}

void smu_instrument_output_disable(smu_instrument_t* s) {
    safe_shutdown(s);
    smu_range_inhibit_input_10m(&s->range, false);
    if (s->state != SMU_STATE_FAULT)
        s->state = SMU_STATE_OUTPUT_OFF;
}

bool smu_instrument_clear_fault(smu_instrument_t* s) {
    if (!smu_fault_clear_latched(&s->faults))
        return false;
    s->state = SMU_STATE_OUTPUT_OFF;
    return true;
}

static void monitor_fault_inputs(smu_instrument_t* s) {
    smu_fault_set_active(&s->faults, SMU_FAULT_POWER, !s->power_good);
    smu_fault_set_active(&s->faults, SMU_FAULT_WATCHDOG, !s->watchdog_ok);
    smu_fault_set_active(&s->faults, SMU_FAULT_SELFTEST, s->hw_fault);
    smu_fault_set_active(&s->faults, SMU_FAULT_ADC, !s->adc_ok);
    smu_fault_set_active(&s->faults, SMU_FAULT_DAC, !s->dac_ok);
    smu_fault_set_active(&s->faults, SMU_FAULT_CAL, !s->calibration_ok);
}

void smu_instrument_tick_1ms(smu_instrument_t* s, float abs_current_A, bool compliance_active) {
    /* Autorange is fed per ADC frame via smu_range_current_autorange_frame(). */
    (void)abs_current_A;
    s->state_ms++;
    monitor_fault_inputs(s);

    if (s->range.tx_state == SMU_RANGE_TX_FAULT)
        smu_fault_raise(&s->faults, SMU_FAULT_RANGE);

    if (smu_fault_any(&s->faults)) {
        safe_shutdown(s);
        smu_range_disconnect_input(&s->range);
        s->state = SMU_STATE_FAULT;
        return;
    }

    switch (s->state) {
    case SMU_STATE_POWER_UP:
        safe_shutdown(s);
        s->state = SMU_STATE_SELF_TEST;
        s->state_ms = 0;
        break;

    case SMU_STATE_SELF_TEST:
        /* All prerequisite booleans are continuously checked above. */
        s->state = SMU_STATE_OUTPUT_OFF;
        s->state_ms = 0;
        break;

    case SMU_STATE_OUTPUT_OFF:
        smu_range_tick_ms(&s->range, 1u);
        s->measurement_valid = false;
        s->precision_servo_allowed = false;
        break;

    case SMU_STATE_OUTPUT_STARTING:
        smu_range_tick_ms(&s->range, 1u);
        if (!smu_range_busy(&s->range) && s->range.active == SMU_RANGE_1P5A && s->range.measurement_valid) {
            if (!safety_hw_request_pa_enable())
                break;
            s->state = SMU_STATE_NORMAL;
            s->state_ms = 0;
        }
        break;

    case SMU_STATE_NORMAL:
    case SMU_STATE_COMPLIANCE:
        smu_range_tick_ms(&s->range, 1u);
        smu_compliance_update(&s->compliance, compliance_active, s->range.measurement_valid, smu_range_busy(&s->range));
        s->measurement_valid = s->range.measurement_valid && !smu_range_busy(&s->range);
        s->precision_servo_allowed = s->compliance.servo_allowed && s->measurement_valid;
        s->state = s->compliance.active ? SMU_STATE_COMPLIANCE : SMU_STATE_NORMAL;
        break;

    case SMU_STATE_RANGE_CHANGE:
        /* Reserved for UI/reporting; range manager is transactional. */
        smu_range_tick_ms(&s->range, 1u);
        break;

    case SMU_STATE_CALIBRATION:
        safe_shutdown(s);
        break;

    case SMU_STATE_FAULT:
    default:
        safe_shutdown(s);
        break;
    }
}
