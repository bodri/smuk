# Desktop application

This is a Python/PySide6 desktop client for the existing firmware serial console.
Read README.md before changing connection or polling behavior.

- Keep firmware and hardware changes outside this directory out of scope unless requested.
- Keep protocol parsing and measurement models independent of Qt and serial hardware.
- Use asynchronous QSerialPort; never block the UI waiting for replies.
- One command may be outstanding; complete known multiline replies before issuing the next.
- A timeout ends the session to prevent late replies being matched to a new command.
- Logs are unsolicited. Reboot banners invalidate the connection and statistics.
- Display confirmed device state. Invalid/stale readings must not appear as live valid data.
- No automatic restoration of settings on reconnect and no PA/source commands.
- Statistics use distinct, valid observed measurements, not every ADC sample.
- Use bounded queues/history; avoid unsolicited Flash writes or calibration operations.
- Run `.venv/bin/python -m pytest` from desktop/. Test UI offscreen where possible.
- Document macOS launch and packaging instructions. Ignore environments/build artifacts.
