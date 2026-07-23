# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING


"""
Remember the pilot main window's size and location across sessions.
"""


from PySide6 import QtCore, QtGui

from ...config import UserConfig
from . import _gui_common

__all__ = [
    'WindowState',
    'apply_geometry',
    'capture_geometry',
]


def _is_int(value):
    """Whether ``value`` is a plain integer, excluding ``bool``."""
    return isinstance(value, int) and not isinstance(value, bool)


def _on_screen(x, y):
    """Whether the point ``(x, y)`` lies on some connected screen.

    This drops a location left behind by a monitor that is no longer
    attached, so the window cannot be restored out of sight.
    """
    point = QtCore.QPoint(x, y)
    for screen in QtGui.QGuiApplication.screens():
        if screen.availableGeometry().contains(point):
            return True
    return False


def apply_geometry(window, section):
    """Apply a saved ``window`` section to ``window``.

    A missing section, or a size or location that is absent, malformed, or
    off every connected screen, leaves the window untouched. Keeping the Qt
    calls in one free function makes the size-and-location policy testable
    against any window, not only the live main window.
    """
    if not isinstance(section, dict):
        return
    width, height = section.get("width"), section.get("height")
    if _is_int(width) and _is_int(height) and width > 0 and height > 0:
        window.resize(width, height)
    x, y = section.get("x"), section.get("y")
    if _is_int(x) and _is_int(y) and _on_screen(x, y):
        window.move(x, y)


def capture_geometry(window):
    """Return the current size and location of ``window`` as a section dict."""
    size, pos = window.size(), window.pos()
    return {
        "width": size.width(),
        "height": size.height(),
        "x": pos.x(),
        "y": pos.y(),
    }


class WindowState(_gui_common.PilotFeature):
    """Restore the saved main-window geometry and save it again on exit.

    Construction restores the last recorded size and location to the main
    window, overriding the launch default; the current geometry is written
    back when the application is about to quit. The values persist in the
    user configuration file under the ``window`` section as plain integers,
    so they stay readable and hand-editable.
    """

    #: Configuration key holding the window geometry.
    SECTION = "window"

    def __init__(self, *args, **kw):
        config = kw.pop('config', None)
        super(WindowState, self).__init__(*args, **kw)
        self._config = UserConfig() if config is None else config
        self._config.load()
        self.restore()
        app = QtCore.QCoreApplication.instance()
        if app is not None:
            app.aboutToQuit.connect(self.save)

    def restore(self):
        """Apply the saved geometry to the main window."""
        apply_geometry(self._mainWindow, self._config.get(self.SECTION))

    def save(self):
        """Capture the current geometry and persist it."""
        self._config.set(self.SECTION, capture_geometry(self._mainWindow))
        self._config.save()


# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
