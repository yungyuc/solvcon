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


def _make_2d_mesh():
    """Two triangles and one quadrilateral in the z = 0 plane."""
    core = solvcon.core
    T = core.StaticMesh.TRIANGLE
    Q = core.StaticMesh.QUADRILATERAL
    mh = core.StaticMesh(ndim=2, nnode=6, nface=0, ncell=3)
    mh.ndcrd.ndarray[:, :] = [(0, 0), (1, 0), (0, 1), (1, 1), (2, 0), (2, 1)]
    mh.cltpn.ndarray[:] = [T, T, Q]
    mh.clnds.ndarray[:, :5] = [(3, 0, 3, 2, -1), (3, 0, 1, 3, -1),
                               (4, 1, 4, 5, 3)]
    mh.build_interior()
    mh.build_boundary()
    mh.build_ghost()
    return mh


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
class RManagerCameraModeTC(unittest.TestCase):
    """Each viewer keeps its own camera mode, and View > Camera shows and
    sets the mode of the current one."""

    def setUp(self):
        self.mgr = pilot.RManager.instance.setUp()
        self.model = self.mgr.menu_model
        self.group = self.model.group("camera.mode")

    def tearDown(self):
        self.mgr.mdiArea.closeAllSubWindows()

    def _checked(self):
        action = self.group.checkedAction()
        return None if action is None else action.objectName()

    def test_the_radio_sets_only_the_current_viewer(self):
        first = self.mgr.add3DWidget()
        second = self.mgr.add3DWidget()
        self.model.action("camera.mode_fps").trigger()
        self.assertEqual(("orbit", "fps"),
                         (first.cameraMode, second.cameraMode))

    def test_the_radio_shows_the_current_viewer(self):
        first = self.mgr.add3DWidget()
        self.model.action("camera.mode_fps").trigger()
        second = self.mgr.add3DWidget()
        # A new viewer starts in its own default and the radio follows it.
        self.assertEqual("orbit", second.cameraMode)
        self.assertEqual("camera.mode_orbit", self._checked())
        self.mgr.setCurrentView(first)
        self.assertEqual("camera.mode_fps", self._checked())

    def test_a_listener_of_the_change_sees_the_new_radio(self):
        viewer = self.mgr.add3DWidget()
        viewer.cameraMode = "fps"
        self.mgr.add2DWidget()
        seen = []

        def slot():
            seen.append(self._checked())

        self.mgr.notifier.currentViewChanged.connect(slot)
        try:
            self.mgr.setCurrentView(viewer)
        finally:
            self.mgr.notifier.currentViewChanged.disconnect(slot)
        self.assertEqual(["camera.mode_fps"], seen)

    def test_a_mode_set_from_code_moves_the_radio(self):
        viewer = self.mgr.add3DWidget()
        viewer.cameraMode = "pan"
        self.assertEqual("camera.mode_pan", self._checked())

    def test_a_2d_mesh_moves_the_radio_to_pan(self):
        viewer = self.mgr.add3DWidget()
        self.assertEqual("camera.mode_orbit", self._checked())
        viewer.updateMesh(_make_2d_mesh())
        self.assertEqual("camera.mode_pan", self._checked())

    def test_without_a_current_viewer_the_radio_is_empty_and_disabled(self):
        viewer = self.mgr.add3DWidget()
        self.mgr.add2DWidget()
        self.assertIsNone(self._checked())
        self.assertFalse(self.group.isEnabled())
        self.mgr.setCurrentView(viewer)
        self.assertTrue(self.group.isEnabled())
        self.assertEqual("camera.mode_orbit", self._checked())

    def test_without_a_current_viewer_the_moves_are_disabled(self):
        moves = self.model.menu("View/Camera move").actions()
        # Every move item is in the group that is enabled as one.
        self.assertEqual(set(moves),
                         set(self.model.group("camera.move").actions()))
        viewer = self.mgr.add3DWidget()
        self.assertTrue(all(a.isEnabled() for a in moves))
        self.mgr.add2DWidget()
        self.assertFalse(any(a.isEnabled() for a in moves))
        self.mgr.setCurrentView(viewer)
        self.assertTrue(all(a.isEnabled() for a in moves))


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


