# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING


"""The lifetime of the PySide wrappers the pilot bindings hand to Python.

PySide ties the wrapper of what a Qt method returns to the wrapper the method
was called on, and invalidates it once that wrapper is collected. A binding
that returns a Qt object therefore has to keep its wrapper alive for as long
as the object lives, or a one-off chain such as
``mgr.menu_model.group(id).checkedAction()`` breaks.
"""

import gc
import os
import subprocess
import sys
import unittest
import weakref

from solvcon import testing

try:
    from solvcon import pilot
    import shiboken6
    from PySide6 import QtCore, QtWidgets
except ImportError:
    pilot = None

_EXIT_HELPER_ENV = 'SOLVCON_QT_WRAPPER_EXIT_HELPER'

# Keep wrappers of several kinds, let one object die before exit and the
# rest go with the application.
_KEEP_WRAPPERS = """
from solvcon import pilot
from PySide6 import QtCore
mgr = pilot.RManager.instance.setUp()
canvas = mgr.add2DWidget()
mgr.add3DWidget()
mgr.menu_model.group("camera.mode").checkedAction().objectName()
mgr.mainWindow.windowTitle()
mgr.mdiArea.subWindowList()
mgr.subWindowOf(canvas).close()
QtCore.QCoreApplication.sendPostedEvents(
    None, QtCore.QEvent.Type.DeferredDelete)
"""

# What a process that tears down badly leaves on its output.
_UNCLEAN = ('Traceback', 'Fatal Python error', 'Exception ignored',
            'already deleted', 'Segmentation fault', 'QObject::')


@testing.requires_pilot
@unittest.skipUnless(os.environ.get(_EXIT_HELPER_ENV) == '1',
                     'exit-path subprocess helper')
def test_qt_wrapper_exit_helper():
    exec(_KEEP_WRAPPERS, {})


@testing.requires_pilot
class QtWrapperExitTC(unittest.TestCase):
    """A process that kept wrappers exits cleanly.

    Qt destroys what is left as the application goes at exit, and every
    destroyed object reaches the binding's release while the interpreter is
    shutting down.
    """

    def _run(self, cmd, env):
        completed = subprocess.run(
            cmd, capture_output=True, env=env, text=True, timeout=120,
            check=False)
        out = completed.stdout + completed.stderr
        self.assertEqual(0, completed.returncode, msg=out)
        for marker in _UNCLEAN:
            self.assertNotIn(marker, out)
        return out

    def test_the_pilot_owned_application_exits_cleanly(self):
        cmd, env = testing.rerun_command(
            __file__, 'test_qt_wrapper_exit_helper', _EXIT_HELPER_ENV)
        if cmd is None:
            self.skipTest(
                f'cannot tell how to rerun the tests with {sys.executable!r}')
        self.assertIn('1 passed', self._run(cmd, env))

    def test_a_borrowed_application_exits_cleanly(self):
        # PySide owns the application when Python creates it first, and
        # tears it down at its own exit.
        if not os.path.basename(sys.executable).lower().startswith('python'):
            self.skipTest('needs a Python interpreter to create the '
                          'application before the pilot loads')
        script = ("from PySide6 import QtWidgets\n"
                  "app = QtWidgets.QApplication([])\n" + _KEEP_WRAPPERS)
        self._run([sys.executable, '-c', script], os.environ.copy())


@testing.requires_pilot
class QtWrapperLifetimeTC(unittest.TestCase):

    def setUp(self):
        self.mgr = pilot.RManager.instance.setUp()

    def test_an_object_reached_through_a_temporary_stays_valid(self):
        # A current viewer leaves one camera mode checked.
        viewer = self.mgr.add3DWidget()
        action = self.mgr.menu_model.group("camera.mode").checkedAction()
        gc.collect()
        self.assertTrue(shiboken6.isValid(action))
        self.assertTrue(action.objectName().startswith("camera.mode_"))
        self.mgr.subWindowOf(viewer).close()

    def test_the_wrapper_outlives_the_temporary_it_came_through(self):
        self.mgr.add2DWidget()
        # Reached through a temporary wrapper of the MDI area; add2DWidget
        # activates the sub-window it opens.
        ref = weakref.ref(self.mgr.mdiArea.activeSubWindow())
        gc.collect()
        self.assertIsNotNone(ref())
        self.assertTrue(shiboken6.isValid(ref()))
        ref().close()

    def test_the_binding_lets_go_when_qt_destroys_the_object(self):
        widget = QtWidgets.QWidget()
        subwin = self.mgr.addSubWindow(widget)
        gc.collect()
        held = sys.getrefcount(subwin)
        subwin.close()
        QtCore.QCoreApplication.sendPostedEvents(
            None, QtCore.QEvent.Type.DeferredDelete)
        gc.collect()
        self.assertFalse(shiboken6.isValid(subwin))
        # Only the binding's own reference goes; the test still holds one.
        self.assertEqual(held - 1, sys.getrefcount(subwin))


# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4 tw=79:
