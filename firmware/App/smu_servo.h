#ifndef SMU_SERVO_H
#define SMU_SERVO_H
#include "smu_types.h"
#include <stdbool.h>

typedef struct {
    float kp;             /* correction per engineering-unit error */
    float max_correction; /* absolute correction authority */
    float max_step;       /* maximum correction change per update */
    float correction;
    float target;
    bool enabled;
    unsigned updates;
} smu_servo_t;

void smu_servo_init(smu_servo_t* s, float kp, float max_correction, float max_step);
void smu_servo_set_target(smu_servo_t* s, float target);
void smu_servo_reset(smu_servo_t* s);
float smu_servo_update(smu_servo_t* s, float measured, bool permission);
#endif
