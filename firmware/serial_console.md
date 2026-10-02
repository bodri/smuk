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
with console traffic. Flash saves are synchronous and may interrupt acquisition;
wait for the save response and fresh valid measurements afterward.

## Commands

| Command | Result |
| --- | --- |
| `PING` | `PONG` |
| `HELP` | Command summary |
| `ECHO ON` / `ECHO OFF` | Device input echo; default off |
| `STATUS?` | State, faults, current/voltage ranges, autorange, validity, frame count, capture status |
| `MEAS?` | Precision filtered measurements in amperes/volts, ranges and validity |
| `RAW?` | Latest raw CH0/CH1/CH2 codes; these are not averaged |
| `RANGE:I 1.5A` / `100MA` / `10MA` / `1MA` / `100UA` | Request fixed current range and disable autorange |
| `RANGE:V 15V` / `6V` | Request fixed voltage range and disable voltage autorange |
| `AUTORANGE ON` / `OFF` or `AUTORANGE:I ON` / `OFF` | Control current autorange |
| `AUTORANGE:V ON` / `OFF` | Control voltage autorange independently |
| `CAL:SHOW?` | Staged measurement coefficients, dirty flag and active Flash sequence |
| `CAL:BEGIN V` / `I` / `BUS` | Start a point set for the selected fixed range/channel |
| `CAL:CAPTURE <reference>` | Average 256 fresh accepted raw ADC samples; reference in volts for V/BUS or amperes for I |
| `CAL:POINTS?` | Show nominal readings and entered references |
| `CAL:FIT` | Fit gain/offset and stage them; show per-point residuals |
| `CAL:SAVE` | Persist staged fits and apply them; requires a dirty record and stable fixed ranges |
| `CAL:ABORT` | Stop capture and clear its point set; retain previously staged fits |
| `CAL:RESET` | Stop capture, discard all staged fits and reload active calibration |

Manual captures **do not select CALBUS or operate calibration relays**. Apply
references using the normal measurement input path, or an externally arranged
CALBUS connection. The entered reference must be independently measured, not
the nominal CALBUS label or the SMU's already calibrated display.

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
   rejects readings near ADC clipping. A failed capture does not add a point.
6. Run `CAL:POINTS?` and `CAL:FIT`. The fit is `reference = gain * nominal + offset`.
   Review residuals. The fit has not changed active coefficients or Flash yet.
7. Repeat for other ranges using `CAL:BEGIN ...`; previously fitted coefficients
   remain staged. Voltage coefficients are separate for 15 V and 6 V; current
   coefficients are separate for all five current ranges; BUS has one pair.
8. Review `CAL:SHOW?`, then explicitly send `CAL:SAVE`. Only fitted measurement
   entries are updated; source calibration and other record fields are preserved.
   A changed active calibration generation rejects the save; reset and refit.
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
```

Host tests validate command parsing, staged fitting, save failures, capture
errors, and UART queue behavior. Board verification is still required for the
actual USB/UART connection and acquisition performance under console traffic.
