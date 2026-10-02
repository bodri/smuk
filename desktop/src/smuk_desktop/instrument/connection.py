"""Qt serial adapter. All IO is driven by event-loop signals."""
from PySide6.QtCore import QObject, QIODevice, Signal
from PySide6.QtSerialPort import QSerialPort, QSerialPortInfo
from ..protocol.parser import LineDecoder


class SerialConnection(QObject):
    line_received = Signal(str)
    opened = Signal(str)
    closed = Signal()
    failed = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.port = QSerialPort(self)
        self.port.setReadBufferSize(65536)
        self.decoder = LineDecoder()
        self.port.readyRead.connect(self._read)
        self.port.errorOccurred.connect(self._error)

    @staticmethod
    def available_ports() -> list[tuple[str, str]]:
        return sorted((p.systemLocation(), f"{p.portName()} — {p.description() or 'Serial port'}") for p in QSerialPortInfo.availablePorts())

    def open(self, path: str):
        self.close()
        self.decoder = LineDecoder()
        self.port.setPortName(path)
        self.port.setBaudRate(115200)
        self.port.setDataBits(QSerialPort.DataBits.Data8)
        self.port.setParity(QSerialPort.Parity.NoParity)
        self.port.setStopBits(QSerialPort.StopBits.OneStop)
        self.port.setFlowControl(QSerialPort.FlowControl.NoFlowControl)
        if not self.port.open(QIODevice.OpenModeFlag.ReadWrite):
            self.failed.emit(self.port.errorString())
            return
        self.opened.emit(path)

    def close(self):
        was_open = self.port.isOpen()
        if was_open:
            self.port.close()
        self.decoder = LineDecoder()
        if was_open:
            self.closed.emit()

    def write(self, text: str):
        data = (text + "\r\n").encode("ascii")
        if not self.port.isOpen() or self.port.write(data) != len(data):
            self.failed.emit("Serial write failed")

    def _read(self):
        try:
            lines = self.decoder.feed(bytes(self.port.readAll()))
        except ValueError as error:
            self.failed.emit(str(error))
            return
        for line in lines:
            if not self.port.isOpen():
                break
            self.line_received.emit(line)

    def _error(self, error):
        if error != QSerialPort.SerialPortError.NoError and self.port.isOpen():
            self.failed.emit(self.port.errorString())
