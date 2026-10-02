# SMU measurement bench validation

Use this checklist to validate the working measurement hardware and firmware
before implementing the PA and source-control logic. Record results against a
calibrated reference instrument. Checkboxes represent completed checks, not
automatic certification of accuracy.

Console reference: [Serial console and manual calibration](../firmware/serial_console.md).

## Test record and preparation

| Item | Record |
| --- | --- |
| Date / operator | |
| Board revision / serial number | |
| Firmware revision | |
| DMM model / calibration date | |
| Voltage and current source | |
| Ambient temperature / warm-up duration | |
| Required voltage accuracy | |
| Required current accuracy per range | |
| Allowed noise / settling time | |

- [ ] Keep the PA disabled throughout these measurement tests.
- [ ] Confirm input ratings, grounding, and reference-source polarity before connecting.
- [ ] Warm up the board and reference instruments until readings stabilize.
- [ ] Set conservative source current and voltage limits.
- [ ] Open the serial console at **115200 baud, 8N1** and enable session logging.
- [ ] Record existing coefficients using `CAL:SHOW?` before changing calibration.
- [ ] Define accuracy and noise acceptance limits before collecting results.

## 1. Startup and acquisition health

```text
STATUS?
ACQ?
MEAS?
CAL:SHOW?
```

- [ ] `STATUS?` reports `state=4`, `faults=0`, and both autoranges enabled at startup.
- [ ] Measurements become `fresh=1`, `settled=1`, and `valid=1`.
- [ ] Precision becomes ready and `MEAS? window` fills completely (default `32/32`).
- [ ] `ACQ?` reports `stale=0`; frame count advances.
- [ ] Observe for a recorded duration; CRC/SPI errors, overruns, and gaps do not increase.
- [ ] Record the DMA-busy count separately. Busy events are skipped DRDY triggers;
      they do not alone invalidate accepted frames or latch a fault.
- [ ] No unexpected resets or dropped logs occur.

| Observation | Start | End | Duration / notes |
| --- | --- | --- | --- |
| Frames | | | |
| CRC errors | | | |
| SPI errors | | | |
| Overruns | | | |
| Gaps | | | |
| Busy events | | | |
| Dropped logs | | | |

## 2. Fixed-range voltage accuracy

Disable both autoranges and select the range. Wait for `MEAS? valid=1` after
each change and allow the physical reference to settle.

```text
AUTORANGE:I OFF
AUTORANGE:V OFF
RANGE:V 6V
```

- [ ] Test approximately **0, ±1, ±3, and ±5 V** on the 6 V range.
- [ ] Select `RANGE:V 15V` and test approximately **0, ±3, ±6, ±9, and ±12 V**.
- [ ] Stay within confirmed hardware ratings; the test points are suggestions.
- [ ] Compare both ranges at identical inputs in their overlap, such as ±3 V.
- [ ] Check zero offset, gain error, polarity symmetry, and repeatability.
- [ ] Repeat independent verification points after calibration; do not rely only
      on the points used to fit coefficients.

Error is **SMU reading − reference reading**. Record absolute error near zero;
percentage error is unsuitable when the reference is zero.

| Range | DMM reference (V) | SMU reading (V) | Error (V) | Pass / notes |
| --- | --- | --- | --- | --- |
| 6 V | | | | |
| 6 V | | | | |
| 6 V | | | | |
| 15 V | | | | |
| 15 V | | | | |
| 15 V | | | | |

Add rows for every applied point and repetition.

## 3. Fixed-range current accuracy

Use a known current source, or a voltage source and suitable series resistance.
Measure the actual reference current; do not assume its commanded value is exact.
Account for DMM burden voltage and the instrument shunt voltage.

- [ ] Keep both autoranges disabled.
- [ ] Select each current range using `RANGE:I`.
- [ ] Wait for a full valid precision window after switching.
- [ ] Test zero and approximately 10%, 50%, and 80% of each range.
- [ ] Test both polarities where the source and hardware permit.
- [ ] Compare adjacent ranges at a shared current that fits both ranges.
- [ ] Check offset, gain, symmetry, repeatability, and compliance of the reference source.

