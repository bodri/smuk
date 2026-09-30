# 🤖 Project Architecture & Agent Router

This repository is a hardware/firmware monorepo. Context is scoped hierarchically. When working inside a specific directory, defer strictly to the local `AGENTS.md` file found in that space.

## 📂 Repository Context Routing
- **`/hardware`**: Electrical layouts, PCB tracks, and manufacturing documents (KiCad 8.0).
  👉 *See `/hardware/AGENTS.md` for schematic and physical board guidelines.*
- **`/firmware`**: Bare-metal C code running on the ARM Cortex-M33 (CMake + GCC Toolchain).
  👉 *See `/firmware/AGENTS.md` for compilation steps and peripheral constraints.*

## 🛠️ Global Workspace Automation (Zed Editor integration)
The project utilizes Zed Tasks mapped to keyboard shortcuts. You can invoke these directly or guide the developer to run them:
- **Build Firmware:** `cmd-b` / `ctrl-b` (Executes the primary CMake build compilation pipeline)
- **Task Runner Labels:** Available via Zed command palette (`task: spawn`):
  - `"STM32H5: Build Firmware"`
  - `"STM32H5: Clean & Rebuild Firmware"`
