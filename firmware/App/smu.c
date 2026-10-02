#include "smu.h"
#include "ads131m03.h"
#include "ads131m03_port.h"
#include "calibration_store.h"
#include "smu_cal_debug.h"
#include "smu_calibration.h"
#include "smu_console.h"
#include "smu_port.h"
#include <string.h>

static smu_context_t g;
static smu_range_manager_t ranges;
static uint32_t last_ms;

/* Non-static for debugger watch windows (previously in main.c). */
ads131m03_bringup_result_t ads_result;
const ads131m03_dma_status_t* ads_dma_status = NULL;
ads131m03_dma_frame_t adc_frame;
smu_measurement_outputs_t smu_outputs;

static void fault(uint32_t bits) {
    smu_range_disconnect_input(&ranges);
    g.faults |= bits;
    g.state = SMU_STATE_FAULT;
    smu_measurement_set_valid(false);
}

bool smu_init(void) {
    memset(&g, 0, sizeof(g));
    g.state = SMU_STATE_POWER_UP;
    g.current_autorange = true;
    g.voltage_autorange = true;

    /*
     * smu_cal_store_init() DOES NOT erase Flash.
     * smu_calibration_init() loads the newest valid Flash calibration (or
     * unity defaults) and initialises smu_measurement with it.
     */
    smu_cal_store_init();
    smu_calibration_init();
    smu_cal_debug_init();
    smu_range_init(&ranges);
    smu_range_disconnect_input(&ranges);

    if (!ads131m03_bringup_run(&ads_result)) {
        fault(SMU_FAULT_ADC);
        return false;
    }

    /* Boot ranges are driven before acquisition starts; the first frames
     * after DRDY is enabled are dropped while they settle. */
    (void)smu_range_request(&ranges, SMU_RANGE_1P5A, SMU_RANGE_REASON_USER);
    (void)smu_range_request_voltage(&ranges, SMU_VRANGE_15V);
    (void)smu_range_request_input_10m(&ranges, true);
    smu_range_tick_ms(&ranges, 0u);
    if (ranges.tx_state == SMU_RANGE_TX_FAULT) {
        fault(SMU_FAULT_RANGE);
        return false;
    }
    smu_range_set_current_autorange(&ranges, g.current_autorange);
    smu_range_set_voltage_autorange(&ranges, g.voltage_autorange);
    smu_measurement_set_valid(true);

    ads131m03_dma_init();
    ads_dma_status = ads131m03_dma_get_status();
    ads131m03_port_drdy_enable(true);

    last_ms = smu_port_millis();
    g.state = SMU_STATE_NORMAL;
    return true;
}

static void process_frames(void) {
    ads131m03_dma_frame_t f;

    while (ads131m03_dma_pop(&f)) {
        adc_frame = f;
        g.frame_count++;

        smu_cal_debug_frame(&f);

        if (!smu_range_accept_frame(&ranges))
            continue;

        /* CRC was already checked in the DMA ISR; bad frames never reach the ring. */
        ads131m03_frame_t mf = {0};
        mf.ch[0] = f.ch0;
        mf.ch[1] = f.ch1;
        mf.ch[2] = f.ch2;
        mf.crc_ok = true;

        if (!smu_measurement_process_frame(&mf))
            continue;
        smu_measurement_get_outputs(&smu_outputs);
        smu_console_frame(&f, ranges.active, ranges.vactive);

        if (g.state == SMU_STATE_NORMAL) {
            smu_range_current_autorange_frame(&ranges, f.ch0, smu_outputs.fast.current_A);
            const smu_linear_cal_t cal = smu_calibration_get()->measurement.voltage[ranges.vactive];
            const float voltage = smu_voltage_from_adc(smu_ads_code_to_volts(f.ch1), ranges.vactive) * cal.gain + cal.offset;
            smu_range_voltage_autorange_frame(&ranges, f.ch1, voltage, smu_outputs.fast.voltage_V);
        }
    }
}

/* The calibration sequencer opens all shunts itself, so ranges are left alone
 * while it runs and driven again once it is done. */
static void update_calibration_state(void) {
    const bool calibrating = smu_cal_debug_active();

    if (smu_cal_debug_faulted()) {
        fault(SMU_FAULT_CAL);
    } else if (calibrating && g.state == SMU_STATE_NORMAL) {
        g.state = SMU_STATE_CALIBRATION;
        smu_range_set_current_autorange(&ranges, false);
        smu_range_set_voltage_autorange(&ranges, false);
        smu_measurement_set_valid(false);
    } else if (!calibrating && g.state == SMU_STATE_CALIBRATION) {
        g.state = SMU_STATE_NORMAL;
        smu_range_reassert(&ranges);
        smu_range_set_current_autorange(&ranges, g.current_autorange);
        smu_range_set_voltage_autorange(&ranges, g.voltage_autorange);
        smu_measurement_set_valid(true);
    }
}

void smu_process(void) {
    if (g.state == SMU_STATE_POWER_UP)
        return;

    process_frames();

    const uint32_t now = smu_port_millis();
    const uint32_t elapsed_ms = now - last_ms;
    last_ms = now;

    smu_cal_debug_process(elapsed_ms);
    update_calibration_state();

    if (g.state == SMU_STATE_NORMAL) {
        smu_range_tick_ms(&ranges, elapsed_ms);
        if (ranges.tx_state == SMU_RANGE_TX_FAULT)
            fault(SMU_FAULT_RANGE);
    }

    g.input_10m_requested = ranges.input_10m_requested;
    g.input_10m_active = ranges.input_10m_active;
    g.range = ranges.active;
    g.vrange = ranges.vactive;
    g.overload = ranges.overload;
    g.range_switch_count = ranges.switch_count;
    g.measurement_valid = (g.state == SMU_STATE_NORMAL) && ranges.measurement_valid;
}

void smu_get_measurement(smu_measurement_outputs_t* out) {
    smu_measurement_get_outputs(out);
}

const smu_context_t* smu_get_context(void) {
    return &g;
}

smu_status_t smu_set_input_10m(bool enabled) {
    if (g.state != SMU_STATE_NORMAL || smu_range_busy(&ranges))
        return SMU_ERR_STATE;
    if (!smu_range_request_input_10m(&ranges, enabled))
        return SMU_ERR_STATE;
    g.input_10m_requested = enabled;
    if (smu_range_busy(&ranges))
        g.measurement_valid = false;
    return SMU_OK;
}

smu_status_t smu_set_current_range(smu_current_range_t range) {
    if (g.state != SMU_STATE_NORMAL)
        return SMU_ERR_STATE;
    g.current_autorange = false;
    smu_range_set_current_autorange(&ranges, false);
    if (!smu_range_request(&ranges, range, SMU_RANGE_REASON_USER))
        return (ranges.error == SMU_RANGE_ERR_BAD_REQUEST) ? SMU_ERR_ARG : SMU_ERR_STATE;
    return SMU_OK;
}

void smu_set_current_autorange(bool enabled) {
    g.current_autorange = enabled;
    if (g.state == SMU_STATE_NORMAL)
        smu_range_set_current_autorange(&ranges, enabled);
}

void smu_set_voltage_autorange(bool enabled) {
    g.voltage_autorange = enabled;
    if (g.state == SMU_STATE_NORMAL)
        smu_range_set_voltage_autorange(&ranges, enabled);
}

smu_status_t smu_set_voltage_range(smu_voltage_range_t range) {
    if (g.state != SMU_STATE_NORMAL)
        return SMU_ERR_STATE;
    if (!smu_range_request_voltage(&ranges, range))
        return (ranges.error == SMU_RANGE_ERR_BAD_REQUEST) ? SMU_ERR_ARG : SMU_ERR_STATE;
    smu_set_voltage_autorange(false);
    return SMU_OK;
}

smu_range_config_t* smu_range_config(void) {
    return &ranges.cfg;
}
