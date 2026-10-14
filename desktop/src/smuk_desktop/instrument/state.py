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
    integration_ms: float = 8
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
        integration = int(f["integration_us"]) / 1000 if "integration_us" in f else float(f.get("integration_ms", str(window / 4)))
        if (rate, group) not in ((4000, 1), (32000, 8)) or integration * 4 != window:
            raise ValueError("Inconsistent sample rate or precision integration metadata")
        return cls(voltage, current, bus, f["I"], f["V"], samples, populated, window, valid, q, rate, group, integration)


@dataclass(frozen=True)
class Source:
    state: int
    requested: float
    applied: float
    minimum: float
    maximum: float
    slew: float
    pa_always_enabled: bool
    relay_available: bool
    received_at: float = field(default_factory=time.monotonic)

    @classmethod
    def from_response(cls, response):
        f = response.fields
        values = [float(f[k]) for k in ("requested_V", "applied_V", "minimum_V", "maximum_V", "slew_V_s")]
        state = int(f["state"])
        if f["mode"] != "FV" or state not in range(5) or not all(math.isfinite(v) for v in values):
            raise ValueError("Invalid FV source metadata")
        requested, applied, minimum, maximum, slew = values
        if state != 0 and (minimum >= maximum or slew <= 0 or not minimum <= requested <= maximum or not minimum <= applied <= maximum):
            raise ValueError("Invalid FV source bounds")
        if f["pa_always_enabled"] not in ("0", "1") or f["relay_available"] not in ("0", "1"):
            raise ValueError("Invalid FV hardware capabilities")
        return cls(state, *values, f["pa_always_enabled"] == "1", f["relay_available"] == "1")


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
    source: Source | None = None
    source_pending: bool = False

    @property
    def healthy(self) -> bool:
        return self.connected and self.status.get("faults") == "0" and self.status.get("state") == "4"

    @property
    def live(self) -> bool:
        m = self.measurement
        rate_ok = self.acquisition.get("rate_known") != "1" or self.acquisition.get("rate_ok") == "1"
        return bool(self.healthy and rate_ok and not self.pending_change and m and m.valid and time.monotonic() - m.received_at < 1.2 and self.status.get("valid") == "1")

    @property
    def source_known(self) -> bool:
        return bool(self.connected and self.source and time.monotonic() - self.source.received_at < 3)

    @property
    def source_busy(self) -> bool:
        return bool(self.source_known and self.source.state in (2, 3))

    @property
    def source_prerequisites(self) -> list[str]:
        reasons = []
        if not self.source_known or self.source.state == 0:
            reasons.append("FV source unavailable or status unknown")
        if not self.live:
            reasons.append("Wait for healthy, settled acquisition")
        if self.acquisition.get("rate_known") != "1" or self.acquisition.get("rate_ok") != "1":
            reasons.append("Wait for confirmed ADC sample rate")
        if self.status.get("autorange") != "0" or self.status.get("autorange_v") != "0":
            reasons.append("Turn both autoranges OFF")
        if self.status.get("I") != "1.5A":
            reasons.append("Select the 1.5 A current range")
        if self.status.get("impedance") != "HIGHZ" or self.status.get("impedance_requested") != "HIGHZ":
            reasons.append("Select HIGHZ input")
        if self.status.get("capture") != "0":
            reasons.append("End calibration capture")
        return reasons
