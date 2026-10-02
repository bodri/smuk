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
| `INTEGRATION 1MS` / `8MS` / `20MS` / `50MS` / `100MS` | Select precision integration window; default 8 ms |
| `CAL:SHOW?` | Coefficients, dirty flag, Flash sequence, FLASH/DEFAULTS provenance and capture limits |
| `CAL:BEGIN V` / `I` / `BUS` | Start a point set for the selected fixed range/channel |
| `CAL:CAPTURE <reference>` | Average 256 fresh accepted raw ADC samples; reference in volts for V/BUS or amperes for I |
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

Select `INTEGRATION 1MS`, `8MS`, `20MS`, `50MS`, or `100MS`. At the nominal
4 kSPS board rate these use 4, 32, 80, 200, and 400 samples respectively.
The default remains 8 ms. Fast filtering and ADC registers are unchanged.
Running sums use compensated arithmetic to limit accumulation drift.

At exactly 4 kSPS, 20 ms covers one 50 Hz cycle, 50 ms covers three 60 Hz
cycles, and 100 ms covers five 50 Hz or six 60 Hz cycles. Verify the actual
sample-clock rate and interference frequency on the board; rejection depends
on their agreement with the selected window. These are sample-based windows,
not a line-synchronized ADC mode. Integration changes are rejected during a
capture and clear its point set while preserving previously staged fits.
After selecting a window, wait for `MEAS? valid=1` before trusting precision
readings. This setting is volatile and returns to 8 ms after reset.

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

Changes reset measurement filters and discard 40 frames (about 10 ms at
4 kSPS). This initial allowance needs verification with the source impedance
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
