"""Only commands whose reply boundaries are known can enter the queue."""
VOLTAGE_RANGES = ("15V", "6V")
CURRENT_RANGES = ("1.5A", "100MA", "10MA", "1MA", "100UA")
INTEGRATIONS = ("1MS", "8MS", "20MS", "50MS", "100MS")
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


def validate_command(text: str) -> str:
    if any(c in text for c in "\r\n\x00"):
        raise ValueError("Send one command at a time")
    command = " ".join(text.strip().upper().split())
    if command in QUERIES:
        return command
    parts = command.split(" ")
    if len(parts) == 2 and parts[0] in VALUES and parts[1] in VALUES[parts[0]]:
        return command
    raise ValueError("Unsupported command or value; calibration capture/save will be added separately")


def changes_measurement(command: str) -> bool:
    return command.split(" ", 1)[0] in {
        "RANGE:I", "RANGE:V", "AUTORANGE", "AUTORANGE:I", "AUTORANGE:V", "IMPEDANCE", "INTEGRATION"
    }
