# 🚀 smuk
An SMU by kapuki

This repository is structured as a unified monorepo containing both the physical hardware designs (schematics, PCB, enclosures) and the bare-metal C firmware.

---

## 📂 Project Architecture

The workspace is split into isolated modules to decouple hardware revisions from compilation build systems:

```text
smuk/
├── firmware/       # Core firmware application, drivers, and build targets
├── hardware/       # Electrical schematics, PCB layout, and production packages
├── AGENTS.md       # Target rules and context for AI assistants (Claude, Codex)
└── README.md       # This file
```

---

## 📐 1. Hardware Module (`/hardware`)

The electronics are designed using **KiCad 10.0** (or your preferred EDA tool), leveraging the high-performance capabilities of the **STM32H5** series (Cortex-M33 with TrustZone support).

### Key Subdirectories
* **`/pcb`**: Contains the source `.kicad_pro`, `.kicad_sch`, and `.kicad_pcb` files.
* **`/mechanical`**: STEP files for the enclosure, 3D component models, and mounting layouts.
* **`/production`**: **Production-ready files.** Check this folder if you need to quickly inspect the schematics without installing the full EDA design suite.

### Quick Hardware Access
* 📄 **Latest Schematic PDF**: [`/hardware/production/schematic.pdf`](hardware/production/) *(Update path to exact filename)*
* 📦 **Gerber Packages / BOM**: Prepared inside [`/hardware/production/`](hardware/production/) for direct manufacturing ordering.

---

## 💻 2. Firmware Module (`/firmware`)

The software environment is built on top of the **STM32 Cube Hardware Abstraction Layer (HAL)** and configured for cross-compilation using the **GNU ARM Embedded Toolchain**.

### Current Firmware Status

Firmware targets the **STM32H503RB**. ADS131M03 SPI/DMA acquisition, physical measurement conversion, CALBUS and voltage calibration, and persistent Flash calibration storage are working.

Range control, PA safety, watchdog, and parts of the platform interface still contain placeholders awaiting hardware integration. The `App` directory is unfinished and is excluded from the firmware source list.

### Core Architecture

```text
firmware/
├── Core/                 # CubeMX initialization, callbacks, integration loop
├── Drivers/SMU/          # ADC, measurement, calibration and hardware interfaces
├── Platform/STM32H503/   # Board-specific HAL implementations
├── Storage/             # Persistent calibration records and Flash storage
├── App/                 # Unfinished instrument/control implementation
├── tests/               # Host regression tests
└── smuk.ioc              # STM32CubeMX configuration
```

`Drivers` also contains the vendor CMSIS, STM32H5 HAL, and board support libraries. `Drivers/SMU/ads131m03.h/.c` contains both blocking device bring-up and DMA acquisition. Its HAL operations are provided through `ads131m03_port.h`, implemented in `Platform/STM32H503/ads131m03_port_hal.c`.

`smu_measurement` converts raw ADC samples into physical measurements and applies calibration and filtering. `smu_calibration` manages active coefficients, `smu_cal_seq` sequences calibration acquisition, and `Storage/calibration_store` persists calibration records.

> ⚠️ **Crucial Development Rule:** In CubeMX-generated files, write custom C code only inside the explicit `/* USER CODE BEGIN ... */` and `/* USER CODE END ... */` sections. Changes outside these sections may be lost when `smuk.ioc` is regenerated. This restriction does not apply to handwritten drivers and modules.

### Calibration Behavior

Calibration discard and acquisition phases each have a configurable timeout, defaulting to **1,000 ms** per phase. The sequencer accounts for elapsed scheduler time, including delayed ticks. On timeout, it opens calibration relays, selects zero CALBUS, requests PA disable and ranges off through the existing hardware interfaces, and latches a fault until sequencer reinitialization. PA and range actions still depend on the placeholder implementations described above.

Applying calibration coefficients preserves measurement ranges, validity/compliance/transition flags, sample count, and filter configuration. Filter history resets to avoid blending values from different calibrations, and cached outputs remain invalid until another frame is processed. Startup still performs full measurement initialization.

### Calibration Flash Reservation

Application Flash is limited to **112 KiB**, enforced by `check_limits.py` during the build. Two **8 KiB** calibration slots occupy the remaining 16 KiB:

| Region | Address range |
| --- | --- |
| Application | `0x08000000`–`0x0801BFFF` |
| Calibration A | `0x0801C000`–`0x0801DFFF` |
| Calibration B | `0x0801E000`–`0x0801FFFF` |

