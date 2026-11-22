# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING


import unittest

import solvcon
from solvcon import testing

try:
    from solvcon import pilot
except ImportError:
    pilot = None


def _make_3d_mesh():
    """A single tetrahedron."""
    core = solvcon.core
    mh = core.StaticMesh(ndim=3, nnode=4, nface=4, ncell=1)
    mh.ndcrd.ndarray[:, :] = [(0, 0, 0), (0, 1, 0), (-1, 1, 0), (0, 1, 1)]
    mh.cltpn.ndarray[:] = core.StaticMesh.TETRAHEDRON
    mh.clnds.ndarray[:, :5] = [(4, 0, 1, 2, 3)]
    mh.build_interior()
    mh.build_boundary()
    mh.build_ghost()
    return mh


@testing.requires_pilot
class RManagerSubWindowTC(unittest.TestCase):

    def setUp(self):
        self.mgr = pilot.RManager.instance.setUp()

    def test_add_subwindow_opens_at_widget_size(self):
        from PySide6 import QtWidgets
        subwin = self.mgr.addSubWindow(QtWidgets.QWidget())
        try:
            self.assertEqual((300, 200), (subwin.width(), subwin.height()))
        finally:
            subwin.close()


@testing.requires_pilot
class RManagerFocusedWidgetTC(unittest.TestCase):
    """The menu actions reach the widget in the current sub-window."""

    def setUp(self):
        self.mgr = pilot.RManager.instance.setUp()
        self.model = self.mgr.menu_model

    def tearDown(self):
        self.mgr.mdiArea.closeAllSubWindows()

    def test_undo_and_redo_reach_the_focused_canvas(self):
        canvas = self.mgr.add2DWidget()
        world = solvcon.WorldFp64()
        canvas.updateWorld(world)
        world.add_circle(0, 0, 1)
        self.assertTrue(world.can_undo)
        self.model.action("edit.undo").trigger()
        self.assertFalse(world.can_undo)
        self.assertTrue(world.can_redo)
        self.model.action("edit.redo").trigger()
        self.assertTrue(world.can_undo)
        self.assertFalse(world.can_redo)

    def test_camera_moves_only_the_focused_viewer(self):
        mesh = _make_3d_mesh()
        other = self.mgr.add3DWidget()
        other.updateMesh(mesh)
        focused = self.mgr.add3DWidget()
        focused.updateMesh(mesh)
        self.assertIs(focused, self.mgr.currentR3DWidget())
        others_pose = tuple(other.cameraPosition)
        focused_pose = tuple(focused.cameraPosition)
        self.model.action("camera.move_up").trigger()
        self.assertEqual(others_pose, tuple(other.cameraPosition))
        self.assertNotEqual(focused_pose, tuple(focused.cameraPosition))


# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4 tw=79:
