"""Exercise QSerialPort with a virtual terminal, never a real SMU."""
import os
import time
import tty
from smuk_desktop.instrument.connection import SerialConnection


def wait(app, condition, timeout=1):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        app.processEvents()
        if condition():
            return
        time.sleep(0.005)
    assert condition()


def test_serial_port_fragmentation_and_configuration(app):
    master, slave = os.openpty()
    tty.setraw(slave)
    os.set_blocking(master, False)
    connection = SerialConnection()
    received, failures = [], []
    connection.line_received.connect(received.append)
    connection.failed.connect(failures.append)
    try:
        connection.open(os.ttyname(slave))
        assert connection.port.isOpen(), failures
        assert connection.port.baudRate() == 115200
        connection.write("PING")
        sent = bytearray()
        def read_command():
            try:
                sent.extend(os.read(master, 1024))
            except BlockingIOError:
                pass
            return b"PING\r\n" in sent
        wait(app, read_command)
        os.write(master, b"PO")
        app.processEvents()
        os.write(master, b"NG\r\nADC log\r\n")
        wait(app, lambda: len(received) == 2)
        assert received == ["PONG", "ADC log"]
        assert not failures
    finally:
        connection.close()
        os.close(slave)
        os.close(master)
