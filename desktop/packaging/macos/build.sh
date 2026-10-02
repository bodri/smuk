#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$project_dir"
# Remove only this project’s generated application outputs before rebuilding.
rm -rf "$project_dir/dist/SMUK" "$project_dir/dist/SMUK.app"
# Keep packaging caches inside the project, including in restricted workspaces.
PYINSTALLER_CONFIG_DIR="$project_dir/build/pyinstaller-cache" \
    "$project_dir/.venv/bin/python" -m PyInstaller --clean --noconfirm packaging/macos/SMUK.spec
