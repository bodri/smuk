# 💻 Firmware Agent Guidelines (STM32H5)

You are operating inside the embedded software workspace. Code generated here executes on an **STM32H5 High-Performance Microcontroller** (ARM Cortex-M33 architecture).

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
