#include "smu_range.h"
#include "range_hw.h"
#include "smu_iforce.h"
#include "smu_measurement.h"
#include <math.h>

/* ~98% of the 24-bit positive full scale: treat as ADC clipping. */
#define ADC_CLIP_CODE 8220000

static bool range_is_valid(smu_current_range_t r) {
    return r >= SMU_RANGE_1P5A && r <= SMU_RANGE_100UA;
}

/* Enum order runs from the largest (1.5 A) to the smallest (100 uA) range. */
static bool is_smaller(smu_current_range_t a, smu_current_range_t b) {
    return (int)a > (int)b;
}

static bool fits(const smu_range_manager_t* rm, float abs_A, smu_current_range_t r) {
    return abs_A < rm->cfg.fit_fraction * smu_current_range_fs_A(r);
}

/* Smallest range that fits abs_A; 1.5 A if none does. */
static smu_current_range_t smallest_fit(const smu_range_manager_t* rm, float abs_A) {
    for (int r = SMU_RANGE_100UA; r > SMU_RANGE_1P5A; r--)
        if (fits(rm, abs_A, (smu_current_range_t)r))
            return (smu_current_range_t)r;
    return SMU_RANGE_1P5A;
}

static void reset_autorange(smu_range_manager_t* rm) {
    rm->up_count = 0;
    rm->down_ms = 0;
    rm->down_candidate = false;
}

static void enter_fault(smu_range_manager_t* rm) {
    range_hw_all_off();
    rm->error = SMU_RANGE_ERR_GATE_INVALID;
    rm->tx_state = SMU_RANGE_TX_FAULT;
    rm->measurement_valid = false;
    rm->servo_allowed = false;
    smu_measurement_set_range_transition(true);
}

static void apply_switch(smu_range_manager_t* rm) {
    uint16_t discard = 0;

    if (rm->tx_state == SMU_RANGE_TX_PENDING) {
        range_hw_select(rm->requested);
        if (!range_hw_is_selected(rm->requested)) {
            enter_fault(rm);
            return;
        }
        rm->active = rm->requested;
        smu_measurement_set_current_range(rm->active);
        discard = rm->cfg.discard_frames[rm->active];
        rm->switch_count++;
    }

    if (rm->vpending) {
        range_hw_select_voltage(rm->vrequested);
        if (range_hw_voltage_selected() != rm->vrequested) {
            enter_fault(rm);
            return;
        }
        rm->vactive = rm->vrequested;
        rm->vpending = false;
        smu_measurement_set_voltage_range(rm->vactive);
        if (rm->cfg.vrange_discard_frames > discard)
            discard = rm->cfg.vrange_discard_frames;
    }

    smu_measurement_set_range_transition(true);
    smu_measurement_reset_filters();
    rm->discard_left = discard;
    rm->tx_state = SMU_RANGE_TX_SETTLE;
    rm->measurement_valid = false;
    rm->servo_allowed = false;
    reset_autorange(rm);
}

void smu_range_init(smu_range_manager_t* rm) {
    *rm = (smu_range_manager_t){0};
    rm->active = SMU_RANGE_NONE;
    rm->requested = SMU_RANGE_NONE;
    rm->vactive = SMU_VRANGE_15V;
    rm->vrequested = SMU_VRANGE_15V;
    rm->tx_state = SMU_RANGE_TX_IDLE;

    rm->cfg.up_fraction = 0.90f;
    rm->cfg.overload_fraction = 1.05f;
    rm->cfg.fit_fraction = 0.70f;
    rm->cfg.up_confirm_frames = 2u;
    rm->cfg.down_persist_ms = 50u;
    /* ~4 kSPS: covers the in-flight frame, sinc3 latency and analog settling.
     * The high-ohm shunts settle slowest. */
    rm->cfg.discard_frames[SMU_RANGE_1P5A] = 8u;
    rm->cfg.discard_frames[SMU_RANGE_100MA] = 8u;
    rm->cfg.discard_frames[SMU_RANGE_10MA] = 8u;
    rm->cfg.discard_frames[SMU_RANGE_1MA] = 16u;
    rm->cfg.discard_frames[SMU_RANGE_100UA] = 40u;
    rm->cfg.vrange_discard_frames = 40u;
}

bool smu_range_busy(const smu_range_manager_t* rm) {
    return rm->tx_state != SMU_RANGE_TX_IDLE || rm->vpending;
}

bool smu_range_request(smu_range_manager_t* rm, smu_current_range_t target, smu_range_reason_t reason) {
    if (!range_is_valid(target)) {
        rm->error = SMU_RANGE_ERR_BAD_REQUEST;
        return false;
    }
    if (rm->tx_state != SMU_RANGE_TX_IDLE) {
        rm->error = SMU_RANGE_ERR_BUSY;
        return false;
    }
    if (target == rm->active)
        return true;

    rm->requested = target;
    rm->reason = reason;
    rm->tx_state = SMU_RANGE_TX_PENDING;
    return true;
}

