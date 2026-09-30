#ifndef SMU_RANGE_H
#define SMU_RANGE_H

#include <stdbool.h>
#include <stdint.h>
#include "smu_types.h"

typedef enum {
    SMU_RANGE_REASON_USER = 0,
    SMU_RANGE_REASON_AUTORANGE,
    SMU_RANGE_REASON_FORCE_I,
    SMU_RANGE_REASON_OVERLOAD
} smu_range_reason_t;

typedef enum {
    SMU_RANGE_TX_IDLE = 0,
    SMU_RANGE_TX_COMMAND,
    SMU_RANGE_TX_WAIT_GATE,
    SMU_RANGE_TX_SETTLE,
    SMU_RANGE_TX_VERIFY,
    SMU_RANGE_TX_COMPLETE,
    SMU_RANGE_TX_FAULT
} smu_range_tx_state_t;

typedef enum {
    SMU_RANGE_ERR_NONE = 0,
    SMU_RANGE_ERR_BUSY,
    SMU_RANGE_ERR_BAD_REQUEST,
    SMU_RANGE_ERR_GATE_TIMEOUT,
    SMU_RANGE_ERR_GATE_INVALID,
    SMU_RANGE_ERR_PLAUSIBILITY
} smu_range_error_t;

typedef struct {
    smu_current_range_t active;
    smu_current_range_t requested;
    smu_range_reason_t reason;
    smu_range_tx_state_t tx_state;
    smu_range_error_t error;
    uint32_t state_ms;
    uint32_t settle_ms;
    uint32_t gate_timeout_ms;
    uint32_t down_persist_ms;
    uint32_t down_counter_ms;
    bool measurement_valid;
    bool servo_allowed;
    bool autorange_enabled;
} smu_range_manager_t;

void smu_range_init(smu_range_manager_t *rm);
bool smu_range_request(smu_range_manager_t *rm, smu_current_range_t target,
                       smu_range_reason_t reason);
void smu_range_tick_1ms(smu_range_manager_t *rm, float abs_current_A);
void smu_range_set_autorange(smu_range_manager_t *rm, bool enabled);
bool smu_range_busy(const smu_range_manager_t *rm);

#endif
