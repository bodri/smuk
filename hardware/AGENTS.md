# 📐 Hardware Agent Guidelines (KiCad 10.0)

You are operating inside the electrical engineering and PCB design workspace. 

## 📂 Design Structure
- **`/pcb`**: Native schematic layouts (`.kicad_sch`) and routing layers (`.kicad_pcb`).
- **`/production`**: Final output plots. **DO NOT** suggest modifying files in this folder directly. Any changes to the BOM, CPL, or Gerbers must occur by modifying the source sheets in `/pcb` and plotting a new layer release.

## 🤖 System Reasoning Rules
- **Component Safety:** When generating connection ideas, verify the STM32H5 pinout capabilities (e.g., ensuring 5V tolerant pins are chosen if interfacing with legacy logic systems).
- **Physical Tracking:** Always remind the developer to run a structural Design Rule Check (DRC) inside KiCad before finalizing a trace layout revision change.