@testing.requires_pilot
class RManagerCurrentViewTC(unittest.TestCase):
    """Code sets the current view without activating a sub-window, and the
    user's touch takes it back.

    The main window is never shown here, so no sub-window ever becomes
    active the way a click makes it, and Qt sends no input. The tests move
    the current view with setCurrentView and send the events Qt would.
    """

    def setUp(self):
        self.mgr = pilot.RManager.instance.setUp()
        self.area = self.mgr.mdiArea
        self.model = self.mgr.menu_model
        # A new canvas takes the manager's tool, which an earlier test in the
        # process may have left on anything.
        self.mgr.setDrawTool("select")
        self.first = self.mgr.add2DWidget()
        self.second = self.mgr.add2DWidget()
        self.focused = self.area.activeSubWindow()

    def tearDown(self):
        self.mgr.setDrawTool("select")
        self.area.closeAllSubWindows()

    def _undoable_worlds(self):
        """Give each canvas a world with one undoable shape."""
        worlds = []
        for canvas in (self.first, self.second):
            world = solvcon.WorldFp64()
            canvas.updateWorld(world)
            world.add_circle(10, 10, 1)
            worlds.append(world)
        return worlds

    def test_the_queries_follow_the_set_view_not_the_focus(self):
        self.assertIs(self.second, self.mgr.currentR2DWidget())
        self.mgr.setCurrentView(self.first)
        self.assertIs(self.first, self.mgr.currentR2DWidget())
        self.assertIs(self.mgr.subWindowOf(self.first),
                      self.mgr.currentSubWindow())
        self.assertIs(self.focused, self.area.activeSubWindow())

    def test_undo_acts_on_the_set_view(self):
        worlds = self._undoable_worlds()
        self.mgr.setCurrentView(self.first)
        self.model.action("edit.undo").trigger()
        self.assertEqual([False, True], [w.can_undo for w in worlds])

    def test_the_draw_tool_reaches_the_set_view(self):
        self.mgr.setDrawTool("line")
        self.assertEqual("line", self.second.drawTool)
        self.assertNotEqual("line", self.first.drawTool)
        self.mgr.setCurrentView(self.first)
        self.assertEqual("line", self.first.drawTool)

    def test_a_viewer_is_set_by_its_widget(self):
        mesh = _make_3d_mesh()
        viewer = self.mgr.add3DWidget()
        viewer.updateMesh(mesh)
        self.mgr.setCurrentView(self.first)
        self.assertIsNone(self.mgr.currentR3DWidget())
        self.mgr.setCurrentView(viewer)
        self.assertIs(viewer, self.mgr.currentR3DWidget())
        pose = tuple(viewer.cameraPosition)
        self.mgr.setCurrentView(self.first)
        self.model.action("camera.move_up").trigger()
        self.assertEqual(pose, tuple(viewer.cameraPosition))

    def test_an_activation_takes_over_from_the_set_view(self):
        self.mgr.setCurrentView(self.first)
        third = self.mgr.add2DWidget()
        self.assertIs(third, self.mgr.currentR2DWidget())
        self.mgr.setCurrentView(self.first)
        self.area.setActiveSubWindow(self.mgr.subWindowOf(self.second))
        self.assertIs(self.second, self.mgr.currentR2DWidget())

    def _touch(self, canvas, *kinds):
        from PySide6 import QtGui, QtWidgets
        target = self.mgr.subWindowOf(canvas).widget()
        pos = QtCore.QPointF(5, 5)
        for kind in kinds:
            if isinstance(kind, QtCore.Qt.FocusReason):
                event = QtGui.QFocusEvent(QtCore.QEvent.Type.FocusIn, kind)
            else:
                event = QtGui.QMouseEvent(
                    kind, pos, target.mapToGlobal(pos),
                    QtCore.Qt.LeftButton, QtCore.Qt.NoButton,
                    QtCore.Qt.NoModifier)
            QtWidgets.QApplication.sendEvent(target, event)

    def test_a_press_on_the_focused_window_takes_the_view_back(self):
        # The user sees only the focused canvas; after a click there, undo
        # must not edit the canvas code chose earlier.
        worlds = self._undoable_worlds()
        self.mgr.setCurrentView(self.first)
        self.assertIs(self.focused, self.mgr.subWindowOf(self.second))
        self._touch(self.second, QtCore.QEvent.Type.MouseButtonPress,
                    QtCore.QEvent.Type.MouseButtonRelease)
        self.assertIs(self.second, self.mgr.currentR2DWidget())
        self.model.action("edit.undo").trigger()
        self.assertEqual([True, False], [w.can_undo for w in worlds])

    def test_a_focus_in_takes_the_view_back(self):
        for reason in (QtCore.Qt.MouseFocusReason, QtCore.Qt.TabFocusReason,
                       QtCore.Qt.BacktabFocusReason,
                       QtCore.Qt.ShortcutFocusReason):
            with self.subTest(reason=reason):
                self.mgr.setCurrentView(self.first)
                self._touch(self.second, reason)
                self.assertIs(self.second, self.mgr.currentR2DWidget())

    def test_focus_qt_restores_keeps_the_set_view(self):
        # A menu that closes hands the focus back with PopupFocusReason, and
        # reactivating the window does so with ActiveWindowFocusReason; the
        # off-screen window here never gets either for real, so send what Qt
        # sends.
        for reason in (QtCore.Qt.PopupFocusReason,
                       QtCore.Qt.ActiveWindowFocusReason,
                       QtCore.Qt.MenuBarFocusReason,
                       QtCore.Qt.OtherFocusReason):
            with self.subTest(reason=reason):
                self.mgr.setCurrentView(self.first)
                self._touch(self.second, reason)
                self.assertIs(self.first, self.mgr.currentR2DWidget())

    def test_a_menu_command_acts_on_the_set_view(self):
        # QMenuPrivate::activateAction closes the popups, which restores the
        # focus with a PopupFocusReason FocusIn, before it triggers the
        # action. Replay that order: the undo must still reach the set view.
        worlds = self._undoable_worlds()
        self.mgr.setCurrentView(self.first)
        self._touch(self.second, QtCore.Qt.PopupFocusReason)
        self.model.action("edit.undo").trigger()
        self.assertEqual([False, True], [w.can_undo for w in worlds])

    def test_a_key_in_the_focused_viewer_takes_the_view_back(self):
        # Escape is a widget shortcut on the focused viewer while the reset
        # action resets the current one; the key's ShortcutOverride, which
        # Qt sends before matching the shortcut, has to align the two.
        from PySide6 import QtGui, QtWidgets
        mesh = _make_3d_mesh()
        first = self.mgr.add3DWidget()
        first.updateMesh(mesh)
        focused = self.mgr.add3DWidget()
        focused.updateMesh(mesh)
        self.mgr.setCurrentView(first)
        host = self.mgr.subWindowOf(focused).widget()
        QtWidgets.QApplication.sendEvent(host, QtGui.QKeyEvent(
            QtCore.QEvent.Type.ShortcutOverride, QtCore.Qt.Key_Escape,
            QtCore.Qt.NoModifier))
        self.assertIs(focused, self.mgr.currentR3DWidget())

    def test_closing_the_set_view_falls_back_to_the_focus(self):
        self.mgr.setCurrentView(self.first)
        self.mgr.subWindowOf(self.first).close()
        self.assertIs(self.second, self.mgr.currentR2DWidget())

    def test_deleting_the_last_view_leaves_none(self):
        # A sub-window deleted outright does not hide first; only its
        # destruction tells the manager the view is gone. Qt deletes them,
        # since Python must not take over what C++ created.
        ours = [self.mgr.subWindowOf(c) for c in (self.first, self.second)]
        # Earlier tests in the process may have left sub-windows open.
        for subwin in self.area.subWindowList():
            if subwin not in ours:
                subwin.close()
        self.mgr.setCurrentView(self.first)
        for subwin in ours:
            subwin.deleteLater()
        QtCore.QCoreApplication.sendPostedEvents(
            None, QtCore.QEvent.Type.DeferredDelete)
        QtCore.QCoreApplication.sendPostedEvents()
        self.assertIsNone(self.mgr.currentSubWindow())
        self.assertIsNone(self.mgr.currentR2DWidget())

    def test_file_close_stays_on_the_focus(self):
        from solvcon.pilot.base import _gui
        # With no focused sub-window the action closes the main window.
        self.assertIsNotNone(self.focused)
        self.mgr.setCurrentView(self.first)
        _gui.close_active_window(self.mgr)
        self.assertTrue(self.focused.isHidden())
        self.assertFalse(self.mgr.subWindowOf(self.first).isHidden())

    def test_a_widget_outside_the_manager_is_rejected(self):
        from PySide6 import QtWidgets
        with self.assertRaises(ValueError):
            self.mgr.setCurrentView(QtWidgets.QWidget())
        self.assertIs(self.second, self.mgr.currentR2DWidget())


# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4 tw=79:
