from smuk_desktop.instrument.controller import Controller
from smuk_desktop.ui.main_window import MainWindow
from smuk_desktop.ui.measurement_display import format_value
from test_controller import FakeConnection, session, complete_startup


def test_disconnected_controls_and_window(app):
    c = Controller(FakeConnection())
    window = MainWindow(c)
    assert not window.ranges.isEnabled()
    assert not window.console.input.isEnabled()
    assert "— V" in window.display.voltage.text()
    window.show()
    app.processEvents()
    assert not window.grab().isNull()
    window.close()


def test_confirmed_live_display_and_control_commands(app):
    c, connection = session(app)
    window = MainWindow(c)
    complete_startup(app, c, connection)
    assert window.ranges.isEnabled()
    assert window.ranges.voltage.currentText() == "Auto"
    assert "3.000 00 V" in window.display.voltage.text()
    assert "LIVE" in window.display.quality.text()
    window.ranges.command.emit("RANGE:V 15V")
    assert connection.written[-1] == "RANGE:V 15V"
    assert c.state.status["V"] == "6V"
    window.close()
    assert not c.state.live


def test_engineering_units():
    assert "0.012 00 mA" in format_value(12e-6, "A")
    assert "mA" in format_value(-0.012, "A")
    assert " A" in format_value(1.2, "A")


def test_32ksps_display_and_rate_mismatch(app):
    from test_controller import MEAS, QUALITY
    c, connection = session(app)
    window = MainWindow(c)
    complete_startup(app, c, connection)
    c.send("MEAS?")
    connection.line_received.emit(MEAS.replace("window=32/32", "window=80/80") + " rate_hz=32000 group_samples=8 integration_ms=20")
    connection.line_received.emit(QUALITY)
    app.processEvents()
    assert "640/640 samples" in window.display.quality.text()
    assert window.ranges.integration.currentText() == "20MS"
    c.send("ACQ?")
    connection.line_received.emit("ACQ rate_hz=32000 frame_hz=16000 drdy_hz=32000 rate_known=1 rate_ok=0")
    app.processEvents()
    assert not c.state.live
    assert "RATE MISMATCH" in window.health.text()
    window.close()
