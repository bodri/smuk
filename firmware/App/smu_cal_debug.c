#include "smu_cal_debug.h"
#include "smu_cal_seq.h"
#include "smu_calibration.h"

/* Globals are intentionally non-static: they are the debugger interface. */
smu_cal_seq_t cal_seq;
volatile bool test_start_vcal = false;

volatile bool test_vcal_capture_gnd = false;
volatile bool test_vcal_capture_1v5 = false;
volatile bool test_vcal_capture_3v0 = false;

volatile int vcal_active_point = -1;

/* Uncalibrated nominal VMEAS */
volatile float vcal_x[3] = {0.0f, 0.0f, 0.0f};

/* Calibrated CALBUS reference */
volatile float vcal_y[3] = {0.0f, 0.0f, 0.0f};

volatile bool vcal_point_valid[3] = {false, false, false};

volatile float vcal_fit_gain = 1.0f;
volatile float vcal_fit_offset = 0.0f;

volatile float vcal_residual[3] = {0.0f, 0.0f, 0.0f};

volatile bool vcal_fit_ready = false;
volatile bool vcal_test_fault = false;

volatile bool test_vcal_commit = false;
volatile bool vcal_commit_ok = false;

static void start_capture(int point, calbus_sel_t bus) {
    vcal_active_point = point;
    vcal_fit_ready = false;
    vcal_test_fault = false;

    if (!smu_cal_seq_start(&cal_seq, CAL_TARGET_VOLTAGE, bus)) {
        vcal_active_point = -1;
        vcal_test_fault = true;
    }
}

void smu_cal_debug_acquisition_gap(void) {
    if (smu_cal_debug_active()) {
        smu_cal_seq_abort(&cal_seq);
        vcal_fit_ready = false;
        vcal_test_fault = true;
    }
}

void smu_cal_debug_init(void) {
    smu_cal_seq_init(&cal_seq);
}

/* Must run once per popped frame, so no frame is double-counted or missed. */
void smu_cal_debug_frame(const ads131m03_dma_frame_t* f) {
    if ((cal_seq.state == CAL_SEQ_DISCARD) || (cal_seq.state == CAL_SEQ_ACQUIRE)) {
        if (cal_seq.target == CAL_TARGET_VOLTAGE) {
            smu_cal_seq_adc_frame(&cal_seq, f->ch1, f->ch2);
        } else {
            smu_cal_seq_adc_frame(&cal_seq, f->ch0, f->ch2);
        }
    }
}

bool smu_cal_debug_active(void) {
    return cal_seq.state != CAL_SEQ_IDLE;
}

bool smu_cal_debug_faulted(void) {
    return cal_seq.state == CAL_SEQ_FAULT;
}

void smu_cal_debug_process(uint32_t elapsed_ms) {
    if (test_start_vcal) {
        test_start_vcal = false;

        (void)smu_cal_seq_start(&cal_seq, CAL_TARGET_VOLTAGE, CALBUS_P1V5);
    }

    if (test_vcal_capture_gnd) {
        test_vcal_capture_gnd = false;
        start_capture(0, CALBUS_0V);
    }

    if (test_vcal_capture_1v5) {
        test_vcal_capture_1v5 = false;
        start_capture(1, CALBUS_P1V5);
    }

    if (test_vcal_capture_3v0) {
        test_vcal_capture_3v0 = false;
        start_capture(2, CALBUS_P3V);
    }

    if (cal_seq.result_ready) {
        cal_seq.result_ready = false;

        if ((vcal_active_point >= 0) && (vcal_active_point < 3)) {
            int p = vcal_active_point;

            /*
             * CH1:
             * Raw ADC average -> ADC volts ->
             * nominal reconstructed SENSE voltage.
             *
             * Do NOT apply stored voltage calibration here.
             */
            vcal_x[p] = smu_calibration_vcal_nominal_voltage(cal_seq.target_average);

            /*
             * CH2:
             * Use already-calibrated CALBUS as our reference.
             */
            vcal_y[p] = smu_calibration_vcal_calbus_voltage((int32_t)cal_seq.calbus_average);

            vcal_point_valid[p] = true;

            vcal_active_point = -1;

            /*
             * Automatically calculate the fit once all
             * three manually acquired points exist.
             */
            if (vcal_point_valid[0] && vcal_point_valid[1] && vcal_point_valid[2]) {
                float gain = 0.0f, offset = 0.0f, residual[3] = {0.0f, 0.0f, 0.0f};

                if (smu_calibration_vcal_fit(vcal_x, vcal_y, &gain, &offset, residual)) {
                    vcal_fit_gain = gain;
                    vcal_fit_offset = offset;
                    vcal_residual[0] = residual[0];
                    vcal_residual[1] = residual[1];
                    vcal_residual[2] = residual[2];
                    vcal_fit_ready = true;
                } else {
                    vcal_test_fault = true;
                }
            }
        }
    }

    if (test_vcal_commit) {
        test_vcal_commit = false;
        vcal_commit_ok = false;

        /*
         * Only allow commit after a successful complete
         * three-point calibration.
         */
        if (vcal_fit_ready && vcal_point_valid[0] && vcal_point_valid[1] && vcal_point_valid[2] && !vcal_test_fault && !cal_seq.fault) {
            vcal_commit_ok = smu_calibration_vforce_commit(vcal_fit_gain, vcal_fit_offset);
        }
    }

    smu_cal_seq_tick_elapsed_ms(&cal_seq, elapsed_ms);
}
