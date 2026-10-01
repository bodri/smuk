#include "smu_control.h"
#include "smu_iforce.h"

static void configure_current_servo(smu_control_t* c, smu_current_range_t r) {
    float fs = smu_current_range_fs_A(r);
    /* Prototype: correction authority 0.5% FS; max step 0.02% FS/update. */
    smu_servo_init(&c->current_servo, 0.05f, 0.005f * fs, 0.0002f * fs);
    smu_servo_set_target(&c->current_servo, c->requested_current_A);
}
void smu_control_init(smu_control_t* c) {
    *c = (smu_control_t){0};
    c->mode = SMU_FORCE_VOLTAGE;
    c->current_range = SMU_RANGE_1P5A;
    smu_servo_init(&c->voltage_servo, 0.05f, 0.050f, 0.001f);
    configure_current_servo(c, c->current_range);
}
void smu_control_set_mode(smu_control_t* c, smu_force_mode_t m) {
    if (c->mode != m) {
        smu_servo_reset(&c->voltage_servo);
        smu_servo_reset(&c->current_servo);
        c->mode = m;
    }
}
bool smu_control_set_current_range(smu_control_t* c, smu_current_range_t r) {
    if (smu_current_range_fs_A(r) <= 0.0f)
        return false;
    /*
     * Preserve the requested physical current across the range transition.
     * Reset only the small precision trim; the nominal command is remapped.
     */
    c->current_range = r;
    configure_current_servo(c, r);
    if (!smu_iforce_from_current(c->requested_current_A, r, &c->effective_iforce_domain_V)) {
        /* Requested current does not fit this range. Keep the physical request;
           range manager must up-range before enabling Force-I output. */
        c->effective_iforce_domain_V = 0.0f;
    }
    return true;
}
void smu_control_set_voltage(smu_control_t* c, float v) {
    c->requested_voltage_V = v;
    smu_servo_set_target(&c->voltage_servo, v);
}
void smu_control_set_current(smu_control_t* c, float a) {
    c->requested_current_A = a;
    smu_servo_set_target(&c->current_servo, a);
    (void)smu_iforce_from_current(a, c->current_range, &c->effective_iforce_domain_V);
}
void smu_control_update_precision(smu_control_t* c, float mv, float ma, bool permit) {
    if (c->mode == SMU_FORCE_VOLTAGE) {
        float d = smu_servo_update(&c->voltage_servo, mv, permit);
        c->effective_voltage_V = c->requested_voltage_V + d;
        c->effective_current_A = c->requested_current_A;
        (void)smu_servo_update(&c->current_servo, ma, false);
    } else {
        float d = smu_servo_update(&c->current_servo, ma, permit);
        c->effective_current_A = c->requested_current_A + d;
        c->effective_voltage_V = c->requested_voltage_V;
        (void)smu_servo_update(&c->voltage_servo, mv, false);
    }
    (void)smu_iforce_from_current(c->effective_current_A, c->current_range, &c->effective_iforce_domain_V);
}
