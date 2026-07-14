# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING

"""
Tests for the lower-right size grip that RMdiSubWindow adds to each MDI
subwindow, so the frame is easy to resize.
"""

import unittest

import solvcon

try:
    from solvcon import pilot
    from PySide6 import QtCore, QtGui, QtWidgets
except ImportError:
    pilot = None


def _find_grip(subwin):
    """The subwindow's resize grip: the fixed 16x16 child that carries the
    lower-right diagonal resize cursor. It is a plain widget rather than a
    QSizeGrip, which QMdiSubWindow would seize, reserving layout space that
    corrupts the hosted canvas."""
    for child in subwin.findChildren(QtWidgets.QWidget):
        if (child.size() == QtCore.QSize(16, 16)
                and child.cursor().shape() == QtCore.Qt.SizeFDiagCursor):
            return child
    return None


def _drag(widget, dx, dy):
    """Press at the widget center, move by (dx, dy), and release."""
    center = QtCore.QPointF(widget.width() / 2, widget.height() / 2)
    press_glob = widget.mapToGlobal(center.toPoint())
    move_glob = press_glob + QtCore.QPoint(dx, dy)
    seq = [
        (QtCore.QEvent.Type.MouseButtonPress, press_glob,
         QtCore.Qt.LeftButton, QtCore.Qt.LeftButton),
        (QtCore.QEvent.Type.MouseMove, move_glob,
         QtCore.Qt.NoButton, QtCore.Qt.LeftButton),
        (QtCore.QEvent.Type.MouseButtonRelease, move_glob,
         QtCore.Qt.LeftButton, QtCore.Qt.NoButton),
    ]
    for etype, glob, button, buttons in seq:
        local = widget.mapFromGlobal(glob)
        event = QtGui.QMouseEvent(etype, QtCore.QPointF(local),
                                  QtCore.QPointF(glob), button, buttons,
                                  QtCore.Qt.NoModifier)
        QtWidgets.QApplication.sendEvent(widget, event)


@unittest.skipUnless(solvcon.HAS_PILOT, "Qt pilot is not built")
class SubWindowResizeGripTC(unittest.TestCase):
    """The size grip is present, anchored lower-right, and resizes the
    frame."""

    @classmethod
    def setUpClass(cls):
        cls.mgr = pilot.RManager.instance.setUp()

    def setUp(self):
        self.mgr.add2DWidget()
        self.sub = self.mgr.mdiArea.subWindowList()[-1]
        self.sub.resize(400, 300)
        # A hidden subwindow gets its resize event lazily, so post one now to
        # settle the grip into its corner before the assertions read it.
        event = QtGui.QResizeEvent(self.sub.size(), QtCore.QSize(0, 0))
        QtWidgets.QApplication.sendEvent(self.sub, event)

    def test_grip_exists(self):
        self.assertIsNotNone(_find_grip(self.sub))

    def test_grip_sits_in_the_lower_right(self):
        grip = _find_grip(self.sub)
        area = self.sub.contentsRect()
        self.assertEqual(grip.geometry().right(), area.right())
        self.assertEqual(grip.geometry().bottom(), area.bottom())

    def test_dragging_the_grip_resizes_the_frame(self):
        grip = _find_grip(self.sub)
        start = self.sub.geometry()
        _drag(grip, 40, 50)
        end = self.sub.geometry()
        self.assertEqual(end.width(), start.width() + 40)
        self.assertEqual(end.height(), start.height() + 50)
        # The lower-right grip drags the right and bottom edges; the top-left
        # corner stays put.
        self.assertEqual(end.topLeft(), start.topLeft())

# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4 tw=79:
