#!/bin/bash

# 1. Source Homebrew environmental paths
if [ -f /opt/homebrew/bin/brew ]; then
    eval "$(/opt/homebrew/bin/brew shellenv)"
fi
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"

# 🌟 Hardcode the precise absolute path to your firmware folder
TARGET_DIR="/Users/bodri/Documents/Electronics/smuk/firmware"
ELF_PATH="$TARGET_DIR/build/smuk.elf"

# Force change directory to where the binary actually lives
cd "$TARGET_DIR"

echo "Connecting to background ST-Link GDB server..."
echo "Using ELF binary symbol map at: $ELF_PATH"

# 2. Run interactive GDB passing the corrected variable references
arm-none-eabi-gdb "$ELF_PATH" \
  -ex "target remote localhost:61234" \
  -ex "monitor halt" \
  -ex "break main" \
  -ex "continue"
