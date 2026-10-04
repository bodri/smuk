# ST-LINK serial console

Connect the NUCLEO-H503RB's **ST-LINK USB connector CN1** to your computer.
The default VCP routing is USART3, PA4 TX / PA3 RX. Use **115200 baud,
8 data bits, no parity, 1 stop bit, no flow control**. No external serial
adapter or target USB firmware is required.

On macOS:

```sh
ls /dev/cu.usbmodem*
 /dev/cu.usbmodemYOUR_DEVICE 115200
```

Exit screen with Ctrl-A then backslash. The startup message appears after reset;
if the terminal connects after startup, send `PING` or `HELP`. Enable the
terminal's local echo, or send `ECHO ON` (use one echo option). Commands are
case insensitive and end with CR, LF, or CRLF. Only one terminal should open
the port at a time.

The HAL port uses fixed-size RX/TX queues and USART3 interrupts at priority 5,
below ADC interrupts. Parsing and command execution run in the foreground.
Receive loss and overlong lines discard input through a newline; resend the
whole command. TX overflow is reported when queue space becomes available.
Requests are not streamed continuously: query at a modest rate and wait for
responses. `printf` still uses the BSP's blocking UART path; avoid mixing it
with console traffic. Flash saves are synchronous and explicitly pause acquisition; wait for the
save response and fresh valid measurements afterward. The ADC configuration
and conversion clock continue unchanged during a save.

## Commands

| Command | Result |
| --- | --- |
| `PING` | `PONG` |
| `HELP` | Command summary |
| `ECHO ON` / `ECHO OFF` | Device input echo; default off |
| `STATUS?` | State, faults, ranges, autorange, validity, frame count, capture status, then a `QUALITY` line |
| `ACQ?` | Acquisition age/staleness, gap and intentional pause counts, CRC/SPI errors, busy events and overruns |
| `MEAS?` | Precision measurements, validity, and populated/configured window size |
| `RAW?` | Latest raw CH0/CH1/CH2 codes; these are not averaged |
| `RANGE:I 1.5A` / `100MA` / `10MA` / `1MA` / `100UA` | Request fixed current range and disable autorange |
| `RANGE:V 15V` / `6V` | Request fixed voltage range and disable voltage autorange |
| `AUTORANGE ON` / `OFF` or `AUTORANGE:I ON` / `OFF` | Control current autorange |
| `AUTORANGE:V ON` / `OFF` | Control voltage autorange independently |
| `IMPEDANCE 10M` / `HIGHZ` | Select differential input loading; wait for `valid=1` |
| `INTEGRATION 1MS` / `8MS` / `20MS` / `50MS` / `100MS` | Select precision integration window; default 20 ms |
| `CAL:SHOW?` | Coefficients, dirty flag, Flash sequence, FLASH/DEFAULTS provenance and capture limits |
| `CAL:BEGIN V` / `I` / `BUS` | Start a point set for the selected fixed range/channel |
| `CAL:CAPTURE <reference>` | Average 2,048 fresh accepted raw ADC samples at 32 kSPS (256 in the 4 kSPS fallback); reference in volts for V/BUS or amperes for I |
| `CAL:POINTS?` | Show readings, references and capture noise/drift |
| `CAL:FIT` | Check point span, fit gain/offset and stage them; show residuals |
| `CAL:SAVE` | Persist staged fits and apply them; requires a dirty record and stable fixed ranges |
| `CAL:ABORT` | Stop capture and clear its point set; retain previously staged fits |
| `CAL:RESET` | Stop capture, discard all staged fits and reload active calibration |

Manual captures **do not select CALBUS or operate calibration relays**. Apply
references using the normal measurement input path, or an externally arranged
CALBUS connection. The entered reference must be independently measured, not
the nominal CALBUS label or the SMU's already calibrated display.

## Measurement quality and acquisition health

`STATUS?`, `MEAS?`, and `RAW?` include a `QUALITY` line. `fresh` means a sample
is available and acquisition is current; `settled` means that the measurement
pipeline is outside a settling transition. `valid=1` requires measurement enabled,
fresh and settled data, and neither current nor voltage overload. Faults and
transitions immediately invalidate cached readings. Numerical values remain
available for diagnostics when invalid; do not use those values for control.

