import argparse
from importlib.resources import files
import sys
from PySide6.QtCore import QTimer
from PySide6.QtGui import QIcon
from PySide6.QtWidgets import QApplication
from .instrument.controller import Controller
from .ui.main_window import MainWindow


def main(argv=None):
    parser = argparse.ArgumentParser(description="SMUK desktop measurement console")
    parser.add_argument("--screenshot", help="Save an offline UI screenshot and exit")
    args = parser.parse_args(argv)
    app = QApplication([sys.argv[0]])
    app.setApplicationName("SMUK")
    app.setOrganizationName("SMUK")
    app.setStyle("Fusion")
    app.setStyleSheet(files("smuk_desktop").joinpath("resources/theme.qss").read_text())
    controller = Controller()
    window = MainWindow(controller)
    window.setWindowIcon(QIcon(str(files("smuk_desktop").joinpath("resources/icons/smuk.svg"))))
    window.show()
    if args.screenshot:
        def capture():
            saved = window.grab().save(args.screenshot)
            app.exit(0 if saved else 1)
        QTimer.singleShot(200, capture)
    return app.exec()
