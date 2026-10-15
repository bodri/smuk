from PySide6.QtWidgets import QDialog, QVBoxLayout, QLabel, QDoubleSpinBox, QDialogButtonBox, QPushButton, QMessageBox


class SourceDialog(QDialog):
    def __init__(self, controller, parent=None):
        super().__init__(parent)
        self.controller = controller
        self.setWindowTitle("FV voltage setpoint")
        self.setMinimumWidth(420)
        source = controller.state.source
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(f"Supported voltage: {source.minimum:g} to {source.maximum:g} V"))
        self.voltage = QDoubleSpinBox()
        self.voltage.setDecimals(6)
        self.voltage.setRange(source.minimum, source.maximum)
        self.voltage.setSingleStep(0.01)
        self.voltage.setSuffix(" V")
        self.voltage.setValue(source.requested)
        layout.addWidget(self.voltage)
        note = QLabel(f"Slew limit: {source.slew:g} V/s. Applying changes the target while output is active.")
        note.setWordWrap(True)
        layout.addWidget(note)
        self.feedback = QLabel()
        self.feedback.setWordWrap(True)
        layout.addWidget(self.feedback)
        self.zero = QPushButton("Learn source zero…")
        self.zero.clicked.connect(self._zero)
        layout.addWidget(self.zero)
        self.regulation = QPushButton()
        self.regulation.clicked.connect(self._regulation)
        layout.addWidget(self.regulation)
        layout.addWidget(QLabel("Bench correction has no hardware compliance feedback.\nLearning is RAM-only and separate for each voltage range."))
        self.buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Apply | QDialogButtonBox.StandardButton.Cancel)
        self.buttons.button(QDialogButtonBox.StandardButton.Apply).clicked.connect(self._apply)
        self.buttons.rejected.connect(self.reject)
        layout.addWidget(self.buttons)
        controller.changed.connect(self._update)
        self._update()

    def _update(self):
        state = self.controller.state
        source = state.source
        allowed = state.source_known and source.state in (1, 2, 3) and not state.source_pending and not self.controller.calibration_active
        self.buttons.button(QDialogButtonBox.StandardButton.Apply).setEnabled(allowed)
        off = bool(allowed and source.state == 1 and source.regulation is not None)
        self.zero.setEnabled(off and not state.source_prerequisites)
        self.regulation.setEnabled(off)
        self.regulation.setText("Disable voltage correction" if source and source.regulation else "Enable bench voltage correction…")
        self.feedback.setText("Zero learning in progress; OUTPUT OFF cancels." if source and source.state == 5 else
                              "Zero learned for this range." if source and source.zero_valid else "Zero has not been learned for this range.")
        if not state.connected:
            self.reject()

    def _apply(self):
        if not self.buttons.button(QDialogButtonBox.StandardButton.Apply).isEnabled():
            return
        self.controller.send(f"SOURCE:V {self.voltage.value():.6f}")
        if self.controller.state.source_pending:
            self.accept()

    def _zero(self):
        answer = QMessageBox.question(self, "Learn source zero", "Disconnect external voltage sources and leave the output unloaded.\nThe PA remains connected and may adjust its output near zero.\nLearn zero for the selected range?")
        if answer == QMessageBox.StandardButton.Yes:
            self.controller.send("SOURCE:ZERO")

    def _regulation(self):
        source = self.controller.state.source
        if not source.regulation:
            answer = QMessageBox.question(self, "Bench voltage correction", "Hardware current-compliance feedback is not connected.\nUse only within the independently configured analog current limit.\nEnable bounded voltage correction?")
            if answer != QMessageBox.StandardButton.Yes:
                return
        self.controller.send("SOURCE:REGULATION OFF" if source.regulation else "SOURCE:REGULATION ON")
