from PySide6.QtCore import Signal
from PySide6.QtWidgets import QFrame, QVBoxLayout, QGridLayout, QLabel, QPushButton
from .measurement_display import format_value


class StatisticsPanel(QFrame):
    reset_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("statisticsPanel")
        self.setMinimumWidth(235)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(20, 22, 20, 22)
        title = QLabel("OBSERVED STATISTICS")
        title.setObjectName("sectionLabel")
        layout.addWidget(title)
        self.values = {}
        for channel in ("Power", "Voltage", "Current"):
            layout.addSpacing(18)
            heading = QLabel(channel.upper())
            heading.setObjectName("sectionLabel")
            layout.addWidget(heading)
            grid = QGridLayout()
            for row, statistic in enumerate(("Min", "Avg", "Max")):
                grid.addWidget(QLabel(statistic), row, 0)
                value = QLabel("—")
                value.setObjectName("statisticValue")
                grid.addWidget(value, row, 1)
                self.values[channel, statistic] = value
            layout.addLayout(grid)
        layout.addStretch()
        self.count = QLabel("0 observations")
        layout.addWidget(self.count)
        button = QPushButton("↺  Reset statistics")
        button.clicked.connect(self.reset_requested.emit)
        layout.addWidget(button)
        note = QLabel("Distinct valid replies at ≈5 Hz.\nPower = averaged V × averaged I.")
        note.setObjectName("muted")
        note.setWordWrap(True)
        layout.addWidget(note)

    def update_statistics(self, statistics):
        for channel, summary, unit in (("Power", statistics.power, "W"), ("Voltage", statistics.voltage, "V"), ("Current", statistics.current, "A")):
            for statistic, value in (("Min", summary.minimum), ("Avg", summary.mean), ("Max", summary.maximum)):
                self.values[channel, statistic].setText(format_value(value if summary.count else None, unit))
        self.count.setText(f"{statistics.voltage.count:,} observations")
