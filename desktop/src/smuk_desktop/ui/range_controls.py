from PySide6.QtCore import QSignalBlocker, Signal
from PySide6.QtWidgets import QFrame, QVBoxLayout, QLabel, QComboBox
from ..protocol.commands import CURRENT_RANGES, VOLTAGE_RANGES, INTEGRATIONS


class RangeControls(QFrame):
    command = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("rangeControls")
        self.setFixedWidth(185)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 24, 16, 24)
        self.voltage = self._selector(layout, "VOLTAGE RANGE", ("Auto", *VOLTAGE_RANGES))
        self.voltage.activated.connect(lambda _: self._range("V", self.voltage.currentText()))
        self.voltage_actual = QLabel("Selected: —")
        layout.addWidget(self.voltage_actual)
        layout.addSpacing(30)
        self.current = self._selector(layout, "CURRENT RANGE", ("Auto", *CURRENT_RANGES))
        self.current.activated.connect(lambda _: self._range("I", self.current.currentText()))
        self.current_actual = QLabel("Selected: —")
        layout.addWidget(self.current_actual)
        layout.addStretch()
        self.impedance = self._selector(layout, "INPUT IMPEDANCE", ("10M", "HIGHZ"))
        self.impedance.activated.connect(lambda _: self.command.emit(f"IMPEDANCE {self.impedance.currentText()}"))
        self.integration = self._selector(layout, "INTEGRATION", INTEGRATIONS)
        self.integration.setCurrentText("8MS")
        self.integration.activated.connect(lambda _: self.command.emit(f"INTEGRATION {self.integration.currentText()}"))
        note = QLabel("Measurement only\nPA unavailable")
        note.setObjectName("muted")
        layout.addSpacing(20)
        layout.addWidget(note)
        self.setEnabled(False)

    @staticmethod
    def _selector(layout, text, choices):
        label = QLabel(text)
        label.setObjectName("sectionLabel")
        layout.addWidget(label)
        combo = QComboBox()
        combo.addItems(choices)
        layout.addWidget(combo)
        return combo

    def _range(self, channel, value):
        self.command.emit(f"AUTORANGE:{channel} ON" if value == "Auto" else f"RANGE:{channel} {value}")

    @staticmethod
    def _confirm(combo, value):
        with QSignalBlocker(combo):
            combo.setCurrentText(value)

    def update_state(self, state):
        self.setEnabled(state.healthy)
        s = state.status
        if not s:
            return
        self._confirm(self.voltage, "Auto" if s.get("autorange_v") == "1" else s.get("V", "Auto"))
        self._confirm(self.current, "Auto" if s.get("autorange") == "1" else s.get("I", "Auto"))
        self.voltage_actual.setText(f"Selected: {s.get('V', '—')}")
        self.current_actual.setText(f"Selected: {s.get('I', '—')}")
        self._confirm(self.impedance, s.get("impedance_requested", "10M"))
        m = state.measurement
        if m:
            self._confirm(self.integration, {4: "1MS", 32: "8MS", 80: "20MS", 200: "50MS", 400: "100MS"}.get(m.window, "8MS"))
