from collections import deque
import time
from PySide6.QtCore import QObject, QTimer, Signal
from ..protocol.commands import validate_command, changes_measurement
from ..protocol.parser import Transaction, parse_line
from .connection import SerialConnection
from .state import InstrumentState, Measurement
from .statistics import Statistics


class Controller(QObject):
    changed = Signal()
    log = Signal(str, str)
    error = Signal(str)
    reply_completed = Signal(object)

    def __init__(self, connection=None, parent=None):
        super().__init__(parent)
        self.connection = connection or SerialConnection(self)
        self.state = InstrumentState()
        self.statistics = Statistics()
        self.user_queue: deque[str] = deque()
        self.poll_queue: deque[str] = deque()
        self.transaction: Transaction | None = None
        self.handshaking = False
        self.last_status = self.last_acq = 0.0
        self.cal_sequence: str | None = None
        self.calibration_active = False
        self.calibration_points = 0
        self.calibration_records = ()
        self.timeout = QTimer(self)
        self.timeout.setSingleShot(True)
        self.timeout.timeout.connect(self._timed_out)
        self.poll_timer = QTimer(self)
        self.poll_timer.setInterval(200)
        self.poll_timer.timeout.connect(self._poll)
        self.connection.opened.connect(self._opened)
        self.connection.closed.connect(self._closed)
        self.connection.failed.connect(self._failed)
        self.connection.line_received.connect(self._line)

    def connect_port(self, path: str):
        self.disconnect()
        self.state = InstrumentState(port=path, message="Connecting…")
        self.changed.emit()
        self.connection.open(path)

    def disconnect(self):
        self._stop()
        self.connection.close()
        self.state.connected = False
        self.state.message = "Disconnected"
        self.statistics.reset()
        self.changed.emit()

    def _stop(self):
        self.timeout.stop()
        self.poll_timer.stop()
        self.transaction = None
        self.handshaking = False
        self.user_queue.clear()
        self.poll_queue.clear()
        self.calibration_active = False
        self.calibration_points = 0

    def _opened(self, path):
        self.state.port = path
        self.handshaking = True
        self.changed.emit()
        self.user_queue.append("PING")
        self._pump()

    def _closed(self):
        self._stop()
        self.state.connected = False
        self.state.message = "Disconnected"
        self.statistics.reset()
        self.changed.emit()

    def _failed(self, message):
        self.disconnect()
        self.state.message = message
        self.log.emit("!", message)
        self.error.emit(message)
        self.changed.emit()

    def _timed_out(self):
        command = self.transaction.command if self.transaction else "command"
        self._failed(f"Timeout waiting for {command}. Reconnect to resynchronize.")

    def reserve_calibration(self):
        if not self.state.healthy or self.user_queue or self.calibration_active:
            return False
        self.calibration_active = True
        self.poll_queue.clear()
        self.changed.emit()
        return True

    def send(self, text: str, *, calibration: bool = False):
        if self.calibration_active and not calibration:
            self.error.emit("Finish or close manual calibration before sending other commands")
            return
        if calibration and not self.calibration_active:
            self.error.emit("Manual calibration session is no longer connected")
            return
        if not self.state.connected:
            self.error.emit("Connect to the instrument first")
            return
        try:
            command = validate_command(text, calibration=calibration)
        except ValueError as error:
            self.error.emit(str(error))
            return
        if len(self.user_queue) >= 16:
            self.error.emit("Command queue is full; wait for the instrument")
            return
        self.user_queue.append(command)
        if changes_measurement(command):
            self.state.pending_change = True
            self.changed.emit()
        self._pump()

    def _enqueue_poll(self, command: str):
        if command not in self.poll_queue and (not self.transaction or self.transaction.command != command):
            self.poll_queue.append(command)

    def _pump(self):
        if self.transaction or not (self.state.connected or self.handshaking):
            return
        queue = self.user_queue or self.poll_queue
        if not queue:
            return
        command = queue.popleft()
        self.transaction = Transaction(command, self.calibration_points)
        self.timeout.start(3000 if self.handshaking or command.startswith("CAL:CAPTURE ") else 2000)
        self.log.emit("TX", command)
        self.connection.write(command)

    def _poll(self):
        now = time.monotonic()
        if now - self.last_status >= 1:
            self._enqueue_poll("STATUS?")
            self.last_status = now
        if now - self.last_acq >= 2:
            self._enqueue_poll("ACQ?")
            self.last_acq = now
        self._enqueue_poll("MEAS?")
        self._pump()
        self.changed.emit()  # expire retained measurements even without new data

    def _line(self, text: str):
        self.log.emit("RX", text)
        try:
            response = parse_line(text)
            if response.kind == "STARTUP" and self.state.connected:
                self._failed("Instrument restarted. Reconnect before continuing.")
                return
            if response.kind == "SMU" and "fault bits=" in text:
                self.state.message = "Instrument fault; awaiting status"
                bits = int(response.fields["bits"], 0)
                self.state.status["faults"] = str(int(self.state.status.get("faults", "0")) | bits)
                self.state.status["state"] = "8"
                self.state.status["valid"] = "0"
                self.changed.emit()
            if not self.transaction:
                return  # unsolicited logging
            reply = self.transaction.accept(response)
            if reply is None:
                return
            self.timeout.stop()
            self.transaction = None
            if reply.error:
                if reply.error.startswith(("ERR TX overflow", "ERR RX overflow", "ERR RX loss")):
                    self._failed(reply.error + "; reconnect to resynchronize")
                    return
                if self.handshaking:
                    self._failed(reply.error)
                    return
                self.state.message = reply.error
                self.error.emit(reply.error)
                self._enqueue_poll("STATUS?")
            else:
                self._apply(reply)
            self.reply_completed.emit(reply)
            self.changed.emit()
            QTimer.singleShot(0, self._pump)  # consume this RX batch before sending
        except (ValueError, KeyError, TypeError) as error:
            self._failed(f"Invalid firmware response: {error}")

    def _apply(self, reply):
        records = {r.kind: r for r in reply.responses}
        if reply.command == "PING" and self.handshaking:
            self.handshaking = False
            self.state.connected = True
            self.state.message = "Connected"
            self.statistics.reset()
            self.cal_sequence = None
            self.user_queue.extend(("ECHO OFF", "STATUS?", "CAL:SHOW?", "ACQ?", "MEAS?"))
            self.last_status = self.last_acq = time.monotonic()
            self.poll_timer.start()
        elif reply.command == "STATUS?":
            status = records["STATUS"].fields
            for key in ("state", "faults", "valid", "autorange", "autorange_v", "frames"):
                int(status[key])
            self.state.status = status
            self.state.watchdog = records["WATCHDOG"].fields
            self.state.message = "Connected" if status["faults"] == "0" else f"Instrument fault: {status['faults']}"
        elif reply.command == "ACQ?":
            self.state.acquisition = records["ACQ"].fields
        elif reply.command == "MEAS?":
            measurement = Measurement.from_responses(records["MEAS"], records["QUALITY"])
            self.state.measurement = measurement
            if not any(changes_measurement(cmd) for cmd in self.user_queue):
                self.state.pending_change = False
            if self.state.healthy and not self.state.pending_change:
                self.statistics.observe(measurement)
        elif reply.command == "CAL:SHOW?":
            self.calibration_records = reply.responses
            sequence = records["CAL"].fields["sequence"]
            if self.cal_sequence is not None and self.cal_sequence != sequence:
                self.statistics.reset()
            self.cal_sequence = sequence
        elif reply.command.startswith("CAL:BEGIN ") or reply.command in ("CAL:ABORT", "CAL:RESET"):
            self.calibration_points = 0
        elif reply.command.startswith("CAL:CAPTURE "):
            self.calibration_points += 1
        elif reply.command == "CAL:POINTS?":
            self.calibration_points = int(reply.responses[-1].fields["points"])
        elif reply.command == "CAL:SAVE":
            self.statistics.reset()
            if not self.calibration_active:
                self._enqueue_poll("CAL:SHOW?")
                self._enqueue_poll("STATUS?")
                self._enqueue_poll("MEAS?")
        elif reply.command.startswith("ECHO "):
            return
        elif reply.command not in ("PING", "RAW?", "HELP", "CAL:POINTS?"):
            self.state.message = reply.responses[-1].text
            if not self.calibration_active:
                self._enqueue_poll("STATUS?")
                self._enqueue_poll("MEAS?")

    def reset_statistics(self):
        self.statistics.reset()
        if self.state.measurement:
            self.statistics.last_sample = self.state.measurement.samples
        self.changed.emit()
