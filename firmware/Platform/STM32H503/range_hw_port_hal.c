#include "range_hw_port.h"

#include "main.h"

typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} range_pin_t;

/* Current range shunt gates are active low: RESET closes the switch.
 * IR2A is the board net name of the 1.5 A range. */
static const range_pin_t current_gate[] = {
    [SMU_RANGE_1P5A] = {IR2A_GPIO_Port, IR2A_Pin},  [SMU_RANGE_100MA] = {IR100MA_GPIO_Port, IR100MA_Pin}, [SMU_RANGE_10MA] = {IR10MA_GPIO_Port, IR10MA_Pin},
    [SMU_RANGE_1MA] = {IR1MA_GPIO_Port, IR1MA_Pin}, [SMU_RANGE_100UA] = {IR100UA_GPIO_Port, IR100UA_Pin},
};

static const range_pin_t* gate_pin(smu_current_range_t r) {
    return (r >= SMU_RANGE_1P5A && r <= SMU_RANGE_100UA) ? &current_gate[r] : NULL;
}

void range_hw_port_current_gate(smu_current_range_t range, bool on) {
    const range_pin_t* p = gate_pin(range);
    if (p)
        HAL_GPIO_WritePin(p->port, p->pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

bool range_hw_port_current_gate_is_on(smu_current_range_t range) {
    const range_pin_t* p = gate_pin(range);
    return p && (HAL_GPIO_ReadPin(p->port, p->pin) == GPIO_PIN_RESET);
}

/* VRANGE is active high: SET selects the 6 V range, RESET the 15 V range. */
void range_hw_port_voltage_6v(bool on) {
    HAL_GPIO_WritePin(VRANGE_GPIO_Port, VRANGE_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool range_hw_port_voltage_6v_is_on(void) {
    return HAL_GPIO_ReadPin(VRANGE_GPIO_Port, VRANGE_Pin) == GPIO_PIN_SET;
}
