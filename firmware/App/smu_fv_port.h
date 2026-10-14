#ifndef SMU_FV_PORT_H
#define SMU_FV_PORT_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float reference_V, zero_dac_V, output_per_dac_V;
    float minimum_V, maximum_V, slew_V_per_s;
    float maximum_current_A;
    bool dac_buffered;
    bool pa_enable_stub;
} smu_fv_config_t;

/* All port functions are foreground only. init must leave PA and relay OFF. */
bool smu_fv_port_init(smu_fv_config_t* config);
bool smu_fv_port_write(uint16_t code);
bool smu_fv_port_pa(bool enabled);
bool smu_fv_port_relay(bool connected);
#endif
