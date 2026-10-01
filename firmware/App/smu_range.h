#ifndef SMU_RANGE_H
#define SMU_RANGE_H

#include "smu_types.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * Sole owner of the current/voltage range switches. Keeps range_hw and the
 * measurement range state in step, and is driven by the ADC frame stream:
 *
 *   for each popped frame (in arrival order):
 *       if (smu_range_accept_frame(rm)) { process; smu_range_autorange_frame(...); }
 *   smu_range_tick_ms(rm, elapsed);   // applies queued switches
 *
 * Switches are only applied from tick, i.e. after the DMA ring was drained, so
 * every frame acquired on the old range has already been converted with the
 * old range. The frames that follow are dropped until the new range settles.
 */

#define SMU_RANGE_SLOTS 6u /* indexed by smu_current_range_t; slot 0 (NONE) unused */

typedef enum { SMU_RANGE_REASON_USER = 0, SMU_RANGE_REASON_AUTORANGE, SMU_RANGE_REASON_FORCE_I, SMU_RANGE_REASON_OVERLOAD } smu_range_reason_t;

/* PENDING: switch queued, applied by the next smu_range_tick_ms().
 * SETTLE:  switched; frames are dropped until the new range has settled. */
typedef enum { SMU_RANGE_TX_IDLE = 0, SMU_RANGE_TX_PENDING, SMU_RANGE_TX_SETTLE, SMU_RANGE_TX_FAULT } smu_range_tx_state_t;

typedef enum { SMU_RANGE_ERR_NONE = 0, SMU_RANGE_ERR_BUSY, SMU_RANGE_ERR_BAD_REQUEST, SMU_RANGE_ERR_GATE_INVALID } smu_range_error_t;

/* Fractions are of the active range's full scale. Prototype values; tune on the bench. */
typedef struct {
    float up_fraction;                        /* |I| above this up-ranges */
    float overload_fraction;                  /* |I| above this is treated as saturated */
    float fit_fraction;                       /* a range fits |I| below this; picks switch targets */
    uint16_t up_confirm_frames;               /* consecutive frames above up_fraction before up-ranging */
    uint32_t down_persist_ms;                 /* a smaller range must fit continuously this long */
    uint16_t discard_frames[SMU_RANGE_SLOTS]; /* frames dropped after switching into a current range */
    uint16_t vrange_discard_frames;           /* frames dropped after a voltage range switch */
} smu_range_config_t;

typedef struct {
    smu_range_config_t cfg;
    smu_current_range_t active;
    smu_current_range_t requested;
    smu_range_reason_t reason;
    smu_range_tx_state_t tx_state;
    smu_range_error_t error;
    smu_voltage_range_t vactive;
    smu_voltage_range_t vrequested;
    bool vpending;
    uint16_t discard_left;
    uint16_t up_count;
    uint32_t down_ms;
    bool down_candidate;
    float last_filtered_A;
    bool autorange_enabled;
    bool overload;
    bool measurement_valid;
    bool servo_allowed;
    uint32_t switch_count;
} smu_range_manager_t;

void smu_range_init(smu_range_manager_t* rm);
bool smu_range_request(smu_range_manager_t* rm, smu_current_range_t target, smu_range_reason_t reason);
bool smu_range_request_voltage(smu_range_manager_t* rm, smu_voltage_range_t target);
void smu_range_set_autorange(smu_range_manager_t* rm, bool enabled);
bool smu_range_busy(const smu_range_manager_t* rm);

/* Drive the active ranges again, e.g. after calibration opened all gates. */
void smu_range_reassert(smu_range_manager_t* rm);

/* Call once per ADC frame, in arrival order. False: drop this frame. */
bool smu_range_accept_frame(smu_range_manager_t* rm);

/* Autorange input for an accepted frame: raw CH0 code (fast up-ranging and
 * overload) and the filtered current (down-ranging). */
void smu_range_autorange_frame(smu_range_manager_t* rm, int32_t current_code, float filtered_current_A);

/* Applies queued switches and advances down-range persistence. elapsed_ms may be 0. */
void smu_range_tick_ms(smu_range_manager_t* rm, uint32_t elapsed_ms);

#endif
