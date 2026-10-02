#ifndef SMU_CAL_SEQ_H
#define SMU_CAL_SEQ_H
#include "cal_hw.h"
#include <stdbool.h>
#include <stdint.h>

/* Maximum elapsed time per ADC discard/acquisition phase. */
#define SMU_CAL_SEQ_DEFAULT_TIMEOUT_MS 1000u

typedef enum { CAL_TARGET_VOLTAGE = 0, CAL_TARGET_CURRENT } smu_cal_target_t;

typedef enum {
    CAL_SEQ_IDLE = 0,
    CAL_SEQ_SAFE,
    CAL_SEQ_SELECT,
    CAL_SEQ_SETTLE,
    CAL_SEQ_RELAY,
    CAL_SEQ_DISCARD,
    CAL_SEQ_ACQUIRE,
    CAL_SEQ_OPEN,
    CAL_SEQ_ZERO,
    CAL_SEQ_DONE,
    CAL_SEQ_FAULT
} smu_cal_seq_state_t;

typedef struct {
    smu_cal_seq_state_t state;
    smu_cal_target_t target;
    calbus_sel_t bus;
    uint32_t state_ms;
    uint32_t acquisition_timeout_ms;
    uint16_t discard_required, discarded;
    uint16_t acquire_required, acquired;
    int32_t target_sum;
    int32_t calbus_sum;
    bool measurement_valid, servo_allowed, fault;

    float ratio;

    float target_average;
    float calbus_average;
    bool result_ready;
} smu_cal_seq_t;

void smu_cal_seq_abort(smu_cal_seq_t* s);
void smu_cal_seq_init(smu_cal_seq_t* s);
bool smu_cal_seq_start(smu_cal_seq_t* s, smu_cal_target_t target, calbus_sel_t bus);
void smu_cal_seq_tick_1ms(smu_cal_seq_t* s);
/* Foreground tick; elapsed time includes delayed/missed scheduler ticks. */
void smu_cal_seq_tick_elapsed_ms(smu_cal_seq_t* s, uint32_t elapsed_ms);
void smu_cal_seq_adc_frame(smu_cal_seq_t* s, int32_t target_code, int32_t calbus_code);
#endif