| Command | Range full scale | Suggested positive test points |
| --- | --- | --- |
| `RANGE:I 1.5A` | 1.5 A | 0.15, 0.75, 1.2 A |
| `RANGE:I 100MA` | 100 mA | 10, 50, 80 mA |
| `RANGE:I 10MA` | 10 mA | 1, 5, 8 mA |
| `RANGE:I 1MA` | 1 mA | 0.1, 0.5, 0.8 mA |
| `RANGE:I 100UA` | 100 µA | 10, 50, 80 µA |

Only use higher-current points within the board's confirmed continuous ratings
and thermal limits. Start low and monitor heating.

| Range | Reference (A) | SMU reading (A) | Error (A) | Pass / notes |
| --- | --- | --- | --- | --- |
| 1.5 A | | | | |
| 100 mA | | | | |
| 10 mA | | | | |
| 1 mA | | | | |
| 100 µA | | | | |

## 4. Autorange and settling

```text
AUTORANGE:I ON
AUTORANGE:V ON
```

Manual `RANGE:I` or `RANGE:V` commands disable the corresponding autorange;
re-enable it before testing automatic behavior.

- [ ] At approximately 3 V, the voltage range settles to 6 V.
- [ ] Slowly increase voltage magnitude: up-range occurs near **6.2 V**.
- [ ] Slowly decrease from the 15 V range: down-range occurs below **5 V**,
      after approximately **100 ms** of qualifying readings.
- [ ] Repeat the voltage sweep with negative polarity.
- [ ] Hold between 5 V and 6.2 V: the selected voltage range remains stable.
- [ ] Sweep current across adjacent ranges, checking correct selection and no chatter.
- [ ] Range changes temporarily invalidate readings, then recover valid measurements.
- [ ] After switching, readings meet the defined accuracy limit.

Use a logic analyser on range-control GPIOs, DRDY, and SPI CS to inspect switching
and settling. Console polling can miss brief transitions. Current thresholds use
calibrated current, so validate coefficients before interpreting their behavior.

| Transition | Applied input | Observed delay | Settling / accuracy / notes |
| --- | --- | --- | --- |
| Voltage 6 V → 15 V | | | |
| Voltage 15 V → 6 V | | | |
| Current up-range | | | |
| Current down-range | | | |

## 5. Input impedance

With a steady voltage input, compare:

```text
IMPEDANCE HIGHZ
IMPEDANCE 10M
```

- [ ] Verify mode selection in `STATUS?` and wait for valid measurements.
- [ ] With a low-impedance source, both modes give consistent readings.
- [ ] Add a known series resistor and check the expected 10 MΩ loading.
- [ ] Check settling and return to valid readings after each impedance change.

For an ideal source, series resistance `Rs`, and a 10 MΩ input load:

```text
Measured voltage = source voltage × 10 MΩ / (Rs + 10 MΩ)
```

If a DMM is connected across the input, its resistance is in parallel with the
instrument input. Include that loading and other leakage in the calculation.
HIGHZ disconnects R26; it does not imply infinite input resistance.

| Source voltage | Series resistance | Mode | Expected voltage | Measured voltage |
| --- | --- | --- | --- | --- |
| | | HIGHZ | | |
| | | 10M | | |

## 6. Noise and integration

Use a stable, low-noise source, fixed ranges, and a consistent wiring arrangement.
Wait for the complete window before recording each set.

```text
INTEGRATION 8MS
INTEGRATION 20MS
INTEGRATION 50MS
INTEGRATION 100MS
```

- [ ] Integration changes reset the precision window and validity recovers when full.
- [ ] Collect repeated readings for each setting over a recorded duration.
- [ ] Compare mean, standard deviation, and peak-to-peak variation.
- [ ] Check that changing integration does not introduce a systematic offset.
- [ ] Verify accepted sample rate and spacing before testing mains rejection.
- [ ] Once timing is verified, compare rejection of controlled 50/60 Hz interference.
      Use isolated, low-voltage test signals within input ratings.