`I_clip` and `V_clip` report raw ADC magnitude at or above 8,220,000 codes
(about 98% full scale), regardless of autorange. `I_overload` additionally
includes the existing 105% current-range threshold applied to calibrated current; `V_overload` reports
voltage ADC clipping. CALBUS clipping is reported independently as `BUS_clip`
and does not invalidate otherwise usable I/V measurements. Precision CALBUS
remains flagged until any clipped sample has left its averaging window.
Clipped/overloaded I/V samples are excluded from filter history.

`MEAS? valid=1` and `precision_ready=1` require the complete configured precision
window. `STATUS? valid` and `RAW? valid` describe the fast path, which is ready
sooner. `MEAS? window=<populated>/<configured>` reports progress explicitly.
Precision values during warm-up remain available but invalid. Integration
changes, range switches, acquisition gaps and calibration application reset
history. Conversion equations remain unchanged.

The foreground health monitor uses the existing ADC counters without changing
SPI/DMA configuration or ISR behavior. Defaults are:

- No clean acquisition progress for 20 ms: invalidate readings and reset filters
  and autorange persistence. New clean frames recover automatically.
- CRC/SPI errors, ring overruns, or a foreground monitoring pause
  of at least 20 ms: discard queued frames of uncertain continuity and reset
  measurement history. Active manual captures are discarded; an active debugger
  calibration sequence aborts with a calibration fault.
- No clean acquisition progress for 1 second, or ten transport errors within a
  fixed 1-second monitoring window: latch an ADC fault, request PA disable, and
  disconnect the 10 MΩ load. Reboot is currently required to clear the fault.

`ACQ?` exposes `age_ms`, `stale`, `gaps`, `pauses`, `crc`, `spi`, `busy`, and `overruns`.
DMA-busy events count skipped DRDY triggers while a transfer is active. They
remain diagnostic and do not alone discard clean frames or latch a fault.
A rising busy count can indicate reduced sampling throughput; verify the actual
accepted sample rate before relying on the nominal integration times for mains rejection.
Age is measured from foreground observation of clean ADC progress, not a
hardware timestamp for each frame. The monitor conservatively drops queued
frames after a long foreground pause. Thresholds can be tuned through
`smu_acquisition_config()`; keep all configured limits positive.

## Precision integration

Select `INTEGRATION 1MS`, `8MS`, `20MS`, `50MS`, or `100MS`. The boot default is
**20 ms**. At 32 kSPS these include 32, 256, 640, 1,600, and 3,200 raw samples.
Every group of eight samples contributes its mean to a 4 kHz rolling precision
window, keeping RAM use bounded. Group counts are 4, 32, 80, 200, and 400.
`MEAS? window=80/80 rate_hz=32000 group_samples=8 integration_ms=20` therefore
means a full 640-sample, nominal 20 ms window. `samples` counts accepted raw frames.
Precision updates at group boundaries; within a group it retains the last complete
window. Clipped I/V samples reset history immediately; any clipped CALBUS sample
marks its entire group and stays flagged until that group leaves the window.
Running sums use compensated arithmetic to limit accumulation drift. The fast
path processes every accepted raw sample; its default EMA coefficient is adjusted
to preserve the former wall-clock response. Fast filtering is independent of the
precision integration selection.

At exactly 4 kSPS, 20 ms covers one 50 Hz cycle, 50 ms covers three 60 Hz
cycles, and 100 ms covers five 50 Hz or six 60 Hz cycles. Verify the actual
sample-clock rate and interference frequency on the board; rejection depends
on their agreement with the selected window. These are sample-based windows,
not a line-synchronized ADC mode. Integration changes are rejected during a
capture and clear its point set while preserving previously staged fits.
After selecting a window, wait for `MEAS? valid=1` before trusting precision
readings. This setting is volatile and returns to 20 ms after reset.

### 32 kSPS acquisition and bench acceptance

Bring-up writes CLOCK explicitly to `0x0702` (HR mode, all channels enabled,
OSR 128), checks the WREG acknowledgment, and verifies the register readback.
32 kSPS requires **8.192 MHz CLKIN**. SPI speed, framing, output CRC and DMA
callbacks remain unchanged. Confirm CLKIN and DRDY on the actual board; register
readback alone cannot establish the effective sampling rate.

The application acquisition/measurement hot path and ADC driver compile with
`-O2` and debug information. HAL sources and generated interrupt dispatch retain
their original optimization settings. Other code retains its existing flags.
Fast-math is disabled. Debug stepping in optimized functions can skip/reorder source
lines and some local variables may be optimized out. An unoptimized hot path can
overflow the ring at 32 kSPS, repeatedly clear integration history and latch fault 4;
the fault thresholds have not been relaxed to accommodate that condition.

