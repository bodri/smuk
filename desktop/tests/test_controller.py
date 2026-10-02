from PySide6.QtCore import QObject, Signal
from smuk_desktop.instrument.controller import Controller

STATUS = "STATUS state=4 faults=0 I=100UA V=6V autorange=1 autorange_v=1 impedance_requested=10M impedance=10M valid=1 frames=20 capture=0 points=0"
QUALITY = "QUALITY fresh=1 settled=1 precision_ready=1 I_clip=0 V_clip=0 BUS_clip=0 I_overload=0 V_overload=0"
MEAS = "MEAS I_A=0.000001 V_V=3 BUS_V=0 I=100UA V=6V valid=1 samples=20 window=32/32"


class FakeConnection(QObject):
    opened = Signal(str)
    closed = Signal()
    failed = Signal(str)
    line_received = Signal(str)

    def __init__(self):
        super().__init__()
        self.written = []
        self.is_open = False

    def open(self, path):
        self.is_open = True
        self.opened.emit(path)

    def close(self):
        if self.is_open:
            self.is_open = False
            self.closed.emit()

    def write(self, text):
        self.written.append(text)

    @staticmethod
    def available_ports():
        return []


def session(app):
    connection = FakeConnection()
    controller = Controller(connection)
    controller.connect_port("test-port")
    assert connection.written == ["PING"]
    connection.line_received.emit("ADC harmless log")
    connection.line_received.emit("PONG")
    app.processEvents()
    assert connection.written[-1] == "ECHO OFF"
    connection.line_received.emit("OK")
    app.processEvents()
    assert connection.written[-1] == "STATUS?"
    return controller, connection


def complete_startup(app, controller, connection):
    for line in (STATUS, QUALITY, "WATCHDOG active=1 reset=0"):
        connection.line_received.emit(line)
    app.processEvents()
    assert connection.written[-1] == "CAL:SHOW?"
    connection.line_received.emit("CAL active sequence=1 staged_dirty=0 source=FLASH")
    connection.line_received.emit("CAPTURE LIMIT rms_codes=4096 drift_codes=8192 minimum_fit_span_codes=64")
    app.processEvents()
    assert connection.written[-1] == "ACQ?"
    connection.line_received.emit("ACQ stale=0 crc=0 spi=0 busy=1 overruns=0 gaps=0")
    app.processEvents()
    assert connection.written[-1] == "MEAS?"
    connection.line_received.emit(MEAS)
    connection.line_received.emit(QUALITY)
    app.processEvents()


def test_serialization_and_user_priority(app):
    c, connection = session(app)
    before = len(connection.written)
    connection.line_received.emit(STATUS)
    connection.line_received.emit(QUALITY)
    app.processEvents()
    assert len(connection.written) == before  # waits for WATCHDOG
    connection.line_received.emit("WATCHDOG active=1 reset=0")
    app.processEvents()
    c.user_queue.clear()
    c.poll_queue.clear()
    assert c.transaction.command == "CAL:SHOW?"
    c._enqueue_poll("MEAS?")
    c.send("RANGE:V 15V")
    connection.line_received.emit("CAL active sequence=1 source=FLASH")
    connection.line_received.emit("CAPTURE LIMIT rms_codes=4096")
    app.processEvents()
    assert connection.written[-1] == "RANGE:V 15V"
    assert c.state.status["V"] == "6V"  # still confirmed old range
    c.disconnect()


def test_live_statistics_disconnect_and_timeout(app):
    c, connection = session(app)
    complete_startup(app, c, connection)
    assert c.state.live and c.statistics.voltage.count == 1
    c.send("MEAS?")
    connection.line_received.emit(MEAS)
    connection.line_received.emit(QUALITY)
    app.processEvents()
    assert c.statistics.voltage.count == 1
    c.send("PING")
    c._timed_out()
    assert not c.state.connected and not c.state.live and not connection.is_open
    assert c.transaction is None and not c.user_queue
    assert "Timeout" in c.state.message
    connection.line_received.emit("PONG")
    assert not c.state.connected  # late reply cannot revive connection


def test_device_reset_and_fault_invalidate_immediately(app):
    c, connection = session(app)
    complete_startup(app, c, connection)
    connection.line_received.emit("SMU fault bits=0x00000004")
    assert not c.state.live and not c.state.healthy
    assert c.state.status["faults"] == "4"
    connection.line_received.emit("ADC ADS131M03 bring-up")
    assert not c.state.connected and c.statistics.voltage.count == 0
    assert "restarted" in c.state.message


def test_rejected_setting_never_updates_confirmed_state(app):
    c, connection = session(app)
    complete_startup(app, c, connection)
    c.send("RANGE:V 15V")
    connection.line_received.emit("ERR range request status=2")
    app.processEvents()
    assert c.state.status["V"] == "6V"
    assert c.state.connected
    c.disconnect()


import pytest


@pytest.mark.parametrize("error", ["ERR TX overflow; retry query", "ERR RX loss; resend after newline"])
def test_transport_overflow_requires_reconnect(app, error):
    c, connection = session(app)
    complete_startup(app, c, connection)
    c.send("MEAS?")
    connection.line_received.emit(error)
    assert not c.state.connected and not connection.is_open
    assert "resynchronize" in c.state.message


def test_stale_measurement_is_retained_but_not_live(app):
    from dataclasses import replace
    import time
    c, connection = session(app)
    complete_startup(app, c, connection)
    c.state.measurement = replace(c.state.measurement, received_at=time.monotonic() - 2)
    assert not c.state.live and c.state.measurement.voltage == 3
    c.disconnect()


def test_setting_invalidates_old_reading_until_new_measurement(app):
    c, connection = session(app)
    complete_startup(app, c, connection)
    assert c.state.live
    c.send("MEAS?")
    c.send("RANGE:V 15V")
    assert c.state.pending_change and not c.state.live
    connection.line_received.emit(MEAS)
    connection.line_received.emit(QUALITY)
    app.processEvents()
    assert not c.state.live  # reply was requested before queued range change
    assert connection.written[-1] == "RANGE:V 15V"
    connection.line_received.emit("OK range requested; wait for STATUS valid=1")
    app.processEvents()
    assert connection.written[-1] == "STATUS?"
    for line in (STATUS.replace("V=6V", "V=15V").replace("autorange_v=1", "autorange_v=0"), QUALITY, "WATCHDOG active=1 reset=0"):
        connection.line_received.emit(line)
    app.processEvents()
    assert connection.written[-1] == "MEAS?"
    connection.line_received.emit(MEAS.replace("V=6V", "V=15V").replace("samples=20", "samples=40"))
    connection.line_received.emit(QUALITY)
    app.processEvents()
    assert c.state.live and not c.state.pending_change
    c.disconnect()
