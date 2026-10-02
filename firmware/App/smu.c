#include "smu.h"
#include "ads131m03.h"
#include "ads131m03_port.h"
#include "calibration_store.h"
#include "safety_hw.h"
#include "smu_cal_debug.h"
#include "smu_calibration.h"
#include "smu_console.h"
#include "smu_log.h"
#include "smu_port.h"
#include <string.h>

static smu_context_t g;
static smu_range_manager_t ranges;
static uint32_t last_ms;
static smu_acquisition_t acquisition;
static bool calibration_save_begin(void);
static void calibration_save_end(void);

static smu_acquisition_counters_t acquisition_counters(void) {
    const ads131m03_dma_status_t* s = ads131m03_dma_get_status();
    return (smu_acquisition_counters_t){.frames = s->frame_count, .crc_errors = s->crc_error_count, .spi_errors = s->spi_error_count, .busy = s->dma_busy_count, .overruns = s->ring_overrun_count};
}

/* Non-static for debugger watch windows (previously in main.c). */
ads131m03_bringup_result_t ads_result;
const ads131m03_dma_status_t* ads_dma_status = NULL;
ads131m03_dma_frame_t adc_frame;
smu_measurement_outputs_t smu_outputs;

static void fault(uint32_t bits) {
    if ((g.faults & bits) != bits)
        (void)smu_log_printf("SMU fault bits=0x%08lX\r\n", (unsigned long)bits);
    smu_cal_debug_acquisition_gap();
    smu_console_acquisition_gap();
    safety_hw_disable_pa();
    smu_range_disconnect_input(&ranges);
    g.faults |= bits;
    g.state = SMU_STATE_FAULT;
    smu_measurement_set_valid(false);
    g.measurement_valid = false;
    smu_measurement_get_outputs(&smu_outputs);
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
    smu_calibration_set_save_hooks(calibration_save_begin, calibration_save_end);
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
    smu_acquisition_init(&acquisition, last_ms, acquisition_counters());
    smu_measurement_set_fresh(false);
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

        smu_range_update_current_overload(&ranges, f.ch0);
        if (!smu_measurement_process_frame(&mf))
            continue;
        smu_measurement_get_outputs(&smu_outputs);
        smu_console_frame(&f, ranges.active, ranges.vactive);

        if (g.state == SMU_STATE_NORMAL) {
            smu_range_current_autorange_frame(&ranges, f.ch0, smu_outputs.fast.current_A);
            const float voltage = smu_measurement_voltage_from_code(f.ch1, ranges.vactive);
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

static void update_measurement_context(void) {
    smu_measurement_get_outputs(&smu_outputs);
    g.precision_ready = smu_outputs.precision_ready;
    g.measurement_fresh = smu_outputs.fast.fresh;
    g.measurement_settled = smu_outputs.fast.settled;
    g.current_clipped = smu_outputs.fast.current_clipped;
    g.voltage_clipped = smu_outputs.fast.voltage_clipped;
    g.calbus_clipped = smu_outputs.fast.calbus_clipped;
    g.current_overload = smu_outputs.fast.current_overload;
    g.voltage_overload = smu_outputs.fast.voltage_overload;
    g.overload = smu_outputs.fast.overload;
    g.measurement_valid = g.state == SMU_STATE_NORMAL && ranges.measurement_valid && !smu_range_busy(&ranges) && smu_outputs.fast.valid;
}

static void discard_queued_frames(void) {
    ads131m03_dma_frame_t dropped;
    while (ads131m03_dma_pop(&dropped)) {
    }
}

static void calibration_save_end(void) {
    discard_queued_frames();
    const uint32_t now = smu_port_millis();
    smu_acquisition_resume(&acquisition, now, acquisition_counters());
    last_ms = now;
    smu_range_resume_measurement(&ranges);
    smu_measurement_set_fresh(false);
    g.acquisition_stale = true;
    g.measurement_age_ms = 0;
    update_measurement_context();
    ads131m03_port_drdy_enable(true);
}

static bool calibration_save_begin(void) {
    if (g.state != SMU_STATE_NORMAL || smu_range_busy(&ranges) || smu_cal_debug_active() || safety_hw_pa_requested())
        return false;
    ads131m03_port_drdy_enable(false);
    smu_measurement_set_fresh(false);
    smu_measurement_reset_filters();
    smu_range_acquisition_gap(&ranges);
    smu_console_acquisition_gap();
    update_measurement_context();
    ++g.acquisition_pause_count;
    const uint32_t start = smu_port_millis();
    /* Interrupts stay enabled so an in-flight SPI/DMA transfer can complete. */
    while (ads131m03_port_spi_dma_busy() || ads_dma_status->dma_active) {
        if (smu_port_millis() - start >= 10u) {
            fault(SMU_FAULT_ADC);
            return false; /* Keep acquisition gated; no Flash write was attempted. */
        }
    }
    discard_queued_frames();
    return true;
}

void smu_process(void) {
    if (g.state == SMU_STATE_POWER_UP)
        return;

    const uint32_t now = smu_port_millis();
    smu_acquisition_update(&acquisition, now, acquisition_counters());
    if (acquisition.gap) {
        /* Counter-only diagnostics cannot locate a gap within queued frames.
         * Conservatively drop the backlog; recovery requires new ADC progress. */
        discard_queued_frames();
        smu_measurement_reset_filters();
        smu_range_acquisition_gap(&ranges);
        smu_console_acquisition_gap();
        smu_cal_debug_acquisition_gap();
    }
    smu_measurement_set_fresh(!acquisition.stale);
    if (acquisition.fault && g.state != SMU_STATE_FAULT)
        fault(SMU_FAULT_ADC);
    if (!acquisition.gap)
        process_frames();

    const uint32_t elapsed_ms = now - last_ms;
    last_ms = now;

    if (g.state != SMU_STATE_FAULT)
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
    update_measurement_context();
    g.measurement_age_ms = acquisition.age_ms;
    g.acquisition_gap_count = acquisition.gap_count;
    g.acquisition_stale = acquisition.stale;
    g.adc_crc_errors = acquisition.previous.crc_errors;
    g.adc_spi_errors = acquisition.previous.spi_errors;
    g.adc_busy_count = acquisition.previous.busy;
    g.adc_overruns = acquisition.previous.overruns;
    g.range_switch_count = ranges.switch_count;
}

void smu_get_measurement(smu_measurement_outputs_t* out) {
    smu_measurement_get_outputs(out);
}

const smu_context_t* smu_get_context(void) {
    update_measurement_context();
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
    if (!smu_range_request(&ranges, range, SMU_RANGE_REASON_USER))
        return (ranges.error == SMU_RANGE_ERR_BAD_REQUEST) ? SMU_ERR_ARG : SMU_ERR_STATE;
    smu_set_current_autorange(false);
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

smu_status_t smu_set_integration_ms(uint16_t milliseconds) {
    if (milliseconds != 1u && milliseconds != 8u && milliseconds != 20u && milliseconds != 50u && milliseconds != 100u)
        return SMU_ERR_ARG;
    if (g.state != SMU_STATE_NORMAL || smu_range_busy(&ranges))
        return SMU_ERR_STATE;
    const uint16_t samples = (uint16_t)(milliseconds * (SMU_MEASUREMENT_SAMPLE_RATE_HZ / 1000u));
    if (!smu_measurement_set_precision_samples(samples))
        return SMU_ERR_ARG;
    update_measurement_context();
    return SMU_OK;
}

smu_acquisition_config_t* smu_acquisition_config(void) {
    return &acquisition.cfg;
}

smu_range_config_t* smu_range_config(void) {
    return &ranges.cfg;
}
