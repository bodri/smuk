from PySide6.QtWidgets import QDialog, QVBoxLayout, QLabel, QDoubleSpinBox, QDialogButtonBox


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
        if not state.connected:
            self.reject()

    def _apply(self):
        if not self.buttons.button(QDialogButtonBox.StandardButton.Apply).isEnabled():
            return
        self.controller.send(f"SOURCE:V {self.voltage.value():.6f}")
        if self.controller.state.source_pending:
            self.accept()
