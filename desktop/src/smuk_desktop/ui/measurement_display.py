import math
import time
from PySide6.QtCore import Qt, QRectF
from PySide6.QtGui import QPainter, QPalette
from PySide6.QtWidgets import QFrame, QLabel, QVBoxLayout, QHBoxLayout


def format_value(value: float | None, unit: str, decimals: int = 5) -> str:
    if value is None or not math.isfinite(value):
        return f"— {unit}"
    scale, prefix = 1.0, ""
    if unit == "A" and 0 < abs(value) < 1:
        scale, prefix = 1e3, "m"
    elif unit == "W" and 0 < abs(value) < 1:
        scale, prefix = (1e6, "µ") if abs(value) < 1e-3 else (1e3, "m")
    number = f"{value * scale: .{decimals}f}"
    if unit in ("A", "V") and decimals > 3:
        integer, fraction = number.split(".")
        fraction = " ".join(fraction[i:i + 3] for i in range(0, len(fraction), 3))
        number = f"{integer}.{fraction}"
    return f"{number} {prefix}{unit}"


class DecimalReading(QLabel):
    """Keep the decimal point at the same position for both measurement rows."""

    def paintEvent(self, event):
        text = self.text()
        if "." not in text:
            super().paintEvent(event)
            return
        metrics = self.fontMetrics()
        rect = self.contentsRect()
        # Reserve identical space for the sign, digits, fractional part and unit.
        template = "-000.000 00 µA"
        decimal_x = (rect.x() + rect.width() / 2
                     - metrics.horizontalAdvance(template) / 2
                     + metrics.horizontalAdvance("-000"))
        x = decimal_x - metrics.horizontalAdvance(text.split(".", 1)[0])
        painter = QPainter(self)
        painter.setPen(self.palette().color(QPalette.ColorRole.WindowText))
        painter.drawText(QRectF(x, rect.y(), rect.right() - x + 1, rect.height()),
                         Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter,
                         text)


class MeasurementDisplay(QFrame):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("measurementDisplay")
        layout = QVBoxLayout(self)
        layout.setContentsMargins(28, 20, 28, 24)
        power_row = QHBoxLayout()
        self.power = QLabel("— W")
        self.power.setObjectName("powerReading")
        power_row.addWidget(self.power)
        self.mode = QLabel("MEASURE")
        self.mode.setObjectName("modeBadge")
        power_row.addStretch()
        power_row.addWidget(self.mode)
        layout.addLayout(power_row)
        layout.addStretch()
        self.voltage = self._reading(layout, "VOLTAGE", "— V")
        self.voltage_range = QLabel("Range —")
        self.voltage_range.setObjectName("rangeBadge")
        self.voltage_range.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(self.voltage_range)
        layout.addSpacing(30)
        self.current = self._reading(layout, "CURRENT", "— A")
        self.current_range = QLabel("Range —")
        self.current_range.setObjectName("rangeBadge")
        self.current_range.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(self.current_range)
        layout.addStretch()
        self.quality = QLabel("Connect an instrument to begin")
        self.quality.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.quality.setWordWrap(True)
        layout.addWidget(self.quality)

    @staticmethod
    def _reading(layout, title, placeholder):
        heading = QLabel(title)
        heading.setObjectName("sectionLabel")
        layout.addWidget(heading)
        label = DecimalReading(placeholder)
        label.setObjectName("largeReading")
        label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        label.setMinimumWidth(430)
        layout.addWidget(label)
        return label

    def update_state(self, state):
        m = state.measurement
        self.voltage.setText(format_value(m.voltage if m else None, "V"))
        self.current.setText(format_value(m.current if m else None, "A"))
        self.power.setText(format_value(m.power if m else None, "W", 6))
        for label in (self.voltage, self.current, self.power):
            label.setProperty("valid", state.live)
            label.style().unpolish(label)
            label.style().polish(label)
        if m:
            self.voltage_range.setText(f"RANGE  {m.voltage_range}")
            self.current_range.setText(f"RANGE  {m.current_range}")
        if not state.connected:
            quality = "DISCONNECTED • readings retained" if m else "Connect an instrument to begin"
        elif state.status.get("faults", "0") != "0":
            quality = f"FAULT {state.status['faults']} • reboot required"
        elif state.live:
            quality = f"LIVE • settled • {m.integration_ms} ms • window {m.populated * m.group_samples}/{m.window * m.group_samples} samples • BUS {m.calbus:.6f} V"
        elif m and any(m.quality.get(k) == "1" for k in ("I_clip", "V_clip", "I_overload", "V_overload")):
            quality = "OVERLOAD / ADC CLIPPING • reading invalid"
        elif m and (time.monotonic() - m.received_at >= 1.2 or m.quality.get("fresh") == "0"):
            quality = "STALE • readings retained • waiting for fresh data"
        else:
            quality = f"WAITING / SETTLING • window {m.populated * m.group_samples}/{m.window * m.group_samples} samples" if m else "Waiting for measurements"
        self.quality.setText(quality)
