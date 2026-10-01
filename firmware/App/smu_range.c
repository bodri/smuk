#include "smu_range.h"
#include "range_hw.h"

#define RANGE_GATE_TIMEOUT_MS 5u
#define RANGE_SETTLE_MS 2u
#define RANGE_DOWN_PERSIST_MS 50u

static bool range_is_valid(smu_current_range_t r) {
    return r >= SMU_RANGE_1P5A && r <= SMU_RANGE_100UA;
}

static smu_current_range_t next_larger(smu_current_range_t r) {
    switch (r) {
    case SMU_RANGE_100UA:
        return SMU_RANGE_1MA;
    case SMU_RANGE_1MA:
        return SMU_RANGE_10MA;
    case SMU_RANGE_10MA:
        return SMU_RANGE_100MA;
    case SMU_RANGE_100MA:
        return SMU_RANGE_1P5A;
    default:
        return SMU_RANGE_1P5A;
    }
}

static smu_current_range_t next_smaller(smu_current_range_t r) {
    switch (r) {
    case SMU_RANGE_1P5A:
        return SMU_RANGE_100MA;
    case SMU_RANGE_100MA:
        return SMU_RANGE_10MA;
    case SMU_RANGE_10MA:
        return SMU_RANGE_1MA;
    case SMU_RANGE_1MA:
        return SMU_RANGE_100UA;
    default:
        return SMU_RANGE_100UA;
    }
}

static float up_threshold(smu_current_range_t r) {
    switch (r) {
    case SMU_RANGE_100UA:
        return 90e-6f;
    case SMU_RANGE_1MA:
        return 900e-6f;
    case SMU_RANGE_10MA:
        return 9e-3f;
    case SMU_RANGE_100MA:
        return 90e-3f;
    default:
        return 1e30f;
    }
}

static float down_threshold(smu_current_range_t r) {
    switch (r) {
    case SMU_RANGE_1MA:
        return 70e-6f;
    case SMU_RANGE_10MA:
        return 700e-6f;
    case SMU_RANGE_100MA:
        return 7e-3f;
    case SMU_RANGE_1P5A:
        return 70e-3f;
    default:
        return -1.0f;
    }
}

void smu_range_init(smu_range_manager_t* rm) {
    *rm = (smu_range_manager_t){0};
    rm->active = SMU_RANGE_NONE;
    rm->requested = SMU_RANGE_NONE;
    rm->tx_state = SMU_RANGE_TX_IDLE;
    rm->gate_timeout_ms = RANGE_GATE_TIMEOUT_MS;
    rm->settle_ms = RANGE_SETTLE_MS;
    rm->down_persist_ms = RANGE_DOWN_PERSIST_MS;
    rm->measurement_valid = false;
    rm->servo_allowed = false;
}

bool smu_range_busy(const smu_range_manager_t* rm) {
    return rm->tx_state != SMU_RANGE_TX_IDLE;
}

bool smu_range_request(smu_range_manager_t* rm, smu_current_range_t target, smu_range_reason_t reason) {
    if (!range_is_valid(target)) {
        rm->error = SMU_RANGE_ERR_BAD_REQUEST;
        return false;
    }
    if (smu_range_busy(rm)) {
        rm->error = SMU_RANGE_ERR_BUSY;
        return false;
    }
    if (target == rm->active)
        return true;

    rm->requested = target;
    rm->reason = reason;
    rm->tx_state = SMU_RANGE_TX_COMMAND;
    rm->state_ms = 0;
    rm->measurement_valid = false;
    rm->servo_allowed = false;
    rm->down_counter_ms = 0;
    return true;
}

void smu_range_set_autorange(smu_range_manager_t* rm, bool enabled) {
    rm->autorange_enabled = enabled;
    rm->down_counter_ms = 0;
}

static void autorange_tick(smu_range_manager_t* rm, float a) {
    if (!rm->autorange_enabled || !range_is_valid(rm->active))
        return;

    if (a > up_threshold(rm->active)) {
        (void)smu_range_request(rm, next_larger(rm->active), SMU_RANGE_REASON_AUTORANGE);
        return;
    }

    float dn = down_threshold(rm->active);
    if (dn >= 0.0f && a < dn) {
        if (++rm->down_counter_ms >= rm->down_persist_ms) {
            (void)smu_range_request(rm, next_smaller(rm->active), SMU_RANGE_REASON_AUTORANGE);
            rm->down_counter_ms = 0;
        }
    } else {
        rm->down_counter_ms = 0;
    }
}

void smu_range_tick_1ms(smu_range_manager_t* rm, float abs_current_A) {
    if (rm->tx_state == SMU_RANGE_TX_IDLE) {
        autorange_tick(rm, abs_current_A);
        return;
    }

    rm->state_ms++;

    switch (rm->tx_state) {
    case SMU_RANGE_TX_COMMAND:
        /*
         * Exact make-before-break/break-before-make policy is intentionally
         * NOT encoded here. range_hw_begin_transition() owns the prototype-
         * specific sequence and may later be changed after hardware testing.
         */
        range_hw_begin_transition(rm->active, rm->requested);
        rm->tx_state = SMU_RANGE_TX_WAIT_GATE;
        rm->state_ms = 0;
        break;

    case SMU_RANGE_TX_WAIT_GATE:
        if (range_hw_gate_state_valid(rm->requested)) {
            rm->tx_state = SMU_RANGE_TX_SETTLE;
            rm->state_ms = 0;
        } else if (range_hw_gate_state_invalid()) {
            rm->error = SMU_RANGE_ERR_GATE_INVALID;
            rm->tx_state = SMU_RANGE_TX_FAULT;
        } else if (rm->state_ms >= rm->gate_timeout_ms) {
            rm->error = SMU_RANGE_ERR_GATE_TIMEOUT;
            rm->tx_state = SMU_RANGE_TX_FAULT;
        }
        break;

    case SMU_RANGE_TX_SETTLE:
        if (rm->state_ms >= rm->settle_ms) {
            rm->tx_state = SMU_RANGE_TX_VERIFY;
            rm->state_ms = 0;
        }
        break;

    case SMU_RANGE_TX_VERIFY:
        if (!range_hw_gate_state_valid(rm->requested)) {
            rm->error = SMU_RANGE_ERR_GATE_INVALID;
            rm->tx_state = SMU_RANGE_TX_FAULT;
        } else if (!range_hw_current_plausible(rm->requested)) {
            rm->error = SMU_RANGE_ERR_PLAUSIBILITY;
            rm->tx_state = SMU_RANGE_TX_FAULT;
        } else {
            rm->active = rm->requested;
            rm->tx_state = SMU_RANGE_TX_COMPLETE;
        }
        break;

    case SMU_RANGE_TX_COMPLETE:
        rm->measurement_valid = true;
        rm->servo_allowed = true;
        rm->error = SMU_RANGE_ERR_NONE;
        rm->tx_state = SMU_RANGE_TX_IDLE;
        rm->state_ms = 0;
        break;

    case SMU_RANGE_TX_FAULT:
        range_hw_all_off();
        rm->measurement_valid = false;
        rm->servo_allowed = false;
        break;

    default:
        break;
    }
}
