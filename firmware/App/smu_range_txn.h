#ifndef SMU_RANGE_TXN_H
#define SMU_RANGE_TXN_H
#include <stdbool.h>
#include "smu_range.h"
#include "smu_control.h"
#include "smu_source.h"
#include "../Storage/calibration_store.h"

typedef enum {
    SMU_FI_TX_IDLE=0,
    SMU_FI_TX_RANGE,
    SMU_FI_TX_REMAP,
    SMU_FI_TX_DAC,
    SMU_FI_TX_SETTLE,
    SMU_FI_TX_VALIDATE,
    SMU_FI_TX_DONE,
    SMU_FI_TX_FAULT
} smu_fi_tx_state_t;

typedef struct {
    smu_fi_tx_state_t state;
    smu_current_range_t target;
    unsigned settle_ms;
    unsigned state_ms;
    unsigned discard_frames;
    unsigned discarded;
    bool measurement_valid;
    bool servo_allowed;
    bool fault;
} smu_force_i_range_txn_t;

void smu_force_i_range_txn_init(smu_force_i_range_txn_t *t);
bool smu_force_i_range_txn_start(smu_force_i_range_txn_t *t,
                                 smu_range_manager_t *rm,
                                 smu_current_range_t target);
void smu_force_i_range_txn_tick_1ms(smu_force_i_range_txn_t *t,
                                    smu_range_manager_t *rm,
                                    smu_control_t *ctl,
                                    const smu_cal_record_t *cal,
                                    float abs_current_A);
void smu_force_i_range_txn_adc_frame(smu_force_i_range_txn_t *t);
#endif