CMake and CubeIDE use separate configuration files and outputs. CMake produces
`build/smuk.elf`; the default CubeIDE launch loads `Debug/smuk.elf`. The local
CubeIDE `.cproject` has matching per-file optimization settings (the file is
ignored by Git). Reopen/refresh the project and clean/rebuild to apply them.
For a fresh CubeIDE project, set `-O2` on the SMU acquisition, measurement,
range, conversion and capture sources and the ADC/range platform wrappers listed
in `CMakeLists.txt`; leave HAL and generated sources unchanged. Startup logs
`SMU processing optimized=1` when `smu.c` was built with optimization; this marker
does not substitute for checking the other hot files' compiler command lines.

`ACQ?` additionally reports `rate_hz` (configured), `frame_hz` (CRC-valid frames
delivered to the ring), `drdy_hz` (observed interrupts), `rate_known`, and `rate_ok`.
Rates use one-second foreground observation windows. Precision is inhibited when
a known delivered frame rate differs by more than 2% from nominal. Fast diagnostics
and autorange remain available. Recovery clears filter history and requires a new
full precision window. Manual captures/fits/saves are rejected while the rate is
known to be wrong. Flash pauses restart the rate observation window. Rate checks
are coarse diagnostics, not proof of uninterrupted delivery: short losses below
the tolerance can escape this check. Before the first window completes, rate is
unverified. Existing CRC, SPI, overrun and stale handling remains in force.

Before accepting the 32 kSPS build:

1. Record the calibrated 4 kSPS baseline at 20 ms: DC readings, zero-input noise,
   reference deviations, ranges, and acquisition counters. Export the calibration
   report/coefficients; do not overwrite the saved record just to change ADC rate.
2. Check CLKIN = 8.192 MHz and DRDY period = 31.25 µs with a scope/logic analyzer.
   Measure CS/SCLK transfer and interrupt turnaround, including console traffic;
   each 15-byte frame must finish before the next conversion with adequate margin.
3. After at least two seconds, verify `ACQ? rate_hz=32000`, `frame_hz` near 32000,
   `rate_known=1 rate_ok=1`, and no increasing CRC/SPI/overrun/gap counters. Inspect
   rising busy counts against the DRDY trace and delivered rate rather than assuming
   every busy edge represents a lost conversion.
4. Check `MEAS? integration_ms=20 window=80/80 group_samples=8 valid=1`; the
   desktop should show 640/640 samples. Exercise all integration selections.
5. Test both autoranges and input-impedance transitions. Discard counts scale eightfold
   to preserve analogue settling: 64/128/320 raw frames for the prior 8/16/40 counts.
   Up-range confirmation is 16 frames; clipping still requests immediate up-ranging.
6. Compare noise and accuracy to the baseline on every calibrated range and at
   independent positive/negative references. Higher-rate ADC samples are noisier;
   equal-duration software averaging does not guarantee identical ADC filtering.
7. Check a manual capture: `CAL:SHOW?` reports `capture_samples=2048 capture_ms=64`.
   Capture duration stays nominally 64 ms; noise/drift limits are unchanged. Inspect
   reported quality before deciding whether calibration needs to be repeated.
8. Run sustained desktop polling, console queries, range transitions and an explicit
   save test if needed; verify no watchdog resets or acquisition faults.

The 4 kSPS fallback preserves the same millisecond integration choices and saved
calibration layout. Build it from `firmware/` with
`ADS131M03_SAMPLE_RATE_HZ=4000 ./build.sh`; a normal `./build.sh` selects 32000.
Do not flash or save calibration automatically during host validation.

## Calibration quality and persistence

`CAL:SHOW? source=FLASH` means a valid record was loaded; `DEFAULTS` means the
instrument is using nominal coefficients. FLASH does not certify that every
range has been calibrated: the version-1 record still has no per-range provenance.
Headers, CRC algorithm, record format and the two Flash slots are unchanged.
Records require finite positive gains, finite offsets, and representable
conversion results. Invalid records are excluded from slot selection; an older
valid slot is used if available, otherwise defaults are applied. Invalid
candidates are rejected before Flash is touched.

