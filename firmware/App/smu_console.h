#ifndef SMU_CONSOLE_H
#define SMU_CONSOLE_H
#include "ads131m03.h"
#include "smu_cal_capture.h"
#include "smu_types.h"
bool smu_console_init(void);
/* Foreground bench tuning, in uncalibrated ADC codes. */
smu_cal_capture_config_t* smu_console_calibration_config(void);
void smu_console_process(void);
void smu_console_acquisition_gap(void);
/* Foreground only, once per accepted, settled measurement frame. */
void smu_console_frame(const ads131m03_dma_frame_t* frame, smu_current_range_t irange, smu_voltage_range_t vrange);
#endif
