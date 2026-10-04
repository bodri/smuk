from dataclasses import dataclass, field
import math
import time
from ..protocol.responses import Response
from ..protocol.commands import CURRENT_RANGES, VOLTAGE_RANGES


@dataclass(frozen=True)
class Measurement:
    voltage: float
    current: float
    calbus: float
    current_range: str
    voltage_range: str
    samples: int
    populated: int
    window: int
    valid: bool
    quality: dict[str, str]
    sample_rate_hz: int = 4000
    group_samples: int = 1
    integration_ms: int = 8
    received_at: float = field(default_factory=time.monotonic)

    @property
    def power(self) -> float:
        return self.voltage * self.current

    @classmethod
    def from_responses(cls, measurement: Response, quality: Response):
        f, q = measurement.fields, quality.fields
        voltage, current, bus = (float(f[k]) for k in ("V_V", "I_A", "BUS_V"))
        if not all(math.isfinite(v) for v in (voltage, current, bus)):
            raise ValueError("Nonfinite measurement")
        if f["I"] not in CURRENT_RANGES or f["V"] not in VOLTAGE_RANGES:
            raise ValueError("Unknown measurement range")
        populated, window = map(int, f["window"].split("/"))
        if not 0 <= populated <= window <= 400 or window == 0:
            raise ValueError("Invalid precision window")
        samples = int(f["samples"])
        if not 0 <= samples <= 0xFFFFFFFF:
            raise ValueError("Invalid sample counter")
        valid = f["valid"] == "1" and populated == window
        valid &= all(q.get(k) == "1" for k in ("fresh", "settled", "precision_ready"))
        valid &= all(q.get(k) == "0" for k in ("I_clip", "V_clip", "I_overload", "V_overload"))
        rate = int(f.get("rate_hz", "4000"))
        group = int(f.get("group_samples", "1"))
        integration = int(f.get("integration_ms", str(window // 4)))
        if (rate, group) not in ((4000, 1), (32000, 8)) or integration * 4 != window:
            raise ValueError("Inconsistent sample rate or precision integration metadata")
        return cls(voltage, current, bus, f["I"], f["V"], samples, populated, window, valid, q, rate, group, integration)


@dataclass
class InstrumentState:
    connected: bool = False
    port: str = ""
    pending_change: bool = False
    status: dict[str, str] = field(default_factory=dict)
    acquisition: dict[str, str] = field(default_factory=dict)
    watchdog: dict[str, str] = field(default_factory=dict)
    measurement: Measurement | None = None
    message: str = "Disconnected"

    @property
    def healthy(self) -> bool:
        return self.connected and self.status.get("faults") == "0" and self.status.get("state") == "4"

    @property
    def live(self) -> bool:
        m = self.measurement
        rate_ok = self.acquisition.get("rate_known") != "1" or self.acquisition.get("rate_ok") == "1"
        return bool(self.healthy and rate_ok and not self.pending_change and m and m.valid and time.monotonic() - m.received_at < 1.2 and self.status.get("valid") == "1")