Every capture reports RMS sample noise, last-half minus first-half mean drift,
and sample span. `*_nominal` values are in the target channel's uncalibrated
amperes/volts; raw-code metrics are also reported so small current noise remains
visible despite decimal display rounding. Defaults reject RMS noise above
4096 ADC codes or absolute half-to-half drift above 8192 codes. These are initial
bench limits (about 0.049% and 0.098% of positive ADC full scale), not accuracy
specifications. Inspect them and tune with `smu_console_calibration_config()`
when characterizing the board. Debugger acquisition uses the same checks,
with limits in `cal_seq.capture_cfg`, on both target and CALBUS channels.

A failed capture does not add a point. `CAL:FIT` reports nominal/reference spans
and requires nominal span at least 64 ADC codes and 20 times the worst captured
RMS noise. It reports each residual without automatically asserting fit accuracy.
Console and debugger paths share the finite, positive-gain fitter. Review
residuals and verify against references not used for fitting.

Before saving, the application disables new DRDY-triggered DMA starts, waits
up to 10 ms for an in-flight transfer, discards the queued data, and invalidates
measurements. IRQ behavior is unchanged and interrupts remain enabled. After
write/readback success or failure, it discards queued data again, restarts health
monitoring from the expected interruption, and resumes with 40 discarded frames.
The selected ranges, impedance and integration window are retained. Fast
measurements then recover; precision additionally waits for a full window.
Intentional saves increment `ACQ? pauses` rather than creating transport gaps.
A DMA-quiesce timeout prevents the write, latches an ADC fault and leaves new
acquisition disabled until reboot. Saves are rejected while ranges are busy,
a calibration sequence is active, or the PA is requested.

A failed write/readback leaves active RAM coefficients unchanged. The new
record must pass semantic/CRC checks, advance sequence, and match the requested
payload before application. If a write succeeds but its reload cannot be
confirmed, a reboot may load that persisted record; recheck `CAL:SHOW?`.

## Input impedance

Startup drives MV_ON LOW, then connects R26 after ADC initialization. Default
input loading is 10 MΩ between SENSE+ and SENSE− (`MV_ON` HIGH). Select
`IMPEDANCE HIGHZ` to disconnect it, or `IMPEDANCE 10M` to reconnect it.
This setting is independent of voltage range and autorange. `STATUS?` reports
`impedance_requested` and applied `impedance` as `10M` or `HIGHZ`.

Changes reset measurement filters and discard 320 frames at 32 kSPS (40 at
4 kSPS), preserving about 10 ms of settling. This allowance needs verification with the source impedance
and input capacitance on the board; external settling can take longer.
Wait for `valid=1` and allow the physical source to settle before capturing.
Changes are rejected during captures and clear an existing calibration point
set. Hold the setting constant throughout a fit and its verification. R26
loads the external sense terminals even when the calibration relay selects CALBUS.

PA startup inhibits the resistor before enabling the PA; output disable restores
the requested setting after a configurable 10 ms output-decay allowance. This
initial delay must be verified with the future PA hardware. PA enable requests are rejected while MV_ON reads HIGH,
and 10M requests are rejected while the PA is requested or starting. Faults
cancel the requested connection and drive MV_ON LOW. PA control is currently a
software placeholder: physical PA-off feedback and output-decay timing must be
implemented when the PA hardware is added. Startup assumes the present
measurement-only hardware has no active PA.

## Voltage autorange

Startup selects 15 V with both current and voltage autorange enabled.
Send `AUTORANGE:V OFF` to disable automatic voltage range selection.
`STATUS?` reports current control as `autorange` and voltage control as `autorange_v`.

On the 6 V range, two consecutive calibrated CH1 readings with magnitude at
least 6.2 V select 15 V. ADC clipping selects 15 V immediately. On the 15 V
range, instantaneous and fast filtered readings must both remain at or below
5.0 V in magnitude for 100 ms before selecting 6 V. This applies to either
polarity. Each switch resets measurement filters and discards settling frames.
Manual `RANGE:V` selection disables voltage autorange. Disable both autoranges
for manual calibration.

## Manual calibration workflow

1. Record `CAL:SHOW?`, warm up the board and reference instruments, and establish
   the physical reference connection. For voltage/current calibration use the
   normal measurement path; for BUS, measure the manually set CALBUS with a DMM.
