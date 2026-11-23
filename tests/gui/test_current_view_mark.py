# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING


"""The current-view mark keeps to its sub-window's frame on a shown window.

A sub-window resizes, and so moves its mark, only once its window is
shown; hidden, Qt holds the resize back until it is.
"""

import unittest

from solvcon import testing

try:
    from solvcon import pilot
    from PySide6 import QtCore, QtWidgets
except ImportError:
    pilot = None


@testing.requires_live_window
class CurrentViewMarkTC(unittest.TestCase):

    def setUp(self):
        self.mgr = pilot.RManager.instance.setUp()
        self.area = self.mgr.mdiArea
        self.mgr.show()
        self.area.closeAllSubWindows()
        QtCore.QCoreApplication.sendPostedEvents()
        self.first = self.mgr.subWindowOf(self.mgr.add2DWidget())
        self.mgr.add2DWidget()
        self.mgr.setCurrentView(self.first.widget())
        self.mark = self.first.findChild(QtWidgets.QWidget, "currentViewMark")

    def tearDown(self):
        self.area.closeAllSubWindows()
        QtCore.QCoreApplication.sendPostedEvents()

    def test_the_mark_follows_a_resize(self):
        self.first.resize(350, 260)
        self.assertEqual(self.first.rect(), self.mark.geometry())

    def test_the_mark_follows_a_tile(self):
        self.area.tileSubWindows()
        self.assertEqual(self.first.rect(), self.mark.geometry())


# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4 tw=79:
