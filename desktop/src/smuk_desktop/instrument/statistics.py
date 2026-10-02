"""Statistics of distinct observed replies, not all ADC conversions."""
from dataclasses import dataclass
import math
from .state import Measurement


@dataclass
class Summary:
    count: int = 0
    mean: float = 0.0
    minimum: float = math.inf
    maximum: float = -math.inf
    m2: float = 0.0

    def add(self, value: float):
        self.count += 1
        delta = value - self.mean
        self.mean += delta / self.count
        self.m2 += delta * (value - self.mean)
        self.minimum = min(self.minimum, value)
        self.maximum = max(self.maximum, value)


class Statistics:
    def __init__(self):
        self.reset()

    def reset(self):
        self.voltage, self.current, self.power = Summary(), Summary(), Summary()
        self.last_sample: int | None = None

    def observe(self, measurement: Measurement):
        if self.last_sample is not None and measurement.samples < self.last_sample:
            self.reset()  # reset or counter wrap: start a new segment
        if not measurement.valid or measurement.samples == self.last_sample:
            return
        if not all(math.isfinite(v) for v in (measurement.voltage, measurement.current, measurement.power)):
            return
        self.last_sample = measurement.samples
        self.voltage.add(measurement.voltage)
        self.current.add(measurement.current)
        self.power.add(measurement.power)
