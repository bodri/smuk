from dataclasses import replace
import math
import time
import pytest
from PySide6.QtWidgets import QDialogButtonBox
from smuk_desktop.instrument.state import Source
from smuk_desktop.protocol.parser import parse_line, Transaction
from smuk_desktop.protocol.commands import validate_command
from smuk_desktop.ui.main_window import MainWindow
from smuk_desktop.ui.source_dialog import SourceDialog
from test_controller import session, complete_startup

SOURCE = "SOURCE mode=FV state=1 requested_V=0 applied_V=0 code=0 backend=STM32_DAC pa_always_enabled=1 relay_available=0 minimum_V=0 maximum_V=1 slew_V_s=0.1"


def source_session(app):
    c, conn = session(app)
    complete_startup(app, c, conn)
    c.send("SOURCE?")
    conn.line_received.emit(SOURCE)
    app.processEvents()
    c.state.status.update(I="1.5A", V="15V", autorange="0", autorange_v="0", impedance="HIGHZ", impedance_requested="HIGHZ")
    c.state.acquisition.update(rate_known="1", rate_ok="1")
    return c, conn


def test_source_parser_and_validation():
    tx = Transaction("SOURCE?")
    assert tx.accept(parse_line("ADC log")) is None
    reply = tx.accept(parse_line(SOURCE))
    source = Source.from_response(reply.responses[0])
    assert source.minimum == 0 and source.maximum == 1 and source.pa_always_enabled
    assert not source.relay_available
    for broken in (SOURCE.replace("maximum_V=1", "maximum_V=nan"), SOURCE.replace("state=1", "state=7"), SOURCE.replace("minimum_V=0", "minimum_V=2")):
        with pytest.raises(ValueError):
            Source.from_response(parse_line(broken))
    with pytest.raises(ValueError):
        parse_line(SOURCE + " state=3")
    for command in ("SOURCE:V NAN", "SOURCE:V INF", "SOURCE:V 1\nOUTPUT ON"):
        with pytest.raises(ValueError):
            validate_command(command)
    assert validate_command("source:v -0.5") == "SOURCE:V -0.5"


def test_setpoint_confirmed_only_after_source_query(app):
    c, conn = source_session(app)
    c.send("SOURCE:V 0.5")
    assert c.state.source.requested == 0 and c.state.source_pending
    conn.line_received.emit("OK voltage requested")
    app.processEvents()
    assert conn.written[-1] == "SOURCE?"
    assert c.state.source.requested == 0
    conn.line_received.emit(SOURCE.replace("requested_V=0", "requested_V=0.5"))
    app.processEvents()
    assert c.state.source.requested == 0.5 and not c.state.source_pending
    c.disconnect()


def test_bounds_and_on_prerequisites(app):
    c, conn = source_session(app)
    before = len(conn.written)
    c.send("SOURCE:V -0.5")
    c.send("SOURCE:V 1.1")
    c.state.status["autorange"] = "1"
    c.send("OUTPUT ON")
    assert len(conn.written) == before
    c.state.status["autorange"] = "0"
    c.send("OUTPUT ON")
    assert conn.written[-1] == "OUTPUT ON" and c.state.source.state == 1
    c.disconnect()


def test_off_prioritizes_and_cancels_queued_on(app):
    c, conn = source_session(app)
    c.send("MEAS?")
    c.send("OUTPUT ON")
    assert "OUTPUT ON" in c.user_queue
    c.send("OUTPUT OFF")
    assert list(c.user_queue) == ["OUTPUT OFF"]
    c.disconnect()


def test_disconnect_and_stale_never_enable_or_restore(app):
    c, conn = source_session(app)
    c.state.source = replace(c.state.source, received_at=time.monotonic() - 4)
    assert not c.reserve_calibration()
    before = len(conn.written)
    c.send("OUTPUT ON")
    assert len(conn.written) == before
    c.send("OUTPUT OFF")
    assert conn.written[-1] == "OUTPUT OFF"
    c.disconnect()
    assert c.state.source is None and not c.state.source_known
    c.connect_port("test-port")
    assert conn.written[-1] == "PING" and "OUTPUT ON" not in c.user_queue
    c.disconnect()


def test_dialog_buttons_trip_and_capability_labels(app):
    c, conn = source_session(app)
    window = MainWindow(c)
    assert window.display.voltage_range.isEnabled()
    assert window.display.output.text() == "OUTPUT ON"
    assert "PA always enabled" in window.display.source_detail.text()
    assert "no isolation relay" in window.display.source_detail.text()
    dialog = SourceDialog(c)
    assert dialog.voltage.minimum() == 0 and dialog.voltage.maximum() == 1
    dialog.voltage.setValue(0.5)
    dialog.buttons.button(QDialogButtonBox.StandardButton.Apply).click()
    assert conn.written[-1] == "SOURCE:V 0.500000"
    c.state.source_pending = False
    c.state.source = replace(c.state.source, state=3, requested=0.5, applied=0.1)
    c.changed.emit()
    assert "ramping" in window.display.source_detail.text()
    assert not window.ranges.isEnabled() and not window.calibrate.isEnabled()
    c.state.source = replace(c.state.source, state=4, requested=0.5)
    c.changed.emit()
    assert "TRIPPED" in window.display.output.text() and window.display.output.isEnabled()
    c.disconnect()
    assert not window.display.output.isEnabled()
    window.close()


def test_older_firmware_keeps_measurements_without_fv(app):
    c, conn = session(app)
    complete_startup(app, c, conn)
    assert c.state.connected and c.state.live and c.state.source is None
    assert not c.source_supported
    c.disconnect()


def test_installed_converter_signed_bounds_and_setpoint(app):
    c, conn = source_session(app)
    c.send("SOURCE?")
    conn.line_received.emit(SOURCE.replace("minimum_V=0", "minimum_V=-1"))
    app.processEvents()
    dialog = SourceDialog(c)
    assert dialog.voltage.minimum() == -1 and dialog.voltage.maximum() == 1
    dialog.voltage.setValue(-0.5)
    dialog.buttons.button(QDialogButtonBox.StandardButton.Apply).click()
    assert conn.written[-1] == "SOURCE:V -0.500000"
    assert c.state.source_pending and c.state.source.requested == 0
    dialog.close()
    c.disconnect()


def test_fv_accepts_6v_measurement_range(app):
    c, conn = source_session(app)
    c.state.status["V"] = "6V"
    before = len(conn.written)
    c.send("OUTPUT ON")
    assert len(conn.written) == before + 1
    assert conn.written[-1] == "OUTPUT ON"
    assert not c.state.source_prerequisites
    c.disconnect()


def test_full_bipolar_bounds_from_firmware(app):
    c, conn = source_session(app)
    c.send("SOURCE?")
    conn.line_received.emit(SOURCE.replace("minimum_V=0", "minimum_V=-15").replace("maximum_V=1", "maximum_V=15").replace("slew_V_s=0.1", "slew_V_s=10"))
    app.processEvents()
    dialog = SourceDialog(c)
    assert dialog.voltage.minimum() == -15 and dialog.voltage.maximum() == 15
    dialog.voltage.setValue(-15)
    dialog.buttons.button(QDialogButtonBox.StandardButton.Apply).click()
    assert conn.written[-1] == "SOURCE:V -15.000000"
    dialog.close()
    c.disconnect()
