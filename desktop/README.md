# SMUK desktop measurement console

A Python/PySide6 client for the SMUK firmware serial console. The display follows
an instrument-panel layout: black background, amber channel header, large green
voltage/current readings, blue-grey controls, and observed min/average/max statistics.

## Run on macOS

Use macOS 13 or later and Python 3.11 or later with the current Qt wheels.
From the repository root:

```sh
cd desktop
python3 -m venv .venv
.venv/bin/python -m pip install -e '.[dev]'
.venv/bin/smuk-desktop
```

Alternatively, run `.venv/bin/python -m smuk_desktop`. The project-local environment
created during development is ignored by Git. To reproduce the tested dependency
versions, install `requirements-dev.lock` before installing this project.

1. Close other serial terminals/debugger serial views that own the VCP.
2. Select the ST-LINK VCP port (usually `/dev/cu.usbmodem…`) and click **Connect**.
3. The app checks `PING`, turns console echo off, and reads status/calibration metadata.
4. Live voltage/current readings and statistics appear when precision data is valid.
5. Select **Auto** or a fixed range independently for voltage/current. Controls
   send firmware commands and update from confirmed replies.
6. Select input impedance and integration; wait for the precision window to fill.
7. Expand **Serial console** for queries and supported controls. **Save log…**
   exports the last 2,000 displayed lines, including timestamps and TX/RX directions.

The app never restores instrument settings automatically on reconnect. Disconnect
only closes the serial port; it does not change the device's operating settings.
If firmware resets or a command times out, reconnect explicitly.

## Current features and scope

- Asynchronous QSerialPort at 115200 / 8N1, with port enumeration and unplug handling.
- One command outstanding; complete multiline response framing, including QUALITY
  and WATCHDOG. Unsolicited bring-up/fault logs stay visible in the console.
- Precision measurement polling at approximately 5 Hz; status at 1 Hz and
  acquisition diagnostics at 0.5 Hz, without an accumulating polling backlog.
- Current ranges: 1.5 A, 100 mA, 10 mA, 1 mA, 100 µA. Voltage ranges: 15 V and 6 V.
- Independent autorange, 10 MΩ/HIGHZ input, and 0.5/1/2/5/8/10/20/50/100 ms integration controls.
  New firmware defaults to 32 kSPS acquisition and 2 ms integration. The display
  shows raw-sample coverage (640 samples for 20 ms), and acquisition diagnostics
  show the delivered rate. A known rate mismatch inhibits live precision readings.
- Invalid, settling, stale, clipped, faulted, and disconnected data is dimmed or labelled.
- Min/average/max and reset for distinct valid observed measurements. These statistics
  cover polled replies, not every ADC sample. Repeated sample counters are excluded;
  reset/counter wrap, reconnect, and observed calibration generation changes start a
  new statistics segment.
- Derived power is the product of averaged voltage and current, not independently
  averaged instantaneous power. Energy integration is deferred.
- Guided manual calibration for voltage, current, and CALBUS: fixed-range preparation,
  rate-aware asynchronous captures, point quality, staged fits/residuals, explicit Flash
  save confirmation, discard, and text report export. FV source controls are available when updated firmware reports source capabilities.

Console commands currently supported: `PING`, `HELP`, `STATUS?`, `MEAS?`, `RAW?`,
`ACQ?`, `SOURCE?`, `SOURCE:V <volts>`, `OUTPUT ON|OFF`, `CAL:SHOW?`, `CAL:POINTS?`, `ECHO ON|OFF`, and the range/autorange/impedance/
integration commands. Unknown commands and multiline command injection are rejected.
See [firmware console documentation](../firmware/serial_console.md) for the wire format.
The client targets the current firmware, including the `WATCHDOG` status line.

## Structure

```text
src/smuk_desktop/
  application.py       QApplication and launch options
  protocol/            Qt-independent commands, responses, line/transaction parsing
  instrument/          Serial adapter, command/poll controller, state, statistics
  ui/                  Instrument display, controls, statistics, console, main window
  resources/           Theme and application icon
tests/                 Recorded wire fixtures, controller/UI and virtual-port tests
packaging/macos/       Local .app packaging configuration
```

Protocol modules do not import Qt; widgets never access serial directly.

## Manual calibration

1. Connect and click **Manual calibration…**. Select **V**, **I**, or **BUS**, both
   fixed ranges, and the input impedance you will use. Click **Prepare fixed ranges**;
   this turns both autoranges off. Integration stays at its current setting.
2. Wait for settled measurements, then click **Begin / restart captures**. For
   voltage, apply a known voltage across the sense inputs. For current, apply a
   known current and measure its actual value with your reference instrument.
   For BUS, set the physical CALBUS reference manually. The app does not switch it.
3. Let each physical reference settle. Enter the actual value in **V** or **A**,
   including its sign, and click **Capture … samples**. The count is read from firmware:
   2,048 samples at 32 kSPS, or 256 at 4 kSPS, both nominally 64 ms. For example, 100 µA is
   `0.0001` A or `1e-4` A. Capture at least two distinct, well-spaced points;
   preferably include zero and both polarities within the selected range. Up to
   eight points are supported. Unstable/clipped captures are rejected by firmware;
   correct the reference and retry. The table shows nominal values and capture RMS/drift.
4. Click **Fit staged coefficients**. Review gain, offset, spans, and residuals in
   the detail panel. A fit updates the staged record only; live measurements still
   use the active calibration until saved. A rejected fit is not a successful calibration.
5. To calibrate another range or target before saving, click **Change target / ranges**,
   select the next configuration, prepare, and repeat. Previous staged fits are retained.
