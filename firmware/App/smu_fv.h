#ifndef SMU_FV_H
#define SMU_FV_H
#include "smu_types.h"

typedef enum { SMU_FV_UNAVAILABLE, SMU_FV_OFF, SMU_FV_STARTING, SMU_FV_ACTIVE, SMU_FV_TRIPPED, SMU_FV_ZEROING } smu_fv_state_t;

typedef struct {
    smu_fv_state_t state;
    float requested_V, applied_V;
    uint16_t code;
    bool regulation, limited, zero_valid;
    float correction_V;
    bool pa_always_enabled;
    bool relay_available;
    float minimum_V, maximum_V, slew_V_per_s;
} smu_fv_status_t;

void smu_fv_init(void);
smu_status_t smu_fv_set_voltage(float voltage);
smu_status_t smu_fv_output(bool enabled);
smu_status_t smu_fv_zero(void);
smu_status_t smu_fv_regulation(bool enabled);
void smu_fv_process(void);
void smu_fv_trip(void);
bool smu_fv_busy(void);
const smu_fv_status_t* smu_fv_status(void);
#endif
