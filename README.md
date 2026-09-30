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

### Core Architecture
* **`/Core`**: Main application logic, interrupt routines, and basic peripheral initialization.
* **`/Drivers`**: Silicon vendor abstraction stacks (CMSIS and STM32H5xx HAL).
* **`my-project.ioc`**: The source configuration file for **STM32CubeMX**. Use this to modify pin assignments, internal clocks, and middleware.

> ⚠️ **Crucial Development Rule:** The codebase uses auto-generated files. You must **only** write custom C code inside the explicit `/* USER CODE BEGIN */` and `/* USER CODE END */` code sections. Any changes written outside these boundaries will be permanently lost if the `.ioc` file is re-generated.

---

## 🛠️ Getting Started (Compilation & Flashing)

For detailed compiler, linker, and AI guardrail guidelines, please refer first to [**`AGENTS.md`**](./AGENTS.md).

### Prerequisites
Ensure you have the following system dependencies installed and added to your environment path:
1. **ARM GCC Toolchain** (`arm-none-eabi-gcc`)
2. **CMake** (v3.22+) & **Ninja** or **Make**
3. **OpenOCD** or **ST-LINK Utility** (for hardware flashing)

### Quick Build Instructions
```bash
# 1. Navigate to the firmware space
cd firmware

# 2. Generate the build files using CMake
mkdir build && cd build
cmake ..

# 3. Compile the binaries (.elf, .bin, .hex)
make -j$(nproc)
```

### Hardware Flashing
Connect your development board via an ST-LINK/V3 debugger and run:
```bash
openocd -f interface/stlink.cfg -f target/stm32h5x.cfg -c "program build/firmware.elf verify reset exit"
```

---

## 🏷️ Versioning Strategy

This project adheres to **Semantic Versioning (v2.0.0)**. Due to the monorepo nature, git tags tie firmware releases directly to specific physical board iterations:
* **`v1.0.0-HW1`**: Initial hardware board spin layout.
* **`v1.0.1-FW`**: Firmware patch fixing a register timing error specifically for the HW1 board revision.
