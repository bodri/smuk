"""Manual references are set by the operator; firmware owns capture and fitting."""
from collections import deque
from datetime import datetime
from pathlib import Path

from PySide6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, QComboBox, QPushButton,
    QLineEdit, QTableWidget, QTableWidgetItem, QPlainTextEdit, QMessageBox,
    QFileDialog,
)
from ..protocol.commands import CURRENT_RANGES, VOLTAGE_RANGES, validate_command


class CalibrationDialog(QDialog):
    def __init__(self, controller, parent=None):
        super().__init__(parent)
        self.controller = controller
        self.setWindowTitle("SMUK • Manual calibration")
        self.resize(860, 760)
        self.setModal(True)
        self.pending = None
        self.queue = deque()
        self.prepared = False
        self.preparing = False
        self.session_started = False
        self.fitted = False
        self.dirty = False
        self.closing = False
        self.capture_samples = 256  # compatible with older firmware
        self.report = deque(maxlen=4000)
        layout = QVBoxLayout(self)
        intro = QLabel(
            "1. Select the target and fixed ranges, then prepare. Both autoranges will be OFF.\n"
            "2. Wait for settled readings and begin captures. Set each physical reference manually,\n"
            "   wait for it to stabilize, then enter the actual reference measured by your bench instrument.\n"
            "3. Capture 2–8 well-spaced points (preferably zero and both polarities), fit, and review residuals.\n"
            "4. Save explicitly to Flash, then verify against independent references.\n"
            "Voltage / CALBUS references are in V; current references are in A (100 µA = 0.0001 A).\n"
            "The app does not switch CALBUS. Fits remain staged until saved; live readings use active coefficients."
        )
        intro.setWordWrap(True)
        layout.addWidget(intro)
        settings = QHBoxLayout()
        self.target = QComboBox()
        self.target.addItems(("V", "I", "BUS"))
        self.vrange = QComboBox()
        self.vrange.addItems(VOLTAGE_RANGES)
        self.irange = QComboBox()
        self.irange.addItems(CURRENT_RANGES)
        self.impedance = QComboBox()
        self.impedance.addItems(("10M", "HIGHZ"))
        status = controller.state.status
        for widget, key in ((self.vrange, "V"), (self.irange, "I"), (self.impedance, "impedance")):
            widget.setCurrentText(status.get(key, widget.currentText()))
        for title, widget in (("Target", self.target), ("Voltage", self.vrange), ("Current", self.irange), ("Input", self.impedance)):
            settings.addWidget(QLabel(title))
            settings.addWidget(widget)
        layout.addLayout(settings)
        row = QHBoxLayout()
        self.prepare = QPushButton("Prepare fixed ranges")
        self.begin = QPushButton("Begin / restart captures")
        row.addWidget(self.prepare)
        row.addWidget(self.begin)
        layout.addLayout(row)
        self.status = QLabel()
        self.status.setWordWrap(True)
        layout.addWidget(self.status)
        capture_row = QHBoxLayout()
        capture_row.addWidget(QLabel("Actual reference (V or A):"))
        self.reference = QLineEdit()
        self.reference.setPlaceholderText("e.g. -5.00012 or 0.0001; scientific notation accepted")
        capture_row.addWidget(self.reference)
        self.capture = QPushButton("Capture reference")
        capture_row.addWidget(self.capture)
        layout.addLayout(capture_row)
        self.points = QTableWidget(0, 4)
        self.points.setHorizontalHeaderLabels(("Nominal (V/A)", "Reference (V/A)", "RMS (V/A)", "Drift (V/A)"))
        self.points.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.points.horizontalHeader().setStretchLastSection(True)
        layout.addWidget(self.points)
        fit_row = QHBoxLayout()
        self.fit = QPushButton("Fit staged coefficients")
        self.save = QPushButton("Save staged fits to Flash…")
        self.discard = QPushButton("Discard all staged fits…")
        for widget in (self.fit, self.save, self.discard):
            fit_row.addWidget(widget)
        layout.addLayout(fit_row)
        self.details = QPlainTextEdit()
        self.details.setReadOnly(True)
        self.details.setMaximumBlockCount(2000)
        self.details.setPlaceholderText("Capture quality, staged coefficients, fit spans and residuals appear here.")
        layout.addWidget(self.details)
        footer = QHBoxLayout()
        export = QPushButton("Export calibration report…")
        close = QPushButton("Close")
        footer.addWidget(export)
        footer.addStretch()
        footer.addWidget(close)
        layout.addLayout(footer)
        self.prepare.clicked.connect(self._prepare)
        self.begin.clicked.connect(lambda: self._request(f"CAL:BEGIN {self.target.currentText()}"))
        self.capture.clicked.connect(self._capture)
        self.fit.clicked.connect(lambda: self._request("CAL:FIT"))
        self.save.clicked.connect(self._save)
        self.discard.clicked.connect(self._discard)
        export.clicked.connect(self._export)
        close.clicked.connect(self.close)
        controller.reply_completed.connect(self._reply)
        controller.changed.connect(self._update)
        self._request("CAL:SHOW?")
        self._update()

    def _request(self, *commands):
        if self.pending or not self.controller.state.connected:
            return
        if "CAL:FIT" in commands:
            self.fitted = False
        self.queue.extend(commands)
        self._next()

    def _next(self):
        if self.queue:
            self.pending = self.queue.popleft()
            self._record("TX " + self.pending)
            self.controller.send(self.pending, calibration=True)
        else:
            self.pending = None
        self._update()

    def _record(self, text):
        self.report.append(text)
        self.details.appendPlainText(text)

    def _prepare(self):
        if self.prepared:
            self.prepared = self.session_started = self.fitted = False
            self._request("CAL:ABORT")
            return
        self.preparing = True
        self.prepared = self.session_started = self.fitted = False
        self.points.setRowCount(0)
        self._request("CAL:ABORT", "AUTORANGE:I OFF", "AUTORANGE:V OFF",
                      f"RANGE:I {self.irange.currentText()}", f"RANGE:V {self.vrange.currentText()}",
                      f"IMPEDANCE {self.impedance.currentText()}", "STATUS?", "MEAS?", "CAL:SHOW?")

    def _capture(self):
        try:
            command = validate_command("CAL:CAPTURE " + self.reference.text(), calibration=True)
        except ValueError as error:
            self._record(str(error))
            return
        self.fitted = False
        self._request(command, "CAL:POINTS?")

    def _save(self):
        if QMessageBox.question(self, "Persist calibration", "Save ALL staged coefficients to instrument Flash?\n"
                                "This replaces the active calibration. Verify the fit and references first.") == QMessageBox.StandardButton.Yes:
            self._request("CAL:SAVE", "CAL:SHOW?", "STATUS?", "MEAS?")

    def _discard(self):
        if QMessageBox.question(self, "Discard staged calibration", "Discard ALL unsaved fits and captured points?\n"
                                "Previously saved calibration is retained.") == QMessageBox.StandardButton.Yes:
            self._request("CAL:RESET", "CAL:SHOW?")

    def _reply(self, reply):
        if reply.command != self.pending:
            return
        for response in reply.responses:
            self._record(response.text)
        if reply.error:
            self._record(reply.error)
            self.queue.clear()
            self.pending = None
            self.preparing = False
            self._update()
            return
        command = reply.command
        if command.startswith("CAL:BEGIN "):
            self.session_started = True
            self.fitted = False
            self.points.setRowCount(0)
        elif command == "CAL:ABORT":
            self.session_started = False
            self.points.setRowCount(0)
            if self.closing:
                self._release()
                self.accept()
                return
        elif command == "CAL:RESET":
            self.session_started = self.fitted = self.dirty = False
            self.points.setRowCount(0)
        elif command == "CAL:POINTS?":
            count = int(reply.responses[-1].fields["points"])
            self.points.setRowCount(count)
            for response in reply.responses:
                if response.kind != "POINT":
                    continue
                parts = response.text.split()
                quality = parts[1] == "QUALITY"
                index = int(parts[2] if quality else parts[1])
                if not 0 <= index < count <= 8:
                    raise ValueError("Invalid calibration point index")
                keys = ("rms_nominal", "drift_nominal") if quality else ("nominal", "reference")
                for col, key in enumerate(keys, 2 if quality else 0):
                    self.points.setItem(index, col, QTableWidgetItem(response.fields[key]))
        elif command == "CAL:FIT":
            self.fitted = self.dirty = True
        elif command == "CAL:SAVE":
            self.dirty = False
            self._record("Saved. Check independent references with MEAS; staged fits are now active.")
        elif command == "CAL:SHOW?":
            self.dirty = reply.responses[0].fields.get("staged_dirty") == "1"
            limits = reply.responses[-1].fields
            self.capture_samples = int(limits.get("capture_samples", "256"))
            self.capture.setText(f"Capture {self.capture_samples} samples")
            if self.preparing and not self.queue:
                self.prepared = True
                self.preparing = False
        self._next()

    def _fixed_and_live(self):
        s = self.controller.state
        return (self.prepared and s.live and s.status.get("autorange") == "0"
                and s.status.get("autorange_v") == "0"
                and s.status.get("I") == self.irange.currentText()
                and s.status.get("V") == self.vrange.currentText()
                and s.measurement.current_range == self.irange.currentText()
                and s.measurement.voltage_range == self.vrange.currentText()
                and s.status.get("impedance") == self.impedance.currentText()
                and s.status.get("impedance_requested") == self.impedance.currentText()
                and (self.target.currentText() != "BUS" or s.measurement.quality.get("BUS_clip") == "0"))

    def _update(self):
        connected = self.controller.state.connected
        idle = connected and self.pending is None
        stable = self._fixed_and_live()
        for widget in (self.target, self.vrange, self.irange, self.impedance):
            widget.setEnabled(idle and not self.prepared)
        self.prepare.setEnabled(idle and self.controller.state.healthy)
        self.prepare.setText("Change target / ranges" if self.prepared else "Prepare fixed ranges")
        self.begin.setEnabled(idle and stable)
        self.capture.setEnabled(idle and stable and self.session_started and self.points.rowCount() < 8)
        self.fit.setEnabled(idle and stable and self.session_started and self.points.rowCount() >= 2)
        self.save.setEnabled(idle and stable and self.dirty and (self.fitted or not self.session_started))
        self.discard.setEnabled(idle and self.controller.state.healthy)
        self.reference.setEnabled(idle and self.session_started)
        if not connected:
            self.session_started = self.prepared = False
            message = "Disconnected / restarted. Close and reconnect; no capture session is resumed automatically."
        elif self.pending:
            message = f"Waiting for {self.pending}…"
        elif not self.prepared:
            message = "Select the target and ranges, then prepare."
        elif not stable:
            message = "Waiting for fresh, settled measurements in the selected fixed ranges."
        else:
            message = f"Ready • target {self.target.currentText()} • I {self.irange.currentText()} • V {self.vrange.currentText()} • input {self.impedance.currentText()}"
        self.status.setText(message)

    def _export(self):
        path, _ = QFileDialog.getSaveFileName(self, "Export calibration report", "smuk-calibration.txt", "Text files (*.txt)")
        if path:
            try:
                Path(path).write_text(f"SMUK calibration report {datetime.now().isoformat()}\n"
                                      f"Port: {self.controller.state.port}\n" + "\n".join(self.report) + "\n")
            except OSError as error:
                self._record(f"Report export failed: {error}")

    def _release(self):
        self.controller.reply_completed.disconnect(self._reply)
        self.controller.changed.disconnect(self._update)
        self.controller.calibration_active = False
        self.controller.changed.emit()

    def reject(self):
        self.close()

    def closeEvent(self, event):
        if not self.controller.state.connected:
            self._release()
            event.accept()
        elif self.pending:
            self._record("Wait for the current operation to finish before closing.")
            event.ignore()
        else:
            event.ignore()
            self.closing = True
            self._request("CAL:ABORT")
