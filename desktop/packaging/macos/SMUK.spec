# Run from desktop/: .venv/bin/python -m PyInstaller packaging/macos/SMUK.spec
from pathlib import Path
import sysconfig

project = Path(SPECPATH).resolve().parents[1]
python_minimum = sysconfig.get_config_var("MACOSX_DEPLOYMENT_TARGET") or "13.0"
minimum = max(("13.0", python_minimum), key=lambda value: tuple(map(int, value.split("."))))
if "." not in minimum:
    minimum += ".0"
a = Analysis(
    [str(project / "packaging/macos/launcher.py")],
    pathex=[str(project / "src")],
    datas=[(str(project / "src/smuk_desktop/resources"), "smuk_desktop/resources")],
    hiddenimports=[],
)
pyz = PYZ(a.pure)
exe = EXE(pyz, a.scripts, [], exclude_binaries=True, name="SMUK", console=False)
collection = COLLECT(exe, a.binaries, a.datas, name="SMUK")
app = BUNDLE(
    collection,
    name="SMUK.app",
    bundle_identifier="org.smuk.desktop",
    info_plist={
        "CFBundleShortVersionString": "0.1.0",
        "NSHighResolutionCapable": True,
        "LSMinimumSystemVersion": minimum,
    },
)
