# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING


"""
Tests for restoring and saving the pilot main-window geometry.
"""


import os
import tempfile
import unittest

import solvcon

try:
    from solvcon import pilot
    from solvcon.config import UserConfig
    from solvcon.pilot.base import _gui
    from solvcon.pilot.base import _window_state
    from PySide6 import QtGui, QtWidgets
except ImportError:
    pilot = None

GITHUB_ACTIONS = os.getenv('GITHUB_ACTIONS', False)


@unittest.skipIf(GITHUB_ACTIONS or not solvcon.HAS_PILOT,
                 "GUI is not available in GitHub Actions")
class GeometrySeamTC(unittest.TestCase):
    """Drive the geometry policy against a hidden, fully controlled window.

    A hidden top-level window honors resize() and move() synchronously,
    without a window manager imposing its own size, so these assertions are
    stable where a live shown window would be flaky.
    """

    def setUp(self):
        _gui.controller.build()
        self.win = QtWidgets.QMainWindow()
        self.win.resize(1000, 600)

    def tearDown(self):
        self.win.deleteLater()
        QtWidgets.QApplication.processEvents()

    def _on_screen_origin(self):
        """A point safely inside the primary screen's available area."""
        avail = QtGui.QGuiApplication.primaryScreen().availableGeometry()
        return avail.x() + 40, avail.y() + 40

    def test_apply_sets_size_and_location(self):
        x, y = self._on_screen_origin()
        _window_state.apply_geometry(
            self.win, {"width": 820, "height": 540, "x": x, "y": y})
        self.assertEqual(self.win.size().width(), 820)
        self.assertEqual(self.win.size().height(), 540)
        self.assertEqual((self.win.pos().x(), self.win.pos().y()), (x, y))

    def test_apply_ignores_a_missing_section(self):
        _window_state.apply_geometry(self.win, None)
        self.assertEqual((self.win.size().width(), self.win.size().height()),
                         (1000, 600))

    def test_apply_rejects_non_positive_size(self):
        _window_state.apply_geometry(self.win, {"width": 0, "height": -5})
        self.assertEqual((self.win.size().width(), self.win.size().height()),
                         (1000, 600))

    def test_apply_rejects_off_screen_location(self):
        _window_state.apply_geometry(self.win, {"x": -30000, "y": -30000})
        self.assertNotEqual((self.win.pos().x(), self.win.pos().y()),
                            (-30000, -30000))

    def test_capture_reads_the_current_geometry(self):
        x, y = self._on_screen_origin()
        self.win.resize(760, 480)
        self.win.move(x, y)
        section = _window_state.capture_geometry(self.win)
        self.assertEqual(section["width"], 760)
        self.assertEqual(section["height"], 480)
        self.assertEqual((section["x"], section["y"]), (x, y))


@unittest.skipIf(GITHUB_ACTIONS or not solvcon.HAS_PILOT,
                 "GUI is not available in GitHub Actions")
class WindowStateRoundTripTC(unittest.TestCase):
    """The feature restores from and saves to its configuration file."""

    def setUp(self):
        self.mgr = _gui.controller.build()
        fd, self.path = tempfile.mkstemp(suffix=".json")
        os.close(fd)
        os.remove(self.path)

    def tearDown(self):
        if os.path.exists(self.path):
            os.remove(self.path)

    def test_save_writes_the_current_window_section(self):
        state = _window_state.WindowState(
            mgr=self.mgr, config=UserConfig(self.path))
        state.save()
        window = UserConfig(self.path).load().get("window")
        self.assertEqual(
            window, _window_state.capture_geometry(self.mgr.mainWindow))


# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
