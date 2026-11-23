# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING


import os
import subprocess
import sys
import unittest

import solvcon
from solvcon import testing

try:
    from solvcon import pilot
    from PySide6 import QtCore
except ImportError:
    pilot = None

_HELPER_ENV = 'SOLVCON_RMANAGER_BEFORE_SETUP_HELPER'


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
@unittest.skipUnless(os.environ.get(_HELPER_ENV) == '1',
                     'before-setUp subprocess helper')
def test_rmanager_before_setup_helper():
    # Runs in a fresh process, where nothing has called setUp() yet.
    mgr = pilot.RManager.instance
    for name in ('add2DWidget', 'add3DWidget'):
        try:
            getattr(mgr, name)()
        except RuntimeError as e:
            assert 'setUp' in str(e), str(e)
        else:
            raise AssertionError(f'{name} before setUp did not raise')
    from PySide6 import QtWidgets
    try:
        mgr.addSubWindow(QtWidgets.QWidget())
    except RuntimeError as e:
        assert 'setUp' in str(e), str(e)
    else:
        raise AssertionError('addSubWindow before setUp did not raise')
    # The queries have nothing to report, which is not an error.
    assert mgr.currentR2DWidget() is None
    assert mgr.currentR3DWidget() is None
    assert [] == mgr.list2DWidgets() == mgr.list3DWidgets()
    # A null Qt object reaches Python as None instead of aborting.
    assert mgr.mdiArea is None


@testing.requires_pilot
class RManagerBeforeSetUpTC(unittest.TestCase):
    """The widget factories need the MDI area that setUp() builds.

    They used to hand back None, and the Python addSubWindow wrapper went on
    to resize the null sub-window and crashed the process. The suite has set
    the singleton up long before this test runs, so the check runs in a
    process of its own.
    """

    def test_factories_raise_before_setup(self):
        cmd, env = testing.rerun_command(
            __file__, 'test_rmanager_before_setup_helper', _HELPER_ENV)
        if cmd is None:
            self.skipTest(
                f'cannot tell how to rerun the tests with {sys.executable!r}')
        completed = subprocess.run(
            cmd, capture_output=True, env=env, text=True, timeout=120,
            check=False)
        out = completed.stdout + completed.stderr
        self.assertEqual(0, completed.returncode, msg=out)
        self.assertIn('1 passed', out)


@testing.requires_pilot
class RManagerSubWindowTC(unittest.TestCase):

    def setUp(self):
        self.mgr = pilot.RManager.instance.setUp()

    def test_add_subwindow_rejects_a_non_widget(self):
        # The binding hands a non-widget QObject over as a null widget,
        # which QMdiArea declines with a null sub-window.
        with self.assertRaises(ValueError):
            self.mgr.addSubWindow(QtCore.QObject())

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


@testing.requires_pilot
class RManagerSubWindowOfTC(unittest.TestCase):
    """A caller reaches the sub-window of a widget it holds, and the widget
    of a sub-window it holds, without activating anything."""

    def setUp(self):
        self.mgr = pilot.RManager.instance.setUp()
        self.area = self.mgr.mdiArea

    def tearDown(self):
        self.area.closeAllSubWindows()

    def test_the_first_of_two_canvases_is_reached_without_activating_it(self):
        first = self.mgr.add2DWidget()
        second = self.mgr.add2DWidget()
        active = self.area.activeSubWindow()
        self.assertIs(second, self.mgr.canvasOf(active))
        subwin = self.mgr.subWindowOf(first)
        self.assertIsNotNone(subwin)
        self.assertIsNot(active, subwin)
        self.assertIs(first, self.mgr.canvasOf(subwin))
        self.assertIsNone(self.mgr.domainWidgetOf(subwin))
        self.assertIs(active, self.area.activeSubWindow())

    def test_a_viewer_is_reached_through_its_host(self):
        viewer = self.mgr.add3DWidget()
        self.mgr.add2DWidget()
        subwin = self.mgr.subWindowOf(viewer)
        self.assertIsNotNone(subwin)
        self.assertIsNot(self.area.activeSubWindow(), subwin)
        self.assertIs(viewer, self.mgr.domainWidgetOf(subwin))
        self.assertIsNone(self.mgr.canvasOf(subwin))

    def test_a_python_widget_is_reached_too(self):
        from PySide6 import QtWidgets
        widget = QtWidgets.QWidget()
        subwin = self.mgr.addSubWindow(widget)
        self.assertIs(subwin, self.mgr.subWindowOf(widget))
        self.assertIs(subwin, self.mgr.subWindowOf(
            QtWidgets.QLabel("inside", widget)))

    def test_a_foreign_widget_has_no_subwindow(self):
        from PySide6 import QtWidgets
        self.assertIsNone(self.mgr.subWindowOf(QtWidgets.QWidget()))
        # A sub-window of another MDI area is not the manager's either.
        other = QtWidgets.QMdiArea()
        widget = QtWidgets.QWidget()
        other.addSubWindow(widget)
        self.assertIsNone(self.mgr.subWindowOf(widget))


# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4 tw=79:
