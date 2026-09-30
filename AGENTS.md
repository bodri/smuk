# 🤖 AI Agent & Coding Guidelines

This file provides critical workspace architecture context and guardrails for AI tools (Claude, Copilot, etc.) interacting with this repository.

## 📂 Repository Layout
- **`/firmware`**: Bare-metal C firmware (STM32CubeMX + CMake + GCC ARM Toolchain).
- **`/hardware`**: Physical board schematics, layouts, and production sheets (KiCad 10.0 format).

## 🛠️ Build & Flash Commands
- **Build firmware**: `cd firmware && mkdir -p build && cd build && cmake .. && make -j`
- **Flash firmware**: `openocd -f interface/stlink.cfg -f target/stm32h5x.cfg -c "program firmware/build/firmware.elf verify reset exit"`

## 💻 Embedded C Coding Standards
- **Language**: Strict C99/C11 standard for resource-constrained embedded systems.
- **STM32CubeMX Protection**: 
  - **CRITICAL**: Only insert application code inside explicit `/* USER CODE BEGIN */` and `/* USER CODE END */` comment tags.
  - **NEVER** write code outside these blocks in auto-generated files, or it will be wiped during the next peripheral regeneration.
- **Hardware Abstraction**: Prioritize using the STM32H5 HAL or LL (Low-Layer) libraries. Avoid raw register hacking unless performance demands it.
- **TrustZone Architecture**: The STM32H5 uses ARM TrustZone (Cortex-M33). 
  - Core security configurations reside in `/firmware/Secure`.
  - General application logic, FreeRTOS tasks, and standard communication stacks reside in `/firmware/NonSecure`.