2. Send `AUTORANGE:I OFF` and `AUTORANGE:V OFF`, select the current and voltage ranges, and wait for
   `STATUS?` to show `valid=1`. Allow the physical reference to settle too.
3. Send `CAL:BEGIN V`, `I`, or `BUS`. Both ranges are locked for this point set:
   a console range/autorange command clears the set. Range changes are rejected
   while a capture is active.
4. Apply the first reference, measure its actual value, and send e.g.
   `CAL:CAPTURE 0.00001234` for a 12.34 microampere current reference. Decimal
   and scientific notation (e.g. `1.234E-5`) are accepted, with up to 12 digits
   and exponent magnitude up to 12. Wait for `OK POINT ...` before changing it.
5. Repeat at several distinct points. At least two points are required; up to
   eight are supported. Include zero and points spread across the usable range,
   and negative points when practical. Acquisition has a 2-second timeout and
   rejects clipping, excessive noise and drift. A failed capture does not add a point.
6. Run `CAL:POINTS?` and `CAL:FIT`. The fit is `reference = gain * nominal + offset`.
   Review capture quality, point span and residuals. The fit has not changed active
   coefficients or Flash yet.
7. Repeat for other ranges using `CAL:BEGIN ...`; previously fitted coefficients
   remain staged. Voltage coefficients are separate for 15 V and 6 V; current
   coefficients are separate for all five current ranges; BUS has one pair.
8. Review `CAL:SHOW?`, then explicitly send `CAL:SAVE`. Only fitted measurement
   entries are updated; source calibration and other record fields are preserved.
   A changed active calibration generation rejects the save; reset and refit.
   Wait for fresh `MEAS? valid=1` after the save before verification.
9. Verify at independent points not used in the fit, reboot, and verify again.
   `CAL:RESET` discards unsaved edits; reset/power loss also loses staged edits.

Example for the 6 V range (replace reference values with actual DMM readings):

```text
AUTORANGE:I OFF
AUTORANGE:V OFF
RANGE:V 6V
STATUS?
CAL:BEGIN V
CAL:CAPTURE 0.000000
CAL:CAPTURE 1.500123
CAL:CAPTURE 3.000456
CAL:POINTS?
CAL:FIT
CAL:SHOW?
CAL:SAVE
MEAS?
```

Apply and settle each physical voltage **before** its `CAL:CAPTURE` command,
and wait for each asynchronous point response. Do not paste the example as
one batch. The console does not generate or switch those voltages.

## Validation

```sh
sh firmware/tests/run_console.sh
sh firmware/tests/run_autorange.sh
sh firmware/tests/run_calibration_state.sh
sh firmware/tests/run_measurement_health.sh
sh firmware/tests/run_precision.sh
sh firmware/tests/run_calibration_quality.sh
```

Host tests validate command parsing, precision-window warm-up and simulated
mains rejection, calibration semantics and capture quality, acquisition pauses
and save failure recovery, autorange, and UART queue behavior. Board verification is still required for the
actual USB/UART connection and acquisition performance under console traffic.

Before PA integration, verify both polarities and all current/voltage ranges on
the board. Check range overlap with independent references, settling in 10M and
HIGHZ modes at low and high source impedances, measured sample rate, noise with
each integration setting, and recovery after saving and rebooting. The current
8–40 frame range allowances and 40-frame impedance/resume allowances remain
initial values until these measurements establish their margins.

## Shared instrument logging

Console output is initialized before instrument bring-up. ADC diagnostics appear
before the serial-ready banner; future DAC bring-up uses the same logger.
`ACQ? log_dropped` reports rejected log messages since logger initialization.
Logs and command responses share the existing nonblocking UART TX queue. A full
queue drops the entire log message; command overflow handling is unchanged.

Drivers include `smu_log.h` and call `smu_log_write("DAC ready\r\n")` or
`smu_log_printf("DAC register=0x%04X\r\n", value)`. Include an ADC/DAC source
prefix and CRLF yourself. Calls are foreground-only, with no dynamic allocation;
messages must fit in 192 bytes including the terminator. Do not log per sample.
Before output registration, logging safely fails and counts the dropped message.
The console command state initializes after calibration loading without resetting
the transport, preserving queued startup logs. No commands execute during bring-up.

Host validation: `sh tests/run_log.sh` and `sh tests/run_console.sh`.

## Independent watchdog