Saves replace the older or invalid slot and verify the replacement by record validation and byte-for-byte readback. The newest valid calibration is preserved while the replacement is written. Storage initialization does not erase calibration; loading falls back to default coefficients if neither slot is valid.

---

## 🛠️ Getting Started (Compilation & Flashing)

For project guidance, see [AGENTS.md](./AGENTS.md) and the firmware-specific [firmware/AGENTS.md](./firmware/AGENTS.md).

### Prerequisites

Install these dependencies and make them available on `PATH`:

1. **ARM GCC toolchain and binutils** (`arm-none-eabi-gcc`, `arm-none-eabi-size`, `arm-none-eabi-objcopy`)
2. **CMake 3.22+** and **Ninja**
3. **Python 3** for the Flash footprint check
4. A **host C compiler** (`cc`, or set `CC`) for host regression tests

`clang-format` and `cppcheck` are optional. When installed, the build script automatically formats C source and header files under `Core/Src`, `Core/Inc`, `Platform`, `Drivers/SMU`, and `Storage`, and runs static analysis on `Core`, `Platform`, `Drivers/SMU`, and `Storage`. These steps are skipped with a warning if the tools are unavailable.

The supplied flashing and debugging helpers currently require **STM32CubeIDE installed under `/Applications` on macOS**, including its STM32CubeProgrammer and ST-LINK GDB server bundles.

### Build and Test

From the repository root:

```bash
cd firmware
./build.sh
sh tests/run_calibration_state.sh
```

The build uses the ARM toolchain through CMake and Ninja, generates `build/smuk.elf`, `build/smuk.hex`, and `build/smuk.bin`, and checks the 112 KiB application Flash limit.

The host regression tests cover calibration timeouts and cleanup, successful acquisition, zero-CALBUS offset capture, and measurement state preservation when applying coefficients. They run without a connected board and do not replace hardware verification.

In Zed, use **STM32H5: Build Firmware** or the configured build shortcut (`cmd-b` / `ctrl-b`). The **STM32H5: Clean & Rebuild Firmware** task currently passes `--clean`, but `build.sh` does not implement special handling for that argument. Each normal build clears the CMake cache and configuration directory; for a complete clean build, remove `firmware/build` before running the script.

### Hardware Flashing

Connect the board through an ST-LINK debugger, build the firmware, then run from `firmware`:

```bash
./flash.sh
```

The helper programs `build/smuk.elf` using STM32CubeProgrammer over SWD in under-reset mode, verifies the image, and resets the target. The equivalent Zed task is **STM32H5: Flash Firmware**.

### Serial Communication and Manual Calibration

The ST-LINK USB connection provides a USART3 Virtual COM Port at **115200 baud,
8N1, no flow control**. The firmware console supports measurement/raw queries,
range selection, and manual calibration from externally applied references.
Start with `HELP` or `PING`. Select precision windows with `INTEGRATION
1MS|8MS|20MS|50MS|100MS`; precision readings become valid after the window fills.
`CAL:SHOW?` reports calibration provenance, and captures report noise/drift. `ACQ?` reports acquisition health; measurement
queries report freshness, settling, precision readiness, and channel clipping.
Stale acquisition invalidates readings, and persistent failures latch an ADC fault. Current and voltage autorange are controlled
independently with `AUTORANGE:I` and `AUTORANGE:V`; both default to enabled.
Input loading defaults to 10 MΩ between the sense terminals; use `IMPEDANCE HIGHZ`
to disconnect it or `IMPEDANCE 10M` to restore it.
Voltage starts at 15 V and autorange switches up at 6.2 V magnitude and down at
5.0 V after 100 ms. Disable both autoranges during manual calibration.

See [Serial console and manual calibration](firmware/serial_console.md) for the
command list, terminal setup, calibration capture/fit/save workflow, and host tests.

### Debug Server

From `firmware`, run:

```bash
./debug_server.sh
```

This starts the ST-LINK GDB server on port **61234**. Connect a GDB client configured for the ARM target and `build/smuk.elf`. The equivalent Zed task is **STM32H5: Start GDB Debug Server**.

---

## 🏷️ Versioning Strategy

This project adheres to **Semantic Versioning (v2.0.0)**. Due to the monorepo nature, git tags tie firmware releases directly to specific physical board iterations:
* **`v1.0.0-HW1`**: Initial hardware board spin layout.
* **`v1.0.1-FW`**: Firmware patch fixing a register timing error specifically for the HW1 board revision.
