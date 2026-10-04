from datetime import datetime
from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QLabel, QComboBox, QPushButton, QFrame
from .measurement_display import MeasurementDisplay
from .range_controls import RangeControls
from .statistics_panel import StatisticsPanel
from .console_panel import ConsolePanel
from .calibration_dialog import CalibrationDialog


class MainWindow(QMainWindow):
    def __init__(self, controller):
        super().__init__()
        self.controller = controller
        self.setWindowTitle("SMUK • Measurement Console")
        self.resize(1200, 760)
        self.setMinimumSize(1040, 680)
        root = QWidget()
        self.setCentralWidget(root)
        layout = QVBoxLayout(root)
        layout.setContentsMargins(24, 20, 24, 20)
        layout.setSpacing(0)
        header = QFrame()
        header.setObjectName("header")
        bar = QHBoxLayout(header)
        title = QLabel("SMUK")
        title.setObjectName("brand")
        bar.addWidget(title)
        badge = QLabel("CH 1   /   MEASUREMENT")
        badge.setObjectName("sectionLabel")
        bar.addWidget(badge)
        bar.addStretch()
        self.clock = QLabel()
        bar.addWidget(self.clock)
        layout.addWidget(header)
        accent = QFrame()
        accent.setObjectName("accent")
        accent.setFixedHeight(4)
        layout.addWidget(accent)
        connection_row = QHBoxLayout()
        connection_row.setContentsMargins(0, 16, 0, 16)
        self.ports = QComboBox()
        self.ports.setMinimumWidth(310)
        connection_row.addWidget(self.ports)
        self.refresh = QPushButton("Refresh ports")
        self.refresh.clicked.connect(self._refresh_ports)
        connection_row.addWidget(self.refresh)
        self.connect_button = QPushButton("Connect")
        self.connect_button.setObjectName("connectButton")
        self.connect_button.clicked.connect(self._connect)
        connection_row.addWidget(self.connect_button)
        self.health = QLabel("Disconnected")
        connection_row.addStretch()
        connection_row.addWidget(self.health)
        layout.addLayout(connection_row)
        body = QHBoxLayout()
        body.setSpacing(10)
        self.ranges = RangeControls()
        self.ranges.command.connect(controller.send)
        body.addWidget(self.ranges)
        self.display = MeasurementDisplay()
        body.addWidget(self.display, 1)
        self.statistics = StatisticsPanel()
        self.statistics.reset_requested.connect(controller.reset_statistics)
        body.addWidget(self.statistics)
        layout.addLayout(body, 1)
        footer = QHBoxLayout()
        footer.setContentsMargins(0, 12, 0, 8)
        self.diagnostics = QLabel("115200 / 8N1 • PA unavailable")
        self.diagnostics.setObjectName("muted")
        footer.addWidget(self.diagnostics)
        footer.addStretch()
        self.calibrate = QPushButton("Manual calibration…")
        self.calibrate.clicked.connect(self._calibrate)
        footer.addWidget(self.calibrate)
        console_toggle = QPushButton("Serial console ▾")
        console_toggle.setCheckable(True)
        footer.addWidget(console_toggle)
        layout.addLayout(footer)
        self.console = ConsolePanel()
        self.console.setVisible(False)
        self.console.setMaximumHeight(230)
        self.console.command.connect(controller.send)
        self.console.error.connect(self._show_error)
        console_toggle.toggled.connect(self.console.setVisible)
        layout.addWidget(self.console)
        controller.changed.connect(self._update)
        controller.log.connect(self.console.append)
        controller.error.connect(self._show_error)
        self.clock_timer = QTimer(self)
        self.clock_timer.setInterval(1000)
        self.clock_timer.timeout.connect(self._tick)
        self.clock_timer.start()
        self._tick()
        self._refresh_ports()
        self._update()

    def _tick(self):
        self.clock.setText(datetime.now().strftime("%H:%M:%S"))

    def _refresh_ports(self):
        selected = self.ports.currentData()
        self.ports.clear()
        for path, description in self.controller.connection.available_ports():
            self.ports.addItem(description, path)
        index = self.ports.findData(selected)
        if index >= 0:
            self.ports.setCurrentIndex(index)
        if not self.ports.count():
            self.ports.addItem("No serial ports found", None)

    def _connect(self):
        if self.controller.state.connected or self.controller.handshaking:
            self.controller.disconnect()
        elif path := self.ports.currentData():
            self.controller.connect_port(path)

    def _show_error(self, message):
        self.statusBar().showMessage(message)

    def _calibrate(self):
        if self.controller.reserve_calibration():
            dialog = CalibrationDialog(self.controller, self)
            dialog.exec()

    def _update(self):
        state = self.controller.state
        busy = state.connected or self.controller.handshaking
        self.ports.setEnabled(not busy)
        self.refresh.setEnabled(not busy)
        self.connect_button.setText("Disconnect" if busy else "Connect")
        self.health.setText("● LIVE" if state.live else state.message)
        self.health.setProperty("valid", state.live)
        self.health.style().unpolish(self.health)
        self.health.style().polish(self.health)
        self.ranges.update_state(state)
        if self.controller.calibration_active:
            self.ranges.setEnabled(False)
        self.calibrate.setEnabled(state.healthy and not self.controller.calibration_active and not self.controller.user_queue)
        self.display.update_state(state)
        self.statistics.update_statistics(self.controller.statistics)
        self.console.set_connected(state.connected)
        a = state.acquisition
        if a:
            self.diagnostics.setText(f"CRC {a.get('crc', '—')}  SPI {a.get('spi', '—')}  Gaps {a.get('gaps', '—')}  Busy {a.get('busy', '—')}  •  IWDG {'ON' if state.watchdog.get('active') == '1' else '—'}  •  PA unavailable")
            if "frame_hz" in a:
                self.diagnostics.setText(self.diagnostics.text() + f" • ADC {a['frame_hz']}/{a.get('rate_hz', '—')} SPS")
                if a.get("rate_known") == "1" and a.get("rate_ok") == "0":
                    self.health.setText("ADC RATE MISMATCH • precision invalid")

    def closeEvent(self, event):
        self.controller.disconnect()
        event.accept()
