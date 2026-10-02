from dataclasses import replace
from smuk_desktop.instrument.state import Measurement
from smuk_desktop.instrument.statistics import Statistics


def measurement(samples, voltage=3, current=1, valid=True):
    return Measurement(voltage, current, 0, "1.5A", "6V", samples, 32, 32, valid, {})


def test_distinct_valid_observations_and_counter_reset():
    stats = Statistics()
    stats.observe(measurement(100))
    stats.observe(measurement(100, 900))
    stats.observe(measurement(101, 1000, valid=False))
    stats.observe(measurement(102, 5, -1))
    assert stats.voltage.count == 2
    assert stats.voltage.mean == 4 and stats.voltage.minimum == 3 and stats.voltage.maximum == 5
    assert stats.power.minimum == -5 and stats.power.maximum == 3
    stats.observe(measurement(1, 7))
    assert stats.voltage.count == 1 and stats.voltage.mean == 7
    stats.observe(replace(measurement(2), voltage=float("nan")))
    assert stats.voltage.count == 1
