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
- Independent autorange, 10 MΩ/HIGHZ input, and 1/8/20/50/100 ms integration controls.
- Invalid, settling, stale, clipped, faulted, and disconnected data is dimmed or labelled.
- Min/average/max and reset for distinct valid observed measurements. These statistics
  cover polled replies, not every ADC sample. Repeated sample counters are excluded;
  reset/counter wrap, reconnect, and observed calibration generation changes start a
  new statistics segment.
- Derived power is the product of averaged voltage and current, not independently
  averaged instantaneous power. Energy integration is deferred.
- Source/PA controls and the manual calibration wizard are deferred. No Flash writes,
  calibration capture, source setpoints, or output-enable commands are sent.

Console commands currently supported: `PING`, `HELP`, `STATUS?`, `MEAS?`, `RAW?`,
`ACQ?`, `CAL:SHOW?`, `CAL:POINTS?`, `ECHO ON|OFF`, and the range/autorange/impedance/
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

A calibration dialog will be created with its workflow, rather than as an empty
placeholder. Protocol modules do not import Qt; widgets never access serial directly.

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
