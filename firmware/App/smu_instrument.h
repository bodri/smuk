#ifndef SMU_INSTRUMENT_H
#define SMU_INSTRUMENT_H
#include "smu_compliance.h"
#include "smu_fault.h"
#include "smu_range.h"
#include "smu_types.h"
#include <stdbool.h>

typedef struct {
    smu_state_t state;
    smu_fault_manager_t faults;
    smu_range_manager_t range;
    smu_compliance_t compliance;
    bool output_requested;
    bool measurement_valid;
    bool precision_servo_allowed;
    bool power_good;
    bool watchdog_ok;
    bool hw_fault;
    bool adc_ok;
    bool dac_ok;
    bool calibration_ok;
    unsigned state_ms;
} smu_instrument_t;

void smu_instrument_init(smu_instrument_t* s);
void smu_instrument_tick_1ms(smu_instrument_t* s, float abs_current_A, bool compliance_active);
bool smu_instrument_output_enable(smu_instrument_t* s);
void smu_instrument_output_disable(smu_instrument_t* s);
bool smu_instrument_clear_fault(smu_instrument_t* s);
#endif
