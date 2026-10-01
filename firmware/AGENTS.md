# 💻 Firmware Agent Guidelines (STM32H5)

You are operating inside the embedded software workspace. Code generated here executes on an **STM32H5 High-Performance Microcontroller** (ARM Cortex-M33 architecture).

The ADS131M03 SPI/GPDMA acquisition, measurement conversion, CALBUS calibration, voltage calibration, and Flash calibration storage are currently working and must remain functional. Do not look at the code in the App directory it does not work yet. Do not modify it.

First inspect the complete project and propose a refactoring plan before editing anything.

## Desired architecture

- main.c: CubeMX/HAL initialization and minimal main loop only
- ADS131M03 driver: ADC communication/acquisition only
- smu_measurement: raw ADC → physical measurements, filtering, calibration application
- smu_ranges: sole owner of voltage/current range GPIO control and synchronization with measurement range state
- smu_calibration: calibration coefficients/fitting/application
- smu_cal_seq: safe calibration acquisition sequencing
- calibration_store: Flash persistence only
- I really like to make the implementation HAL agnostic. For example, you can see ads131m03_bringup: all code which needs to call the HAL goes into the ads131m03_bringup_port. And the implemetation of the port goes into the ads131m03_bringup_port_hal file under the Platform directory. In this way, the HAL implementation can be swapped out without modifying the rest of the code. Please follow this pattern when adding new HAL agnostic code.

## Important constraints

- Do not change working ADS131M03 register setup, SPI/DMA timing, ISR behavior, CRC handling, or frame format.
- Do not change measurement equations or calibration coefficients.
- Do not change Flash layout or calibration record format.
- Do not change GPIO polarities based on assumptions.
- Avoid dynamic allocation.
- Keep ISR work minimal.
- Make the refactor in small buildable steps.
- After each step, build the project and fix compilation errors before continuing.
- Preserve existing debugger/test hooks initially; remove obsolete bring-up code only after identifying it.

## ⚠️ CRITICAL RULES (Code Destruction Prevention)
- **STM32CubeMX Guardrails:** This project uses auto-generated initializers. 
  - **ONLY** insert application code inside explicit `/* USER CODE BEGIN <Name> */` and `/* USER CODE END <Name> */` comment lines.
  - **NEVER** write or edit code outside these blocks in auto-generated files (e.g., `main.c`, `stm32h5xx_it.c`), or it will be wiped upon peripheral recalculation.

## 💻 Embedded C Programming Standards
- **Standard:** Strict C99/C11 compilation profiles for resource-constrained environments.
- **Hardware Abstraction:** Prioritize utilizing the official STM32H5 HAL (Hardware Abstraction Layer) or LL (Low-Layer) libraries. Avoid direct, naked register parsing unless explicitly requested for execution efficiency.
- **TrustZone Handling:** The Cortex-M33 implements physical security boundaries:
  - Secure World calculations go into `/firmware/Secure/`.
  - Application code, FreeRTOS tasks, and peripheral handling reside in `/firmware/NonSecure/`.

## 🛠️ Local Build & Hardware Flashing Mechanics
Compilation and deployment run out-of-source via automated helper tools. You can instruct the developer to use the following integrated Zed Task hooks:

*   **Build Project Tree:** Run command `./build.sh` (or press **`cmd-b`** / **`ctrl-b`**).
*   **Wipe & Rebuild:** Run command `./build.sh --clean`.
*   **Flash Microcontroller:** Deploys the target output (`firmware/build/stm32h5-firmware.elf`) via OpenOCD over an ST-LINK v3 interface. Trigger by running the `"STM32H5: Flash Firmware"` Zed task (or press **`cmd-shift-b`** / **`ctrl-shift-b`**).

## 🚨 Static Analysis & Linting Protocols
- **Style Rules:** Code formatting follows the local `.clang-format` profile (Allman style braces, 4-space indents). Before committing modifications, run `clang-format -i <file>` to automatically polish layout aesthetics.
- **Bug Prevention:** The build loop leverages `cppcheck`. Avoid generating naked arithmetic type pointer adjustments or uninitialized peripheral buffer indexes that violate static stability protocols.
- **Local Validation Gate:** Execute `./build.sh` locally before submitting a Pull Request to catch styling discrepancies and analysis exceptions before the CI runner flags them.

## 🚨 Memory Limits & Calibration Guardrails
- **Flash Memory Constraint:** The maximum flash allocation for application code is **strictly bounded at 112 KB**. 
- **Calibration Area Safety:** The final sectors of the physical internal flash are reserved exclusively for device calibration. Do not exceed 112 KB.
- **Footprint Validation:** The build pipeline enforces this checking mechanism automatically via `python3 check_limits.py`. If a code modification expands sector metrics over the budget allocation, the verification loop returns an execution error state.
