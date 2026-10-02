#ifndef SMU_TYPES_H
#define SMU_TYPES_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { SMU_OK = 0, SMU_ERR_ARG, SMU_ERR_STATE, SMU_ERR_HW, SMU_ERR_TIMEOUT } smu_status_t;

typedef enum {
    SMU_STATE_POWER_UP = 0,
    SMU_STATE_SELF_TEST,
    SMU_STATE_OUTPUT_OFF,
    SMU_STATE_OUTPUT_STARTING,
    SMU_STATE_NORMAL,
    SMU_STATE_COMPLIANCE,
    SMU_STATE_RANGE_CHANGE,
    SMU_STATE_CALIBRATION,
    SMU_STATE_FAULT
} smu_state_t;

typedef enum { SMU_FORCE_VOLTAGE = 0, SMU_FORCE_CURRENT } smu_force_mode_t;

typedef enum { SMU_RANGE_NONE = 0, SMU_RANGE_1P5A, SMU_RANGE_100MA, SMU_RANGE_10MA, SMU_RANGE_1MA, SMU_RANGE_100UA } smu_current_range_t;

typedef enum { SMU_VRANGE_15V = 0, SMU_VRANGE_6V } smu_voltage_range_t;

typedef enum {
    SMU_FAULT_NONE = 0,
    SMU_FAULT_SELFTEST = 1u << 0,
    SMU_FAULT_RANGE = 1u << 1,
    SMU_FAULT_ADC = 1u << 2,
    SMU_FAULT_DAC = 1u << 3,
    SMU_FAULT_POWER = 1u << 4,
    SMU_FAULT_CAL = 1u << 5,
    SMU_FAULT_WATCHDOG = 1u << 6,
    SMU_FAULT_OVERLOAD = 1u << 7
} smu_fault_bits_t;

typedef struct {
    int32_t adc_i_raw, adc_v_raw, adc_cal_raw;
    float current_A, voltage_V, calbus_V;
    smu_current_range_t range;
    smu_voltage_range_t vrange;
    bool valid, compliance, overload, range_transition;
} smu_measurement_t;

typedef struct {
    smu_state_t state;
    smu_current_range_t range;
    smu_voltage_range_t vrange;
    uint32_t faults;
    uint32_t frame_count;
    uint32_t range_switch_count;
    bool current_autorange, voltage_autorange, measurement_valid, overload;
} smu_context_t;
#endif
