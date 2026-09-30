/*
 * cal_hw.c
 *
 *  Created on: 29 Sept 2026
 *      Author: bodri
 */

#include "cal_hw.h"
#include "main.h"

static calbus_sel_t calbus = CALBUS_0V;
static bool vcal_relay = false, ical_relay = false;

void cal_hw_select_bus(calbus_sel_t s)
{
    if (s > CALBUS_N3V)
        s = CALBUS_0V;

    GPIO_PinState s0 = GPIO_PIN_RESET;
    GPIO_PinState s1 = GPIO_PIN_RESET;

    switch (s)
    {
    case CALBUS_0V:
        s1 = GPIO_PIN_RESET;
        s0 = GPIO_PIN_RESET;
        break;

    case CALBUS_P1V5:
        s1 = GPIO_PIN_RESET;
        s0 = GPIO_PIN_SET;
        break;

    case CALBUS_P3V:
        s1 = GPIO_PIN_SET;
        s0 = GPIO_PIN_RESET;
        break;

    case CALBUS_N3V:
        s1 = GPIO_PIN_SET;
        s0 = GPIO_PIN_SET;
        break;

    default:
        s1 = GPIO_PIN_RESET;
        s0 = GPIO_PIN_RESET;
        s = CALBUS_0V;
        break;
    }

    HAL_GPIO_WritePin(CALBUS_S0_GPIO_Port,
                      CALBUS_S0_Pin, s0);

    HAL_GPIO_WritePin(CALBUS_S1_GPIO_Port,
                      CALBUS_S1_Pin, s1);

    calbus = s;
}

void cal_hw_voltage_relay(bool on) {
	HAL_GPIO_WritePin(VCAL_GPIO_Port, VCAL_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
	vcal_relay = on;
}

void cal_hw_current_inject_relay(bool on) {
	HAL_GPIO_WritePin(ICAL_GPIO_Port, ICAL_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
	ical_relay = on;
}

bool cal_hw_pa_interlock_ok(void) {
	return true;
}