The STM32 IWDG starts after console transport initialization, before instrument
bring-up. The Platform port owns the HAL handle and initialization; no generated
`MX_IWDG_Init()` call is needed. The matching STM32CubeH5 V1.7.0 IWDG HAL source
and header are included and enabled by a CMake compile definition.

Prescaler 64 and reload 999 give `64 * (999 + 1) / 32000 = 2.000 s` nominal.
Using the datasheet LSI range of 29.4–33.6 kHz gives approximately 1.90–2.18 s.
There is no refresh window and no early-warning interrupt. Debug freeze is
configured before start: halting the core suspends the watchdog; a DEBUG build
still uses the watchdog when running normally or without a debugger.

Only the foreground loop services it, at most once per 100 ms, after instrument,
console, and measurement processing return. Interrupt handlers never refresh it.
A responsive latched fault remains diagnosable while the PA request and input
load are off. Invalid runtime states, unsafe fault state, or refresh failure
stop servicing. `Error_Handler()` disables the software PA request and waits;
after watchdog start, this and HardFault hangs eventually reset the MCU.
An error before watchdog start is not covered. PA control currently remains a
software stub; physical PA shutdown and reset-safe enable circuitry are required
when the output stage is implemented. Watchdog recovery never restores a PA request.

The reset flag is captured before RCC reset flags are cleared. Startup logs
`SMU IWDG started nominal_ms=2000 reset=0|1`; `STATUS?` adds
`WATCHDOG active=1 reset=0|1`. A watchdog reset boots the regular safe measurement
startup with saved calibration and default ranges/autorange/integration settings.

After DMA is quiesced, an authorized calibration save refreshes once immediately
before writing Flash. No refresh occurs inside Flash erase/program waits or DMA
waits. The current DMA wait is bounded to 10 ms; SPI bring-up transfers and DRDY
waits each have 100 ms limits. The Flash HAL has a 1 s timeout per operation and
returns on failure. These are software bounds, not hardware timing measurements;
verify the complete save duration remains comfortably below the minimum watchdog
timeout on the board. A genuinely stuck save resets rather than feeds indefinitely;
existing alternating calibration slots and readback validation are preserved.

### CubeMX setup

1. Open `smuk.ioc`; select **System Core → IWDG** and enable it (Activated).
2. Set **Prescaler = 64**, **Reload = 999**, **Window = 4095** (disabled), and
   **Early Wakeup Interrupt = disabled / 0**. Leave IWDG NVIC interrupt disabled.
3. Under RCC, use the internal **LSI** oscillator. Do not alter the existing
   HSI/PLL/system clock configuration; HAL IWDG start also enables LSI itself.
4. Under **Project Manager → Advanced Settings → Generated Function Calls**,
   select **Do Not Generate Function Call** for `MX_IWDG_Init`. Keep HAL selected.
   The Platform port initializes the watchdog at the required startup point;
   an automatic generated call would start it earlier and initialize it twice.
5. Keep **Keep User Code when re-generating** enabled. Keep the existing
   software-start option-byte configuration; do not enable hardware start.
6. Generate code and inspect the diff. Confirm the HAL IWDG source/header are
   retained and no automatic `MX_IWDG_Init()` call appears in `main()`. The application
   `smu_watchdog_start()` and foreground service calls remain in USER CODE blocks.
   Build with the existing CMake workflow.

The `.ioc` has not been changed automatically; apply these settings before your
next regeneration. The current CMake firmware already enables IWDG without that step.
CubeMX UI wording can vary by version. References:
[STM32CubeMX advanced settings](https://dev.st.com/stm32cube-docs/stm32cubemx/6.18.1/en/docs/markup/CubeMX_UserManual/chapters/04_4_stm32cubemx_user_interface.html),
[STM32H503 datasheet, LSI characteristics](https://www.st.com/resource/en/datasheet/stm32h503eb.pdf).

### Validation

`sh tests/run_watchdog.sh` checks service timing/tick wrap, handled fault states,
initialization/refresh failures, and Flash-save policy. Console and measurement
health suites check integration with their existing paths.

On the board, verify normal operation and calibration saves do not reset; halt
in the debugger for more than 2 s and confirm debug freeze; then run freely with
a deliberate foreground infinite loop or debugger-injected HardFault and verify
reset after approximately 2 s and `WATCHDOG reset=1` afterward. Use a temporary
bench-only fault injection, never a normal serial command. Verify PA enable and
range/impedance reset behavior electrically before adding a working output stage.
