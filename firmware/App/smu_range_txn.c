#include "smu_range_txn.h"
#include "smu_iforce.h"

void smu_force_i_range_txn_init(smu_force_i_range_txn_t* t) {
    *t = (smu_force_i_range_txn_t){0};
    t->state = SMU_FI_TX_IDLE;
    t->settle_ms = 2u;
    t->discard_frames = 8u; /* prototype value; not frozen */
}
bool smu_force_i_range_txn_start(smu_force_i_range_txn_t* t, smu_range_manager_t* rm, smu_current_range_t target) {
    if (t->state != SMU_FI_TX_IDLE || smu_range_busy(rm))
        return false;
    t->target = target;
    t->state = SMU_FI_TX_RANGE;
    t->state_ms = 0;
    t->discarded = 0;
    t->measurement_valid = false;
    t->servo_allowed = false;
    return smu_range_request(rm, target, SMU_RANGE_REASON_FORCE_I);
}
void smu_force_i_range_txn_adc_frame(smu_force_i_range_txn_t* t) {
    if (t->state == SMU_FI_TX_SETTLE && t->discarded < t->discard_frames)
        t->discarded++;
}
void smu_force_i_range_txn_tick_1ms(smu_force_i_range_txn_t* t, smu_range_manager_t* rm, smu_control_t* ctl, const smu_cal_record_t* cal, float abs_current_A) {
    /* Autorange is fed per ADC frame via smu_range_autorange_frame(). */
    (void)abs_current_A;
    t->state_ms++;
    switch (t->state) {
    case SMU_FI_TX_IDLE:
        return;
    case SMU_FI_TX_RANGE:
        smu_range_tick_ms(rm, 1u);
        if (rm->tx_state == SMU_RANGE_TX_FAULT) {
            t->fault = true;
            t->state = SMU_FI_TX_FAULT;
            break;
        }
        if (!smu_range_busy(rm) && rm->active == t->target) {
            t->state = SMU_FI_TX_REMAP;
            t->state_ms = 0;
        }
        break;
    case SMU_FI_TX_REMAP:
        if (!smu_control_set_current_range(ctl, t->target) || !smu_current_fits_range(ctl->requested_current_A, t->target, 1.0f)) {
            t->fault = true;
            t->state = SMU_FI_TX_FAULT;
            break;
        }
        t->state = SMU_FI_TX_DAC;
        t->state_ms = 0;
        break;
    case SMU_FI_TX_DAC: {
        smu_source_request_t r = {0};
        smu_dac_codes_t d;
        r.mode = SMU_FORCE_CURRENT;
        r.current_range = t->target;
        r.current_command = ctl->effective_current_A;
        /* Existing limiter refs remain owned by higher-level command manager. */
        if (!smu_source_build_codes(&r, cal, &d) || !smu_source_commit(&d)) {
            t->fault = true;
            t->state = SMU_FI_TX_FAULT;
            break;
        }
        t->discarded = 0;
        t->state = SMU_FI_TX_SETTLE;
        t->state_ms = 0;
        break;
    }
    case SMU_FI_TX_SETTLE:
        if (t->state_ms >= t->settle_ms && t->discarded >= t->discard_frames) {
            t->state = SMU_FI_TX_VALIDATE;
            t->state_ms = 0;
        }
        break;
    case SMU_FI_TX_VALIDATE:
        if (!rm->measurement_valid) {
            t->fault = true;
            t->state = SMU_FI_TX_FAULT;
            break;
        }
        t->state = SMU_FI_TX_DONE;
        break;
    case SMU_FI_TX_DONE:
        t->measurement_valid = true;
        t->servo_allowed = true;
        t->state = SMU_FI_TX_IDLE;
        t->state_ms = 0;
        break;
    case SMU_FI_TX_FAULT:
        t->measurement_valid = false;
        t->servo_allowed = false;
        break;
    }
}
