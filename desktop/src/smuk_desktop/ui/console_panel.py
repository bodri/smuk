from datetime import datetime
from PySide6.QtCore import Signal
from PySide6.QtWidgets import QFrame, QVBoxLayout, QHBoxLayout, QPlainTextEdit, QLineEdit, QPushButton, QFileDialog


class ConsolePanel(QFrame):
    command = Signal(str)
    error = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        self.output = QPlainTextEdit()
        self.output.setReadOnly(True)
        self.output.setMaximumBlockCount(2000)
        layout.addWidget(self.output)
        row = QHBoxLayout()
        self.input = QLineEdit()
        self.input.setPlaceholderText("Command, e.g. STATUS? or CAL:SHOW?")
        self.input.returnPressed.connect(self._send)
        row.addWidget(self.input, 1)
        self.send = QPushButton("Send")
        self.send.clicked.connect(self._send)
        row.addWidget(self.send)
        save = QPushButton("Save log…")
        save.clicked.connect(self._save)
        row.addWidget(save)
        layout.addLayout(row)
        self.set_connected(False)

    def _send(self):
        text = self.input.text().strip()
        if text:
            self.command.emit(text)
            self.input.clear()

    def append(self, direction, text):
        self.output.appendPlainText(f"{datetime.now():%H:%M:%S.%f}"[:-3] + f"  {direction:>2}  {text}")

    def set_connected(self, connected):
        self.input.setEnabled(connected)
        self.send.setEnabled(connected)

    def _save(self):
        path, _ = QFileDialog.getSaveFileName(self, "Save displayed console log", "smuk-session.txt", "Text files (*.txt)")
        if path:
            try:
                with open(path, "w", encoding="utf-8") as stream:
                    stream.write(self.output.toPlainText() + "\n")
            except OSError as error:
                self.error.emit(str(error))
