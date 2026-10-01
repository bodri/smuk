#ifndef SMU_CONTROL_H
#define SMU_CONTROL_H
#include "smu_servo.h"
#include "smu_types.h"
#include <stdbool.h>

typedef struct {
    smu_force_mode_t mode;
    smu_current_range_t current_range;
    float requested_voltage_V, requested_current_A;
    float effective_voltage_V, effective_current_A;
    float effective_iforce_domain_V;
    smu_servo_t voltage_servo, current_servo;
} smu_control_t;

void smu_control_init(smu_control_t* c);
void smu_control_set_mode(smu_control_t* c, smu_force_mode_t mode);
bool smu_control_set_current_range(smu_control_t* c, smu_current_range_t range);
void smu_control_set_voltage(smu_control_t* c, float v);
void smu_control_set_current(smu_control_t* c, float a);
void smu_control_update_precision(smu_control_t* c, float measured_V, float measured_A, bool servo_permission);
#endif