| Integration | Samples collected | Mean | Standard deviation | Peak-to-peak |
| --- | --- | --- | --- | --- |
| 8 ms | | | | |
| 20 ms | | | | |
| 50 ms | | | | |
| 100 ms | | | | |

### Accepted sample rate

Record `STATUS?` and `ACQ?` twice, approximately 10 seconds apart, with a steady
input and no calibration saves or range changes. Use the actual elapsed time.

```text
Accepted samples/s = (frames₂ − frames₁) / elapsed seconds
Busy events/s     = (busy₂ − busy₁) / elapsed seconds
Window duration   = window samples / accepted samples/s
```

This is a first estimate; serial-command timing introduces uncertainty. A logic
analyser on DRDY, CS, and SCLK reveals conversion rate, transfer duration, and
whether skipped triggers create uneven spacing. Integration labels assume
**4,000 accepted samples/s**. Average rate alone does not establish mains rejection.

## 7. Manual calibration, persistence, and save recovery

The CALBUS reference must be set physically; firmware cannot switch it on this
hardware. Use the actual measured reference value for every capture.

- [ ] Record `CAL:SHOW?` before starting.
- [ ] Disable both autoranges, select fixed ranges, and wait for valid measurements.
- [ ] Start `CAL:BEGIN V`, `CAL:BEGIN I`, or `CAL:BEGIN BUS` as appropriate.
- [ ] Apply and settle each physical reference before `CAL:CAPTURE <value>`.
- [ ] Use zero and well-separated points, including both polarities where practical.
- [ ] Review capture RMS noise and drift; unstable captures must not become fit points.
- [ ] Run `CAL:POINTS?` and `CAL:FIT`; inspect span and residuals.
- [ ] Review staged coefficients with `CAL:SHOW?`, then run `CAL:SAVE`.
- [ ] After saving, wait for fresh, settled measurements and a full precision window.
- [ ] Confirm `ACQ? pauses` increases without new transport gaps or an ADC fault.
- [ ] Power-cycle; confirm the saved coefficients/sequence and `source=FLASH`.
- [ ] Verify independent reference points after restart.
- [ ] Repeat save/recovery checks and confirm no unexpected watchdog reset.
- [ ] Measure complete save duration and establish margin below the watchdog timeout.

Capture reference units are **volts** for V/BUS and **amperes** for I.
`CAL:RESET` discards unsaved fits. `source=FLASH` means a usable record was loaded;
it does not prove that every range in the record has been calibrated.

## 8. Watchdog recovery

- [ ] Normal measurement operation and calibration saves do not cause unexpected resets.
- [ ] `STATUS?` reports `WATCHDOG active=1`.
- [ ] Halt the core in the debugger for more than 2 seconds: debug freeze prevents reset.
- [ ] With the debugger running freely, inject a temporary bench-only foreground
      infinite loop; verify a reset after approximately 2 seconds from the last refresh.
- [ ] Repeat with a debugger-injected HardFault if available.
- [ ] After watchdog recovery, verify startup logging and `WATCHDOG reset=1`.
- [ ] Confirm saved calibration survives and measurement operation resumes normally.
- [ ] Confirm no PA-enable request is restored.
- [ ] Remove temporary fault injection before normal use.

The watchdog period is nominally 2 seconds and varies with LSI frequency.
Responsive, safely latched ADC faults remain diagnosable without repeated resets.
PA shutdown currently changes software state; verify physical output-disable and
reset behavior electrically when the PA hardware and control are implemented.

## Completion record

- [ ] Accuracy meets the recorded limits across the tested ranges and polarities.
- [ ] Noise and repeatability meet the recorded limits.
- [ ] Autorange and impedance changes settle reliably without chatter.
- [ ] Calibration saves, power cycles, and watchdog recovery preserve valid operation.
- [ ] Session logs, reference readings, and analyser captures are saved with this record.
- [ ] Untested points, failures, and follow-up work are listed below.

| Open item | Evidence / suspected cause | Next action |
| --- | --- | --- |
| | | |
