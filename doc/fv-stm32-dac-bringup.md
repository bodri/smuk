# Temporary STM32 DAC FV control

The firmware now contains a HAL-agnostic FV controller and an STM32H503 DAC1/channel-2 backend. It uses PA5 for the analog output. The existing ADS131M03 acquisition and measurement calibration remain independent.

**Current PA board has ENABLE tied to VCC and no relay.** Both controls are stubbed. `OUTPUT OFF` and fault handling attempt a neutral DAC command only; they cannot disable or isolate the PA. If the DAC write fails, the MCU hangs, or the MCU resets, firmware cannot guarantee zero output. Independent hardware current limits remain essential. `SOURCE? pa_always_enabled=1` reports this limitation.

**Current default: bench DAC backend enabled on PA5.** LD2 is disabled in the updated CubeMX project and its BSP initialization is absent. The user has freed PA5; SB6 must be open so the LED driver does not load the unbuffered DAC. Startup loads the neutral DAC code (approximately 1.5 V at PA5) before enabling DAC1 channel 2, requesting nominal 0 V at the DUT. Nonzero output requires explicit `SOURCE:V` and `OUTPUT ON` commands. The PA itself remains physically enabled because ENABLE is tied to VCC.

## Information needed to activate it

- Installed mapping: buffered DAC → INA105 (2 × VDAC − 3 V) → PA (gain 5). Overall DUT gain is 10 V/V with neutral DAC voltage 1.5 V. General transfer function: `V_DUT = gain × (V_DAC − zero_dac_V)`.
- The measured DAC reference/analog supply voltage.
- ENABLE is currently tied to VCC; no PA GPIO is required while `SMU_FV_PA_ENABLE_STUB=1`. No relay GPIO is required while its stub is selected.
- Confirmed independent positive and negative hardware current limits. Firmware does not program the LT1970A current-limit inputs in this prototype.
- Conservative voltage bounds, slew rate, and software supervisory current threshold.
- Confirmation that connecting a charged or externally driven DUT at zero PA command is permitted by the intended output sequence.

Zero PA command is not guaranteed to equal measured zero DUT voltage. `SOURCE? applied_V` reports the nominal applied command, not feedback-verified source accuracy. Source-path calibration is separate from the existing measurement calibration.

## CubeMX and hardware setup

1. Disable LD2 in CubeMX and open SB6 to disconnect its driver. Reserve PA5 for DAC1_OUT2; remove other uses of that pin. The STM32 port sets it to analog mode and owns DAC startup via ST LL calls.
2. No GPIOs are required for the current ENABLE/relay stubs. For a later hardware revision, configure the confirmed ENABLE and relay pins as GPIO outputs with safe OFF initial states. Ensure their GPIO clocks are enabled. Do not guess active levels or reuse a pin assigned to range/ADC/UART hardware.
3. Keep the source enable physically inhibited while checking DAC startup with a voltmeter/scope.
4. Configure DAC1 channel 2 for normal external unbuffered output, no DMA and no trigger if using CubeMX DAC generation. The current LL backend resets and configures DAC1 itself; do not use DAC1 channel 1 simultaneously without first changing ownership to preserve both channels.
5. CubeMX may add HAL DAC files; they are not required by this LL backend. The imported `Platform/STM32H503/vendor/stm32h5xx_ll_dac.h` is ST's official v1.7.0 header matching the installed HAL version.
6. Refresh and clean/build the CubeIDE project after adding the new files. CMake collects them automatically. Never manually change generated initialization outside USER CODE blocks.

### Board definitions

Review the bench defaults in `smu_fv_board.h` and override the remaining definitions as needed; `SMU_FV_HW_CONFIRMED` is already 1. Add reviewed definitions to `firmware/Platform/STM32H503/smu_fv_board.h` before its final `#endif` (or provide equivalent build definitions). The defaults and meanings are described below:

| Definition | Meaning |
|---|---|
| `SMU_FV_HW_CONFIRMED` | Set to 1 only after completing wiring/scaling review |
| `SMU_FV_PA_ENABLE_STUB` | Defaults to 1; ENABLE is tied to VCC and writes are no-ops |
| `SMU_FV_PA_PORT`, `SMU_FV_PA_PIN` | Required only when ENABLE stub is disabled |
| `SMU_FV_PA_ACTIVE_HIGH` | 1 for active high, 0 for active low |
| `SMU_FV_DAC_BUFFERED` | Defaults to 0 (unbuffered), enabling a nominal zero command |
| `SMU_FV_RELAY_STUB` | Defaults to 1; relay writes are no-ops and provide no isolation |
| `SMU_FV_RELAY_PORT`, `SMU_FV_RELAY_PIN` | Confirmed relay GPIO port/pin; required only with relay stub disabled |
| `SMU_FV_RELAY_ACTIVE_HIGH` | 1 for active high, 0 for active low |
| `SMU_FV_REFERENCE_V` | Measured DAC reference voltage |
| `SMU_FV_ZERO_DAC_V` | DAC voltage requesting nominal zero output |
| `SMU_FV_GAIN` | Signed DUT-volts per DAC-volt |
| `SMU_FV_MIN_V`, `SMU_FV_MAX_V` | Conservative allowed output command range |
| `SMU_FV_SLEW_V_S` | Maximum requested command slew, volts/second |
| `SMU_FV_MAX_CURRENT_A` | Software absolute-current trip threshold; not analog compliance |
| `SMU_FV_EXTERNAL_LIMITS_CONFIRMED` | Must be 1; independently established source/sink limits required |

Use floating-point constants where appropriate. GPIOs must be configured by CubeMX before `smu_init`. ENABLE must have a physical safe-default pull resistor. Recheck the actual wiring and GPIO electrical requirements before assigning pins. The firmware cannot verify those facts.

The default unbuffered DAC permits nominal 0 V through the reference voltage. With the installed converter, signed DUT commands from −15 to +15 V are supported. Buffered mode, if selected, restricts the DAC to 0.2 V through `reference_V − 0.2 V`. Requests outside either the configured source range or that DAC interval are rejected, not clipped. Check the buffered-output load and voltage specifications for the actual supply and load.

## Serial commands

| Command | Behavior |
|---|---|
| `SOURCE?` | FV state, requested and nominal applied voltage, DAC code, backend |
| `SOURCE:V <volts>` | Stage a finite bounded setpoint; active output slews toward it |
| `OUTPUT ON` | Begin output sequence only when prerequisites are met |
| `OUTPUT OFF` | Request neutral DAC voltage and reset target to zero; real ENABLE/relay controls are used only when fitted |

State numbers: 0 unavailable (backend disabled or initialization failed), 1 OFF, 2 STARTING, 3 ACTIVE, 4 TRIPPED. A trip is latched in this source controller. Correct the cause and explicitly issue `OUTPUT OFF`; it never automatically resumes. Instrument faults continue to use the existing fault handling and may require reboot.

Before ON, select both fixed ranges, with current range **1.5 A** and voltage range **6 V or 15 V** for FV operation, set both autoranges OFF and select HIGHZ input. Wait for settled valid measurement and a confirmed nominal ADC rate. Calibration sessions must be ended with `CAL:ABORT` before sourcing.

Example after confirmed hardware is configured:

```text
OUTPUT OFF
CAL:ABORT
AUTORANGE:I OFF
AUTORANGE:V OFF
RANGE:I 1.5A
RANGE:V 15V
IMPEDANCE HIGHZ
STATUS?
ACQ?
SOURCE:V 0.5
OUTPUT ON
SOURCE?
MEAS?
OUTPUT OFF
```

Wait for ranges and acquisition to settle before `OUTPUT ON`. Firmware starts at zero command, enables PA, waits 10 ms, closes the relay, waits another 10 ms, then slews toward the requested setpoint. The delays and externally energized-DUT behavior require bench validation; they are not a general-purpose hot-plug sequence.

While source output is starting or active, console calibration, autorange, range, and impedance changes are rejected. Source-safe shunt switching is not implemented. The 10 MΩ path must be OFF.

Unhealthy acquisition, instrument faults, range changes, overload, excessive measured current, or DAC failure disable the PA and isolate the output. Setpoint and supervisory checks run at most once per millisecond; acquisition-gap and instrument-fault hooks trip immediately when detected. Software reaction time is not a replacement for the independent analog limiter. Range/calibration procedures require `OUTPUT OFF` first.

## Verification and limitations

- Host tests cover startup/relay ordering, fixed-range interlocks, finite bounds, DAC quantization, slew, timer wrap, shutdown, and write failures.
- Existing measurement/acquisition tests remain required.
- The unbuffered DAC has higher output impedance; verify input loading, settling, and actual residual DUT voltage at the neutral code. Do not add a large capacitor directly to the DAC pin without checking load limits.
- Test DAC startup, reset, watchdog, and debugger halt behavior on the actual hardware before connecting a DUT. Hardware ENABLE defaults/interlocks must provide the required protection if software stops executing.
- Verify both polarities using conservative external current limits and reference instruments.
- Confirm measured current and voltage, acquisition counters, and PA temperature throughout transitions.
- Do not expose automatic reconnect/output restoration in the desktop. Initial commands are available through a serial terminal; dedicated desktop source controls are deferred until wiring and bench behavior are confirmed.
- This version contains no digital servo, FI mode, independently programmable compliance references, or source calibration persistence.