6. Click **Save staged fits to Flash…** and confirm only after reviewing the results.
   This saves **all** staged coefficients, activates them, and resets desktop statistics.
   Save once after fitting the intended ranges to reduce Flash writes. Verify the result
   against independent reference values, including values not used in the fit.
7. **Export calibration report…** saves up to the last 4,000 workflow lines, including
   configuration commands, references, points, quality, fits, residuals and save results.
   **Discard all staged fits…** restores the staged record from the active calibration
   and clears captures; it does not erase saved calibration.

Closing aborts the capture session and clears its points but retains staged fits.
Both autoranges remain off and ranges/impedance remain as selected: restore your
preferred settings explicitly after closing. Calibration controls stay in this dialog;
the serial console does not permit capture, fit, reset or save commands. Other desktop
controls are locked during the workflow. A disconnect/reset does not resume it;
reconnect and begin again, checking staged coefficients before proceeding.

## Tests and preview

```sh
.venv/bin/python -m pytest
QT_QPA_PLATFORM=offscreen .venv/bin/python -m smuk_desktop --screenshot /tmp/smuk.png
```

Tests use recorded responses and fake transports plus a virtual terminal for the
actual Qt serial adapter. They never open the physical SMU. The screenshot command
shows the disconnected UI and exits without opening a port. Real board acceptance
still requires connection, controls, range settling, unplug/replug, and watchdog
reset checks. See [bench checklist](../doc/bench-validation-checklist.md).

If macOS marks files in the environment hidden, Qt can fail to discover its
platform plugins and Python 3.14 can skip the editable-install `.pth` file.
Both occurred in this workspace during validation. If startup reports a missing
Qt platform plugin or `smuk_desktop` module, clear the visibility flag inside
this environment and retry:

```sh
chflags -R nohidden .venv
```

## Build a local macOS app

On the Mac architecture you want to target:

```sh
.venv/bin/python -m pip install -e '.[packaging]'
sh packaging/macos/build.sh
open dist/SMUK.app
```

The bundle minimum macOS version is the higher of Qt’s baseline and the bundled
Python deployment target. The current Homebrew Python was built for macOS 27, so
this local bundle targets macOS 27. To target older Macs, build with a compatible
Python distribution.

The bundle is for local use. The build tool attempts ad-hoc signing; workspace
Finder metadata can prevent it, so distribution signing is not validated here.
The bundled executable has been smoke-tested without opening hardware.
Signing/notarization for distribution and Intel/Apple
Silicon builds are separate deployment work. Build output and environments remain
ignored. Qt's deployment guidance: [Qt for Python packaging](https://doc.qt.io/qtforpython-6/deployment/index.html).

## FV source controls

Click the **SET … V** button beneath the voltage measurement to open the voltage
setpoint dialog. Bounds and slew rate come from `SOURCE?`; signed voltages become
available only when supported by the configured hardware. Apply sends `SOURCE:V`
and confirms the target with a fresh source query. The large readout remains the
measured DUT voltage. Target and nominal applied command are shown during ramping.

The **OUTPUT ON** button is enabled only with healthy settled readings, confirmed
ADC rate, both autoranges OFF, a fixed current range, voltage range 6 V or 15 V, HIGHZ input, and no calibration
session. Unmet prerequisites are displayed beneath the controls. The app does not
silently alter instrument settings. Once starting/active, the button requests
**OUTPUT OFF**; a tripped source can be reset explicitly with the same OFF request.
Range and calibration controls are locked during source activity or pending source
requests. An OFF request cancels queued source requests and takes user-queue priority;
it still waits for any outstanding serial transaction to finish.

Source state is confirmed through polling at approximately 1 Hz. An expired source
reply or disconnect is shown as UNKNOWN and never permits ON. Reconnect queries the
existing source state without restoring settings or enabling output. Older firmware
without capability metadata remains usable for measurements with FV controls disabled.

On the present PA board, ENABLE is tied to VCC and no relay exists: **OFF requests
zero voltage, not PA shutdown or DUT isolation**. The display shows both limitations.
Disconnecting/closing the app does not stop the physical output; request OFF before
disconnecting when a zero command is wanted. Communication failure cannot guarantee
zero output on this hardware.

Firmware now assumes the installed INA105 bipolar converter followed by PA gain 5:
overall gain 10, neutral DAC voltage 1.5 V, and DUT limits −15 to +15 V and nominal slew 10 V/s.
Reconnect after flashing; controls adopt the signed bounds reported by firmware.
The DAC must remain buffered externally to prevent loading. Hardware startup
protection remains necessary because the PA is always enabled; verify the midpoint
with PA power removed before connecting/powering it. See [FV bring-up](../doc/fv-stm32-dac-bringup.md).

### Measured source zero and bench voltage correction

`SOURCE:ZERO` explicitly learns zero on the selected range with OUTPUT OFF,
a fixed current range, both autoranges OFF, HIGHZ, and healthy acquisition.
Disconnect all external sources and leave the output unloaded first. The desktop
SET dialog provides **Learn source zero…** with confirmation. SOURCE state 5
means learning; OUTPUT OFF cancels; success returns state 1 and `zero_valid=1`.
Each range keeps its own learned neutral in RAM until reboot. The previous
hardcoded +52 mV trim is removed, so zero needs learning again after reboot.

`SOURCE:REGULATION ON|OFF` selects optional bounded voltage correction while
OUTPUT is OFF. The SET dialog exposes this as a bench control. It defaults OFF;
the hardware compliance pin is not yet connected. The source query reports
`regulation`, `limited`, `zero_valid`, and `correction_V`; correction is limited
to ±0.2 V and remains subject to DAC resolution and source endpoints. OFF clears
runtime correction and commands the accepted learned neutral without isolation.
See [FV bring-up](../doc/fv-stm32-dac-bringup.md) for the complete bench procedure.
