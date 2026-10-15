import math
import time
from PySide6.QtCore import Qt, QRectF
from PySide6.QtGui import QPainter, QPalette
from PySide6.QtWidgets import QFrame, QLabel, QVBoxLayout, QHBoxLayout, QPushButton


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
        self.voltage_range = QPushButton("SET — V")
        self.voltage_range.setObjectName("rangeBadge")
        source_row = QHBoxLayout()
        source_row.addWidget(self.voltage_range, 1)
        self.output = QPushButton("OUTPUT UNKNOWN")
        self.output.setObjectName("outputButton")
        source_row.addWidget(self.output)
        layout.addLayout(source_row)
        self.source_detail = QLabel("FV source status unavailable")
        self.source_detail.setWordWrap(True)
        self.source_detail.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(self.source_detail)
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
            self.current_range.setText(f"RANGE  {m.current_range}")
        source = state.source if state.source_known else None
        self.voltage_range.setText(f"SET {source.requested:.6f} V" if source else "SET — V")
        self.voltage_range.setEnabled(bool(source and source.state in (1, 2, 3) and not state.source_pending))
        self.output.setProperty("sourceState", str(source.state) if source else "unknown")
        if source:
            labels = {0: "OUTPUT UNAVAILABLE", 1: "OUTPUT ON", 2: "STARTING… / OUTPUT OFF", 3: "OUTPUT OFF", 4: "OUTPUT TRIPPED / RESET", 5: "ZERO LEARNING / CANCEL"}
            self.output.setText("OUTPUT OFF" if state.source_pending else labels[source.state])
            self.output.setEnabled(source.state in (2, 3, 4, 5) or state.source_pending or (source.state == 1 and not state.source_prerequisites))
            detail = "Zero command" if source.state == 1 else "FV"
            if source.state in (2, 3) and abs(source.requested - source.applied) > 0.00001:
                detail += f" • target {source.requested:.6f} V • ramping (command {source.applied:.6f} V)"
            if source.regulation is not None:
                detail += f" • correction {'ON' if source.regulation else 'OFF'} ({source.correction:+.6f} V)"
                detail += " • LIMITED" if source.limited else ""
                detail += " • zero learned" if source.zero_valid else " • zero not learned"
            if source.pa_always_enabled:
                detail += " • PA always enabled"
            if not source.relay_available:
                detail += " • no isolation relay"
            if source.state == 1 and state.source_prerequisites:
                detail += "\n" + "; ".join(state.source_prerequisites)
            self.source_detail.setText(detail)
            self.mode.setText("FV" if source.state in (2, 3) else "MEASURE")
        else:
            self.output.setText("OUTPUT UNKNOWN")
            # A stale known source may still be commanded OFF; never ON.
            self.output.setEnabled(bool(state.connected and state.source and state.source.state != 0))
            self.source_detail.setText("FV source status unavailable; output state unknown")
            self.mode.setText("MEASURE")
        self.output.setToolTip("Request zero output; PA remains enabled without hardware ENABLE control" if source and source.pa_always_enabled else "Control confirmed FV output")
        self.output.style().unpolish(self.output)
        self.output.style().polish(self.output)
        if not state.connected:
            quality = "DISCONNECTED • readings retained" if m else "Connect an instrument to begin"
        elif state.status.get("faults", "0") != "0":
            quality = f"FAULT {state.status['faults']} • reboot required"
        elif state.live:
            quality = f"LIVE • settled • {m.integration_ms:g} ms • window {m.populated * m.group_samples}/{m.window * m.group_samples} samples • BUS {m.calbus:.6f} V"
        elif m and any(m.quality.get(k) == "1" for k in ("I_clip", "V_clip", "I_overload", "V_overload")):
            quality = "OVERLOAD / ADC CLIPPING • reading invalid"
        elif m and (time.monotonic() - m.received_at >= 1.2 or m.quality.get("fresh") == "0"):
            quality = "STALE • readings retained • waiting for fresh data"
        else:
            quality = f"WAITING / SETTLING • window {m.populated * m.group_samples}/{m.window * m.group_samples} samples" if m else "Waiting for measurements"
        self.quality.setText(quality)
