"""Only commands whose reply boundaries are known can enter the queue."""
import math
import re
VOLTAGE_RANGES = ("15V", "6V")
CURRENT_RANGES = ("1.5A", "100MA", "10MA", "1MA", "100UA")
INTEGRATIONS = ("500US", "1MS", "2MS", "5MS", "8MS", "10MS", "20MS", "50MS", "100MS")
QUERIES = {"PING", "STATUS?", "MEAS?", "RAW?", "ACQ?", "HELP", "CAL:SHOW?", "CAL:POINTS?"}
VALUES = {
    "RANGE:V": VOLTAGE_RANGES,
    "RANGE:I": CURRENT_RANGES,
    "AUTORANGE": ("ON", "OFF"),
    "AUTORANGE:I": ("ON", "OFF"),
    "AUTORANGE:V": ("ON", "OFF"),
    "IMPEDANCE": ("10M", "HIGHZ"),
    "INTEGRATION": INTEGRATIONS,
    "ECHO": ("ON", "OFF"),
}


def validate_command(text: str, *, calibration: bool = False) -> str:
    if any(c in text for c in "\r\n\x00"):
        raise ValueError("Send one command at a time")
    command = " ".join(text.strip().upper().split())
    if command in QUERIES:
        return command
    parts = command.split(" ")
    if calibration:
        if command in {"CAL:FIT", "CAL:SAVE", "CAL:RESET", "CAL:ABORT"}:
            return command
        if len(parts) == 2 and parts[0] == "CAL:BEGIN" and parts[1] in ("V", "I", "BUS"):
            return command
        if len(parts) == 2 and parts[0] == "CAL:CAPTURE":
            value = parts[1]
            if re.fullmatch(r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:E[+-]?\d+)?", value) and math.isfinite(float(value)) and abs(float(value)) <= 3.4028235e38:
                return command
    if len(parts) == 2 and parts[0] in VALUES and parts[1] in VALUES[parts[0]]:
        return command
    raise ValueError("Unsupported command or value; use the manual calibration dialog for calibration controls")


def changes_measurement(command: str) -> bool:
    return command.split(" ", 1)[0] in {
        "RANGE:I", "RANGE:V", "AUTORANGE", "AUTORANGE:I", "AUTORANGE:V", "IMPEDANCE", "INTEGRATION", "CAL:SAVE"
    }
