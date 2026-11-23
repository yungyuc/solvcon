# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING


"""The panels name the current view, which can differ from the focus.

The main window is never shown, so no sub-window becomes active the way a
click makes it; ``setCurrentView`` alone moves the current view.
"""

import unittest

from solvcon import testing

try:
    from solvcon import pilot
    from solvcon.pilot.base import _gui
    from PySide6 import QtCore
except ImportError:
    pilot = None


@testing.requires_pilot
class PanelsFollowTheCurrentViewTC(unittest.TestCase):

    def setUp(self):
        self.mgr = _gui.controller.build()
        self.area = self.mgr.mdiArea
        self.first = self.mgr.add2DWidget()
        self.mgr.subWindowOf(self.first).setWindowTitle("first canvas")
        self.second = self.mgr.add2DWidget()
        self.mgr.subWindowOf(self.second).setWindowTitle("second canvas")
        self.focused = self.area.activeSubWindow()

    def tearDown(self):
        self.area.closeAllSubWindows()

    def test_the_console_label_names_the_set_view(self):
        console = _gui.controller.console_panel
        console._ensure_panel()
        label = console.panel._canvas
        self.mgr.setCurrentView(self.first)
        self.assertEqual("canvas: first canvas", label.text())
        self.mgr.setCurrentView(self.second)
        self.assertEqual("canvas: second canvas", label.text())
        self.mgr.setCurrentView(self.first)
        self.assertIs(self.focused, self.area.activeSubWindow())

    def test_the_console_label_hears_of_the_last_view_deleted(self):
        # A sub-window deleted outright does not hide first, so only its
        # destruction can report that no view is left. Qt deletes it, since
        # Python must not take over what C++ created.
        console = _gui.controller.console_panel
        console._ensure_panel()
        label = console.panel._canvas
        last = self.mgr.subWindowOf(self.second)
        self.mgr.setCurrentView(self.first)
        # Earlier tests in the process may have left sub-windows open.
        for subwin in self.area.subWindowList():
            if subwin is not last:
                subwin.close()
        self.assertEqual("canvas: second canvas", label.text())
        last.deleteLater()
        QtCore.QCoreApplication.sendPostedEvents(
            None, QtCore.QEvent.Type.DeferredDelete)
        QtCore.QCoreApplication.sendPostedEvents()
        self.assertEqual("canvas: none", label.text())

    def test_the_painter_rereads_on_a_change_of_view(self):
        # The pages re-read only while on screen, which they never are here,
        # so watch the re-read request and what it reads. The re-read waits
        # for the event loop, so it reads the view the change settled on.
        painter = _gui.controller.painter
        painter._ensure_dock()
        read = []
        painter.panel.refresh = lambda: read.append(painter.panel._source())
        try:
            self.mgr.setCurrentView(self.first)
            self.assertEqual([], read)
            QtCore.QCoreApplication.sendPostedEvents()
        finally:
            del painter.panel.refresh
        self.assertEqual([self.first], read)
        self.assertIs(self.focused, self.area.activeSubWindow())


# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4 tw=79:
