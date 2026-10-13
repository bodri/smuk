# PA output voltage monitor using ADC CH2

Status: preliminary design proposal. This is a measurement concept, not a finished protected schematic.

## Purpose

Repurpose the ADS131M03 CH2 input currently used for CALBUS to measure the power amplifier output before the output relay. Keep the existing voltage channel measuring SENSE+ relative to SENSE− at the DUT.

The additional measurement would support:

- Checking PA startup voltage before closing the output relay.
- Comparing PA output voltage with sensed DUT voltage to identify excessive cable drop or insufficient operating headroom. Interpretation depends on output and sense connection state.
- Observing PA discharge after disabling the output.
- Inspecting PA startup, overshoot, and settling during development, within the bandwidth and latency of the measurement path.

Use this channel for supervisory monitoring initially. It must not be the sole fast protection mechanism or an assumed substitute for the PA control-loop feedback path.

## Assumptions

- PA output range is up to ±30 V for this example; actual maximum output, rail voltages, and fault voltages remain to be confirmed.
- PA return / FORCE− shares the ADC analog ground reference. A floating output requires a differential circuit instead.
- ADC CH2 operates at PGA gain 1.
- A ±5 V op-amp supply is available.
- The CALBUS connection to CH2 will be disconnected before connecting this monitor.

## Circuit concept

```text
                   Output relay
PA_OUT ───────────────o/ o──────────── FORCE+
   │
   │       R1          R2          R3
   └─────100k────────100k────────100k────●─────(−)
                                        │       │\
                                        │       │ \────●──100Ω─── AIN2P
                                        │       │ /    │             │
                                        │   ┌──(+)     │            10nF
                                        │   │          │             │
                                        └───┼──10k─────┘      AIN2N──●
                                            │                        │
                                           AGND                     AGND

                U1: OPA189 / spare OPA4189 section
                         powered from ±5 V
```

The feedback resistor is 10 kΩ from the op-amp output to its inverting input. The non-inverting input connects to AGND. The feedback connection crosses the AGND wire without connecting to it.

Connection list to remove ambiguity from the drawing:

1. PA_OUT → R1 (100 kΩ) → R2 (100 kΩ) → R3 (100 kΩ) → U1 inverting input.
2. U1 output → Rf (10 kΩ) → U1 inverting input.
3. U1 non-inverting input → AGND.
4. U1 output → Riso (100 Ω) → ADS131M03 AIN2P.
5. ADS131M03 AIN2N → AGND.
6. Cfilter (10 nF) between AIN2P and AIN2N.
7. U1 supply pins → +5 V and −5 V, with local decoupling.

## Scaling

The inverting amplifier has a nominal gain of −10 kΩ / 300 kΩ:

**VADC = −VPA / 30**

| PA output | ADC differential input |
|---:|---:|
| −30 V | +1.00 V |
| −15 V | +0.50 V |
| 0 V | 0 V |
| +15 V | −0.50 V |
| +30 V | −1.00 V |

The ADS131M03 has a nominal ±1.2 V differential full-scale range at gain 1. Its specified absolute input operating range at this gain permits the proposed negative input voltage with AIN2N at AGND. The ±1 V design target leaves measurement headroom; it is not an overvoltage protection specification.

At 30 V, the input branch draws approximately 100 µA from the PA. Locating the tap before the output relay keeps this branch disconnected from the DUT when the relay is open.

## Initial component choices

- R1–R3: 100 kΩ each, 0.1%, low temperature coefficient, with suitable voltage and power ratings for normal and fault conditions.
- Rf: 10 kΩ, 0.1%, preferably with ratio drift matched to the input resistors.
- U1: OPA189 or a spare OPA4189 section. The OPA189 family supports the proposed ±5 V supply.
- Supply decoupling: 100 nF close to each op-amp supply pin, with additional bulk decoupling as appropriate.
- Riso / Cfilter: 100 Ω and 10 nF as starting values. Verify amplifier stability, ADC loading, settling, and noise before finalizing.
- Test points: PA_OUT and U1 output.

The op-amp provides a low-impedance drive rather than connecting a high-resistance passive divider directly to the ADC.

## Protection and layout still to be designed

Protection components are deliberately not specified until PA rail and fault voltages are known. Review:

- Normal operation, PA saturation, and external voltage applied through a closed output relay.
- Op-amp input protection and resistor ratings when amplifier supplies are absent.
- ADC pin protection if U1 saturates or its supplies are present while the ADC is unpowered.
- Clamp leakage, clamp current paths, and possible back-powering of supply rails.
- Analog ground routing and PA return current; use a quiet reference connection rather than a high-current return trace.
- Filter placement near the ADC and interference from the PA and digital switching.

## Firmware implications

- Replace CALBUS interpretation of CH2 with a dedicated PA voltage measurement.
- Undo the nominal inversion and scaling, then apply separate gain/offset calibration.
- Review calibration persistence and channel quality handling before changing the existing firmware; this proposal does not authorize a particular storage-format change.
- Retain clipping, freshness, and validity reporting for CH2.
- Evaluate ADC digital-filter and foreground-processing latency before using this measurement in output sequencing or control.

## Validation before implementation

1. Confirm the maximum PA output, supply rails, fault voltages, and FORCE− relationship to AGND.
2. Finalize protection and verify powered and unpowered input conditions.
3. Simulate the analog path, including filtering and ADC load.
4. Bench-check zero, both polarities, gain, settling, noise, and saturation recovery against a reference instrument.
5. Check PA-to-DUT voltage comparisons under load and with the relay open.
6. Run KiCad ERC and PCB DRC before finalizing the schematic and layout revision.

## References

- [TI ADS131M03 datasheet](https://www.ti.com/lit/ds/symlink/ads131m03.pdf): differential full-scale range and absolute input operating limits.
- [TI OPA189 / OPA2189 / OPA4189 datasheet](https://www.ti.com/lit/ds/symlink/opa4189.pdf): supply range and amplifier characteristics.

No firmware or KiCad design changes are included in this proposal.