References: [STM32H503 datasheet](https://www.st.com/resource/en/datasheet/stm32h503rb.pdf) for PA5/DAC1_OUT2 and DAC limits; [ST v1.7.0 DAC LL header](https://github.com/STMicroelectronics/stm32h5xx-hal-driver/blob/v1.7.0/Inc/stm32h5xx_ll_dac.h).

Current hardware revision: INA105 converter installed, PA gain = 5, overall gain = 10, neutral DAC voltage = 1.5 V, no output relay. Gain, zero and relay-stub defaults are already configured. Reference, conservative bounds and hardware-limit configuration remain required; the user has confirmed independent hardware current limiting. GPIOs are not required for either stub. Relay sequencing delays remain in place for compatibility, but no physical contact opens or closes in stub mode.

With the always-enabled PA board, firmware boots the voltage input in HIGHZ and rejects 10 MΩ loading, even when the source controller is logically OFF. Physical PA disable, safe reset behavior and isolation must be implemented in hardware before treating this as a finished SMU. Bench bring-up should use a passive load; the zero-command/relay-stub sequence is not validated for an externally energized DUT.

Bench defaults are now supplied: nominal DAC reference 3.3 V, DUT output requests −15 to +15 V, slew 10 V/s, software current trip at 10 mA. Independent hardware limits are confirmed. `SMU_FV_HW_CONFIRMED` now defaults to 1 for PA5 bench bring-up. Set it to 0 to disable the DAC backend. Neither ENABLE nor relay GPIOs are required with both stubs selected. Verify the PA5/reference/load conditions with the PA disconnected first. These defaults do not establish calibrated source accuracy.

DAC ownership: `Core/Src/dac.c` returns from `MX_DAC1_Init` inside its USER CODE block. The FV platform backend initializes DAC1 after UART transport starts and selects HCLK-compatible high-frequency mode. Keep this USER CODE block when regenerating CubeMX; do not start the DAC through a second HAL handle.

Desktop FV control uses `SOURCE?` metadata: `minimum_V`, `maximum_V`, `slew_V_s`, `pa_always_enabled`, and `relay_available`. The −15 to +15 V limits refer to DUT commands, not DAC pin voltage. PA5 = 1.5 V + V_DUT / 10. The desktop reads these signed bounds from firmware after reconnect; no new desktop build is needed.

## Installed INA105 converter: first bipolar bench check

The default mapping now assumes the converter is physically installed. It is
incompatible with the previous direct DAC-to-PA wiring. The independent hardware
current limits, 10 V/s slew and 10 mA software supervisory threshold remain.

With PA power removed, flash and reboot, then verify PA5 near 1.5 V and INA105
output near 0 V before powering or connecting the PA. Inspect startup/reset with
a scope if the converter remains connected. Preloading the DAC holding register
does not control the analog pin during reset or before firmware executes. Since
ENABLE and relay are still stubbed, firmware cannot guarantee safe startup or
physical isolation. A hardware command clamp or controllable ENABLE is still needed.

| DUT target | Nominal PA5 voltage | Nominal INA105 output |
|---:|---:|---:|
| −1 V | 1.40 V | −0.20 V |
| −0.5 V | 1.45 V | −0.10 V |
| 0 V | 1.50 V | 0 V |
| +0.5 V | 1.55 V | +0.10 V |
| +1 V | 1.60 V | +0.20 V |

At nominal 3.3 V reference, the neutral 12-bit code is 1861. Quantization alone
means this is not exactly 1.500000 V; measure residual output rather than treating
`applied_V=0` as calibrated physical zero. Source-path calibration is deferred.

After verifying the neutral command, use a passive load with conservative hardware
limits. Select fixed 1.5 A / (6 V or 15 V) measurement ranges, both autoranges OFF, HIGHZ,
and wait for settled valid acquisition. Test 0 V, +0.5 V, −0.5 V and OUTPUT OFF.
The nominal ramp takes 0.5 seconds from 0 to ±0.5 V, 1 second from 0 to ±1 V,
and 2 seconds from −15 to +15 V. Allow additional analog settling and confirm
`SOURCE? applied_V` has reached `requested_V` before comparing voltages. Compare PA5, converter output, DUT voltage and MEAS?. Check
STATUS? and ACQ? for faults, CRC errors and gaps after each step. OUTPUT OFF and
faults request the same 1.5 V neutral DAC command; they do not switch off the PA.

## Diagnosing source gain or offset errors

`SOURCE? applied_V` is the nominal drive command, not measured DUT voltage. At
steady commands −1, 0, and +1 V, record SOURCE?, MEAS?, a reference DMM reading,
PA5 voltage, and INA105 output. Expected PA5 values are 1.40, 1.50, and 1.60 V;
expected converter outputs are −0.20, 0, and +0.20 V. Confirm the sign of each
measured output and that the source is ACTIVE rather than TRIPPED. Check hardware
current limiting and the load before attributing an error to gain. Do not change
the nominal gain from two readings until the DAC, converter and PA stages have
been separated and the ramp is complete. Measurement calibration and source-path
calibration are independent.

FV accepts either fixed voltage-measurement range in firmware and desktop. The 6 V feedback gain is 2.5 times the 15 V gain, so firmware uses overall DAC-to-DUT gain 4 rather than 10, preserves the trimmed DAC neutral, and scales the advertised source limits to approximately −5.9792 to +6 V. Switch ranges with OUTPUT OFF, then select a new setpoint. Setpoints remain bounded to the nominal hardware range ±15 V; this is not a guarantee of full endpoint voltage under every load. The 10 mA software supervisory trip and independent hardware current limits are unchanged. Start low and verify endpoints with a reference instrument and suitable load before using the full range.

## Measured zero and optional voltage correction

The hardcoded +52 mV trim has been removed. `SMU_FV_ZERO_OFFSET_V` defaults to
zero; measurement calibration and Flash records are unchanged. Learned source
neutrals are separate for 6 V and 15 V and last until reboot. Startup uses the
nominal unlearned neutral, so OFF can have a residual until zero is learned.

1. Disconnect external voltage sources and leave the output unloaded. OFF on
   this board commands zero; it cannot isolate or disable the PA.
2. Select HIGHZ, fixed current range 1.5 A, either fixed voltage range, and both
   autoranges OFF. Finish measurement calibration and wait for valid readings.
3. Send `OUTPUT OFF`, then `SOURCE:ZERO`. Alternatively open SET in the desktop
   and select **Learn source zero…**, confirming the unloaded setup.
4. Poll `SOURCE?`. State 5 means learning; state 1 plus `zero_valid=1` means
   success. `OUTPUT OFF` cancels. A fault, range change, excessive error,
   compliance indication, unstable data or a 60-second timeout prevents learning.
   A failed/cancelled attempt retains the previous accepted neutral.
5. Verify zero with an external meter. Repeat for the other voltage range.

Learning averages 16 non-overlapping precision windows after settling, then
adjusts by at most one DAC step per iteration. It accepts stable readings within
0.65 DAC step of zero. Corrections are bounded to 0.2 V referred to DUT output;
a large measured voltage is rejected, but firmware cannot distinguish a small
external voltage from an offset. Never learn with an external source attached.
Learning on future ENABLE/relay hardware is inhibited until a dedicated connected
learning sequence exists.

Optional voltage correction is disabled at boot. With OUTPUT OFF, send
`SOURCE:REGULATION ON` (or use **Enable bench voltage correction…** in SET), set
`SOURCE:V`, and enable output. After ramping and settling, calibrated precision
voltage closes a slow integral correction loop. Updates are separated by at
least 100 ms plus the precision window, use each sample only once, and are
bounded to one DAC step per update and ±0.2 V total. The source endpoint bounds
still apply. `SOURCE:REGULATION OFF` is accepted with OUTPUT OFF.

`SOURCE?` reports `regulation`, `limited`, `zero_valid` and `correction_V`.
`requested_V` and `applied_V` remain nominal DUT ramp targets; `correction_V`
reports the added command. The measured voltage remains the accuracy reference.
A new setpoint or OFF clears the integral correction, while accepted zero trims
remain. Neither enabling correction nor reconnecting automatically enables output.

Compliance or current above 80% of the software trip threshold freezes
correction; invalid acquisition retains the existing source trip behavior.
**The LT1970A compliance pin is not connected yet.** Its current limiting may
therefore be invisible to firmware. Use correction as an explicit bench feature
within independently set analog current limits. `limited=1` also reports a
correction/endpoint bound; it is not proof of hardware compliance.

DAC granularity remains about 8.06 mV at the DUT on 15 V and 3.22 mV on 6 V.
The deadband avoids chasing errors smaller than useful DAC resolution. Validate
positive/negative setpoints and load changes with an external meter before use.
