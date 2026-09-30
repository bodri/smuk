#include "smu_servo.h"

static float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}
void smu_servo_init(smu_servo_t *s, float kp, float authority, float max_step) {
    *s=(smu_servo_t){0};
    s->kp=kp; s->max_correction=authority; s->max_step=max_step;
}
void smu_servo_set_target(smu_servo_t *s, float target) { s->target=target; }
void smu_servo_reset(smu_servo_t *s) { s->correction=0.0f; s->enabled=false; }
float smu_servo_update(smu_servo_t *s, float measured, bool permission) {
    if (!permission) { s->enabled=false; return s->correction; } /* freeze, don't wind up */
    s->enabled=true;
    float delta=s->kp*(s->target-measured);
    delta=clampf(delta,-s->max_step,s->max_step);
    s->correction=clampf(s->correction+delta,-s->max_correction,s->max_correction);
    s->updates++;
    return s->correction;
}