bool smu_range_request_voltage(smu_range_manager_t* rm, smu_voltage_range_t target) {
    if (target != SMU_VRANGE_15V && target != SMU_VRANGE_6V) {
        rm->error = SMU_RANGE_ERR_BAD_REQUEST;
        return false;
    }
    if (rm->tx_state == SMU_RANGE_TX_FAULT) {
        rm->error = SMU_RANGE_ERR_BUSY;
        return false;
    }
    rm->vrequested = target;
    rm->vpending = true;
    return true;
}

void smu_range_set_autorange(smu_range_manager_t* rm, bool enabled) {
    rm->autorange_enabled = enabled;
    reset_autorange(rm);
}

void smu_range_reassert(smu_range_manager_t* rm) {
    if (rm->tx_state == SMU_RANGE_TX_FAULT)
        return;
    if (range_is_valid(rm->active)) {
        rm->requested = rm->active;
        rm->tx_state = SMU_RANGE_TX_PENDING;
    }
    if (!rm->vpending)
        rm->vrequested = rm->vactive;
    rm->vpending = true;
}

bool smu_range_accept_frame(smu_range_manager_t* rm) {
    if (rm->tx_state == SMU_RANGE_TX_SETTLE) {
        if (rm->discard_left > 0u) {
            rm->discard_left--;
            return false;
        }
        rm->tx_state = SMU_RANGE_TX_IDLE;
        rm->measurement_valid = true;
        rm->servo_allowed = true;
        smu_measurement_set_range_transition(false);
    }
    return rm->tx_state != SMU_RANGE_TX_FAULT;
}

void smu_range_autorange_frame(smu_range_manager_t* rm, int32_t code, float filtered_A) {
    if (!range_is_valid(rm->active))
        return;

    const float fs = smu_current_range_fs_A(rm->active);
    const float abs_A = fabsf(smu_current_from_adc(smu_ads_code_to_volts(code), rm->active));
    const bool clipped = code >= ADC_CLIP_CODE || code <= -ADC_CLIP_CODE;
    const bool saturated = clipped || abs_A > rm->cfg.overload_fraction * fs;

    rm->overload = saturated;
    smu_measurement_set_overload(saturated);

    if (!rm->autorange_enabled || rm->tx_state != SMU_RANGE_TX_IDLE)
        return;

    /* Up-range. A saturated reading says nothing about the real current, so
     * go straight to 1.5 A and let down-ranging find the right range;
     * otherwise jump to the smallest range that fits the reading. */
    if (rm->active != SMU_RANGE_1P5A && abs_A > rm->cfg.up_fraction * fs) {
        rm->down_candidate = false;
        rm->down_ms = 0;
        if (saturated) {
            (void)smu_range_request(rm, SMU_RANGE_1P5A, SMU_RANGE_REASON_OVERLOAD);
        } else if (++rm->up_count >= rm->cfg.up_confirm_frames) {
            smu_current_range_t t = smallest_fit(rm, abs_A);
            if (!is_smaller(rm->active, t))
                t = (smu_current_range_t)((int)rm->active - 1);
            (void)smu_range_request(rm, t, SMU_RANGE_REASON_AUTORANGE);
        }
        return;
    }
    rm->up_count = 0;

    /* Down-range candidate: some smaller range fits the filtered current.
     * Must hold for down_persist_ms (checked in tick). */
    rm->last_filtered_A = fabsf(filtered_A);
    rm->down_candidate = is_smaller(smallest_fit(rm, rm->last_filtered_A), rm->active);
    if (!rm->down_candidate)
        rm->down_ms = 0;
}

void smu_range_tick_ms(smu_range_manager_t* rm, uint32_t elapsed_ms) {
    if (rm->tx_state == SMU_RANGE_TX_FAULT)
        return;

    if (rm->tx_state == SMU_RANGE_TX_PENDING || (rm->tx_state == SMU_RANGE_TX_IDLE && rm->vpending)) {
        apply_switch(rm);
        return;
    }

    if (!rm->autorange_enabled || rm->tx_state != SMU_RANGE_TX_IDLE || !rm->down_candidate)
        return;

    rm->down_ms += elapsed_ms;
    if (rm->down_ms >= rm->cfg.down_persist_ms) {
        (void)smu_range_request(rm, smallest_fit(rm, rm->last_filtered_A), SMU_RANGE_REASON_AUTORANGE);
        reset_autorange(rm);
    }
}
