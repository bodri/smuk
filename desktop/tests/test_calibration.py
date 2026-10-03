import pytest
from PySide6.QtWidgets import QMessageBox
from smuk_desktop.protocol.commands import validate_command
from smuk_desktop.protocol.parser import Transaction, parse_line
from smuk_desktop.ui.calibration_dialog import CalibrationDialog
from test_controller import session, complete_startup, STATUS, QUALITY, MEAS


def test_capture_waits_for_terminal_point_and_quality():
    tx = Transaction("CAL:CAPTURE -5")
    assert tx.accept(parse_line("OK acquiring 256 samples")) is None
    assert tx.accept(parse_line("ADC background log")) is None
    assert tx.accept(parse_line("CAPTURE QUALITY rms_nominal=0.001 drift_nominal=0 span_nominal=0.01 rms_codes=2 drift_codes=0 samples=256")) is None
    reply = tx.accept(parse_line("OK POINT 0 nominal=-4.9 reference=-5"))
    assert len(reply.responses) == 3
    tx = Transaction("CAL:CAPTURE 1")
    tx.accept(parse_line("OK acquiring 256 samples"))
    assert tx.accept(parse_line("ERR unstable capture; settle reference and retry")).error


def test_fit_waits_for_all_residuals():
    tx = Transaction("CAL:FIT", 2)
    assert tx.accept(parse_line("FIT QUALITY nominal_span=10 reference_span=10")) is None
    assert tx.accept(parse_line("OK FIT gain=1.01 offset=0.001 (staged; CAL:SAVE persists)")) is None
    assert tx.accept(parse_line("RESIDUAL 0 0.0001")) is None
    reply = tx.accept(parse_line("RESIDUAL 1 -0.0001"))
    assert len(reply.responses) == 4


@pytest.mark.parametrize("value", ["nan", "inf", "1e100", "1\nCAL:SAVE", "1 junk"])
def test_reference_validation(value):
    with pytest.raises(ValueError):
        validate_command("CAL:CAPTURE " + value, calibration=True)


def emit(app, connection, *lines):
    for line in lines:
        connection.line_received.emit(line)
    app.processEvents()


def test_manual_workflow_save_confirmation_and_exclusive_controls(app, monkeypatch):
    c, connection = session(app)
    complete_startup(app, c, connection)
    assert c.reserve_calibration()
    d = CalibrationDialog(c)
    emit(app, connection, "CAL active sequence=1 staged_dirty=0 source=FLASH", "CAPTURE LIMIT rms_codes=4096")
    assert d.prepare.isEnabled() and not d.capture.isEnabled()
    before = len(connection.written)
    c.send("RANGE:V 15V")
    assert len(connection.written) == before
    d._prepare()
    for command in ("CAL:ABORT", "AUTORANGE:I OFF", "AUTORANGE:V OFF", "RANGE:I 100UA", "RANGE:V 6V", "IMPEDANCE 10M"):
        assert connection.written[-1] == command
        emit(app, connection, "OK")
    fixed = STATUS.replace("autorange=1", "autorange=0").replace("autorange_v=1", "autorange_v=0")
    emit(app, connection, fixed, QUALITY, "WATCHDOG active=1 reset=0")
    emit(app, connection, MEAS, QUALITY)
    emit(app, connection, "CAL active sequence=1 staged_dirty=0 source=FLASH", "CAPTURE LIMIT rms_codes=4096")
    assert d.begin.isEnabled()
    d.begin.click()
    emit(app, connection, "OK manual calibration; set physical reference, then CAL:CAPTURE value")
    for index, value in enumerate(("0", "3")):
        d.reference.setText(value)
        d.capture.click()
        assert connection.written[-1] == f"CAL:CAPTURE {value}"
        emit(app, connection, "OK acquiring 256 samples")
        assert not d.capture.isEnabled() and c.transaction.command.startswith("CAL:CAPTURE")
        emit(app, connection, "CAPTURE QUALITY rms_nominal=0.0001 drift_nominal=0", f"OK POINT {index} nominal={value} reference={value}")
        assert connection.written[-1] == "CAL:POINTS?"
        for i in range(index + 1):
            emit(app, connection, f"POINT {i} nominal={i * 3} reference={i * 3}")
            emit(app, connection, f"POINT QUALITY {i} rms_nominal=0.0001 drift_nominal=0")
        emit(app, connection, f"OK points={index + 1}")
    d.fit.click()
    emit(app, connection, "FIT QUALITY nominal_span=3 reference_span=3", "ERR invalid or degenerate fit")
    assert not d.save.isEnabled() and d.fit.isEnabled()
    assert connection.written[-1] == "STATUS?"
    emit(app, connection, fixed, QUALITY, "WATCHDOG active=1 reset=0")
    d.fit.click()
    emit(app, connection, "FIT QUALITY nominal_span=3 reference_span=3", "OK FIT gain=1 offset=0 (staged; CAL:SAVE persists)")
    assert not d.save.isEnabled()
    emit(app, connection, "RESIDUAL 0 0", "RESIDUAL 1 0")
    assert d.save.isEnabled()
    monkeypatch.setattr(QMessageBox, "question", lambda *args: QMessageBox.StandardButton.No)
    d.save.click()
    assert "CAL:SAVE" not in connection.written
    monkeypatch.setattr(QMessageBox, "question", lambda *args: QMessageBox.StandardButton.Yes)
    d.save.click()
    assert connection.written[-1] == "CAL:SAVE"
    emit(app, connection, "OK saved sequence=2")
    emit(app, connection, "CAL active sequence=2 staged_dirty=0 source=FLASH", "CAPTURE LIMIT rms_codes=4096")
    emit(app, connection, fixed, QUALITY, "WATCHDOG active=1 reset=0")
    emit(app, connection, MEAS.replace("samples=20", "samples=30"), QUALITY)
    assert not d.save.isEnabled()
    d.close()
    assert c.calibration_active
    emit(app, connection, "OK capture aborted; staged fits retained")
    assert not c.calibration_active
    c.disconnect()


def test_disconnect_invalidates_dialog_and_discard_requires_confirmation(app, monkeypatch):
    c, connection = session(app)
    complete_startup(app, c, connection)
    assert c.reserve_calibration()
    d = CalibrationDialog(c)
    emit(app, connection, "CAL active sequence=1 staged_dirty=1 source=FLASH", "CAPTURE LIMIT rms_codes=4096")
    monkeypatch.setattr(QMessageBox, "question", lambda *args: QMessageBox.StandardButton.No)
    d.discard.click()
    assert "CAL:RESET" not in connection.written
    monkeypatch.setattr(QMessageBox, "question", lambda *args: QMessageBox.StandardButton.Yes)
    d.discard.click()
    assert connection.written[-1] == "CAL:RESET"
    emit(app, connection, "OK staged calibration reset")
    emit(app, connection, "CAL active sequence=1 staged_dirty=0 source=FLASH", "CAPTURE LIMIT rms_codes=4096")
    assert not d.dirty
    c.disconnect()
    assert not d.prepare.isEnabled() and not d.capture.isEnabled() and not d.save.isEnabled()
    d.close()


def test_capture_timeout_disconnects_without_resuming_session(app):
    c, connection = session(app)
    complete_startup(app, c, connection)
    assert c.reserve_calibration()
    c.send("CAL:CAPTURE 1", calibration=True)
    assert c.timeout.interval() == 3000
    emit(app, connection, "OK acquiring 256 samples")
    c._timed_out()
    assert not c.state.connected and not c.calibration_active
    emit(app, connection, "OK POINT 0 nominal=1 reference=1")
    assert c.calibration_points == 0
