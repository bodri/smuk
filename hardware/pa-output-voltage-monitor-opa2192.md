# PA output voltage monitor — OPA2192 and ADS131M03 CH2

Date: 2026-10-16.

Status: selected final-board topology; protection component values are preliminary and require verification. This document supersedes the earlier ±30 V, inverting monitor concept in [the original proposal](pa-output-voltage-monitor-proposal.md). No firmware or KiCad changes are included.

## Purpose and system connections

Measure the PA output before the output relay, independently of the calibrated DUT voltage measurement. PA rails are nominally +17.5 V and −17.5 V. The relay connects DUT FORCE+ either to PA output (ON) or ground (OFF), using break-before-make contacts. DUT return passes through the low-side current shunt. SENSE+ and SENSE− remain connected to the DUT terminals.

CH2 can therefore observe PA zero while the relay selects ground. OFF grounds FORCE+; it does not isolate the DUT. An externally powered DUT can still drive current through the grounded output and return path.

Use a quiet analog-ground reference on the ground side of the low-side shunt. Monitor-divider current must not pass through the DUT current shunt. PA analog feedback must remain closed when the relay disconnects the PA from the DUT.

## Final-board measurement path

```text
PA_OUT, before relay
        |
      190 kΩ
        |
        +---------------------- U1A non-inverting input (+)
        |
       10 kΩ
        |
       AGND

U1A = one OPA2192 channel, connected as a voltage follower:
    output directly connected to its inverting input (−)
    positive supply = +17.5 V
    negative supply = −17.5 V

U1A output --> protection and filter below --> ADS131M03 AIN2P
AGND --------------------------------------> ADS131M03 AIN2N
```

Use 0.1% low-temperature-coefficient divider resistors, preferably with specified ratio tracking. The 190 kΩ may be split into series resistors for voltage rating, layout, and fault tolerance; maintain the total resistance.

Nominal unloaded scaling at PGA gain 1:

`V_buffer = V_PA / 20`

| PA output | Buffer output before ADC loading |
|---:|---:|
| −17.5 V | −0.875 V |
| −15 V | −0.750 V |
| 0 V | 0 V |
| +15 V | +0.750 V |
| +17.5 V | +0.875 V |

The divider draws 87.5 µA at 17.5 V. Buffering allows this high-resistance divider without direct loading by the ADC.

The OPA2192 supports up to 36 V total operating supply. Nominal ±17.5 V totals 35 V: verify regulator tolerance, ripple and startup overshoot remain within the operating limits. Add 100 nF decoupling from each supply pin to AGND close to the IC, plus local bulk capacitance as appropriate. Configure the unused channel as a ground-referenced follower rather than leaving its inputs floating.

The specified maximum initial amplifier offset of 25 µV at 25°C corresponds to approximately 0.5 mV referred to PA output through the divider, before calibration.

## ADC protection and filter

This follows the topology in TI application note SBAA506. It requires adaptation and validation for the OPA2192's ±17.5 V rails; the application note's original fault voltages differ.

```text
U1A OUT --- R1 ---+--- R2 ---+--- AIN2P
                 |          |
                 |          C1
                 |          |
                 |         AGND
                 |
                 +--- D1 ---> ADC AVDD
                 |
AGND --- D2 ---> D3 --->-----+

AIN2N ---------------------- AGND
```

Diode connections, stated explicitly:

- D1: BAT54, anode at the R1/R2 junction, cathode at ADC AVDD.
- D2 and D3: the two series junctions of a BAV199. D2 anode connects to AGND; D2 cathode connects to D3 anode; D3 cathode connects to the R1/R2 junction. Current flows from AGND toward a negative fault.
- Do not substitute a single diode from AGND to the input: it would clip valid negative measurements.

| Component | Preliminary value | Role |
|---|---:|---|
| R1 | 10 kΩ, 0.125 W | Limits buffer-fault current |
| R2 | 100 Ω | Additional ADC input isolation/filtering |
| C1 | 10 nF, C0G | Input filtering |
| D1 | BAT54 | Positive clamp to ADC supply |
| D2 + D3 | BAV199 series pair | Negative clamp while allowing bipolar input |

With a negative buffer fault near −17.5 V, R1 limits current to approximately 1.6 mA if the clamp node is near −1.6 V. This is an estimate, not a guaranteed clamp voltage. Verify forward-voltage extremes, especially at cold temperatures, diode tolerances, and ADC input current against the ADS131M03 absolute maximum ratings. Absolute maximum limits are damage limits, not normal operating targets.

At 17.5 V across R1, its conservative dissipation estimate is about 31 mW; check actual worst-case rails and fault duration before selecting the resistor package.

### Supply clamp and power sequencing are not yet finalized

The positive diode injects fault current into ADC AVDD. The supply needs a verified sink/clamp path; a regulator that only sources current is insufficient by itself. A positive buffer fault can inject roughly 1.4 mA with AVDD near 3.3 V. Review the ADC-off/buffer-on condition separately, including back-powering.

Select and verify the AVDD clamp against the permitted rail voltage and injected current. A generic “3.3 V TVS” is not automatically suitable: its actual clamp voltage may be too high. Do not treat this circuit as production-ready until that path and negative clamp extremes have been validated.

### Measurement effects

The 10.1 kΩ total series resistance interacts with the ADC's finite, clock-dependent input impedance. The ADC voltage will not be exactly `V_PA / 20`. Include this loading, clamp leakage, temperature dependence and filter settling in calibration and testing. Do not use an ideal ×20 conversion as a precision guarantee.

The input filter also adds latency. This channel is intended for slow zero tracking and supervision, not sole fast PA protection.

## Temporary passive version

For current bench work, omit U1 and use the previously selected lower-resistance divider:

```text
PA_OUT --- 19 kΩ ---+--- ADC input filter/protection --- AIN2P
                   |
                  1 kΩ
                   |
                  AGND ------------------------------- AIN2N
```

Nominal divider output is ±0.875 V at ±17.5 V, before ADC loading. It draws 0.875 mA at the rail extremes. Its Thevenin resistance is 950 Ω. Calibrate the complete installed path, including any added series resistance and filtering. This version has no buffer rail-fault mechanism, but wiring errors, divider faults and power sequencing still need consideration.

## Calibration and firmware requirements

- Disconnect the former CALBUS source before connecting this circuit to CH2.
- Keep CH2 at PGA gain 1 and implement a dedicated PA-voltage conversion with separate gain/offset calibration.
- Calibrate CH2 against an external meter at zero and both voltage polarities before using it to learn PA zero. Otherwise CH2 offset becomes real source offset.
- Preserve the existing DUT measurement calibration and calibration-storage layout unless separately redesigned.
- Continuous OFF-state zero tracking must use CH2, not the DUT-side voltage reading. Require fresh, settled, valid data and bound correction.
- Freeze zero tracking before switching the relay; check PA voltage near zero, switch, wait for contact and measurement settling, then ramp.
- Existing firmware zero learning is based on DUT voltage and does not automatically implement this new CH2/relay workflow.

## Validation checklist

- [ ] Check OPA2192 supply extremes, including startup overshoot.
- [ ] Verify diode package pinout and clamp orientation.
- [ ] Calculate positive/negative clamp behavior over component tolerance and temperature.
- [ ] Finalize AVDD fault-current sinking/clamping and powered/unpowered behavior.
- [ ] Test the protection network with a current-limited source before attaching the ADC.
- [ ] Measure CH2 loading, noise, gain, offset, settling and saturation recovery.
- [ ] Verify monitor return current bypasses the DUT shunt.
- [ ] Verify break-before-make relay operation and PA feedback continuity.
- [ ] Calibrate CH2 independently, then validate zero tracking with an external meter.
- [ ] Run KiCad ERC and PCB DRC before finalizing the schematic/layout revision.

## References

- [TI OPA2192 datasheet](https://www.ti.com/lit/ds/symlink/opa2192.pdf)
- [TI ADS131M03 datasheet](https://www.ti.com/lit/ds/symlink/ads131m03.pdf)
- [TI SBAA506: Circuit for protecting ADS131M0x ADC from electrical overstress](https://www.ti.com/lit/an/sbaa506/sbaa506.pdf?ts=1781300498469)
