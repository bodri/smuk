#ifndef SMU_FV_BOARD_H
#define SMU_FV_BOARD_H
/* Bench DAC enabled on freed PA5; starts with a nominal zero command.
 * Do not use the legacy software-only safety mocks to enable physical output.
 * See doc/fv-stm32-dac-bringup.md for the required definitions. */
#ifndef SMU_FV_HW_CONFIRMED
#define SMU_FV_HW_CONFIRMED 1
#endif
/* Confirmed PA mapping: gain 5, direct unipolar command, no output relay fitted yet. */
#ifndef SMU_FV_GAIN
#define SMU_FV_GAIN 5.0f
#endif
#ifndef SMU_FV_ZERO_DAC_V
#define SMU_FV_ZERO_DAC_V 0.0f
#endif
#ifndef SMU_FV_DAC_BUFFERED
#define SMU_FV_DAC_BUFFERED 0
#endif
#ifndef SMU_FV_RELAY_STUB
#define SMU_FV_RELAY_STUB 1
#endif
#ifndef SMU_FV_PA_ENABLE_STUB
/* Current board ties ENABLE to VCC. OFF is a zero command, not shutdown. */
#define SMU_FV_PA_ENABLE_STUB 1
#endif

/* Conservative bench defaults; reference is nominal, not source calibration. */
#ifndef SMU_FV_REFERENCE_V
#define SMU_FV_REFERENCE_V 3.3f
#endif
#ifndef SMU_FV_MIN_V
#define SMU_FV_MIN_V 0.0f
#endif
#ifndef SMU_FV_MAX_V
#define SMU_FV_MAX_V 1.0f
#endif
#ifndef SMU_FV_SLEW_V_S
#define SMU_FV_SLEW_V_S 0.1f
#endif
#ifndef SMU_FV_MAX_CURRENT_A
#define SMU_FV_MAX_CURRENT_A 0.01f
#endif
#ifndef SMU_FV_EXTERNAL_LIMITS_CONFIRMED
/* User confirmed independent hardware current limiting on the tested PA. */
#define SMU_FV_EXTERNAL_LIMITS_CONFIRMED 1
#endif
#endif
