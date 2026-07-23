# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING


"""
Tests for the "Window" menu window manager.

Exercise the dynamic sub-window list the WindowManager feature anchors
under the "Window" menu: what it lists, which entry is checked, and what
activating an entry does.
"""

import os
import unittest

import solvcon

try:
    from solvcon import pilot
    from solvcon.pilot.base import _gui
    from solvcon.pilot.onedim import _euler1d
    from solvcon.pilot.panel._window_manager import WindowManager
    from PySide6 import QtWidgets
except ImportError:
    pilot = None

GITHUB_ACTIONS = os.getenv('GITHUB_ACTIONS', False)


@unittest.skipIf(GITHUB_ACTIONS or not solvcon.HAS_PILOT,
                 "GUI is not available in GitHub Actions")
class WindowMenuTC(unittest.TestCase):
    """Drive the WindowManager list through the live "Window" menu."""

    def setUp(self):
        self.mgr = _gui.controller.build()
        self.model = self.mgr.menu_model
        self.area = self.mgr.mdiArea
        # Show the manager so a freshly added sub-window reports visible; the
        # list filters on isVisible() to tell open windows from closed ones.
        self.mgr.show()
        # The viewers do not delete on close, so an earlier test's windows
        # linger hidden in the list; drop them before each case.
        self.area.closeAllSubWindows()
        QtWidgets.QApplication.processEvents()

    def tearDown(self):
        self.area.closeAllSubWindows()
        QtWidgets.QApplication.processEvents()

    def _show(self):
        """Fire the real freshness hook that refreshes the list."""
        self.model.menu("Window").aboutToShow.emit()

    def _items(self):
        """The dynamic per-sub-window actions, isolated by objectName."""
        return [a for a in self.model.menu("Window").actions()
                if a.objectName() == WindowManager.ITEM_ID]

    def test_empty_lists_no_windows(self):
        self._show()
        self.assertEqual(self._items(), [])

    def test_menu_is_never_empty(self):
        # A native menu bar hides an empty menu, and a hidden menu can
        # never fire aboutToShow to fill itself, so the list must be
        # seeded without waiting for the first show.
        self.assertFalse(self.model.menu("Window").isEmpty())

    def test_single_window_is_listed(self):
        self.mgr.add2DWidget()
        self._show()
        items = self._items()
        self.assertEqual(len(items), 1)
        self.assertTrue(items[0].text().endswith("2D canvas"))

    def test_multiple_windows_are_listed(self):
        self.mgr.add2DWidget()
        self.mgr.add3DWidget()
        self._show()
        items = self._items()
        self.assertEqual(len(items), 2)
        self.assertTrue(items[0].text().endswith("2D canvas"))
        self.assertTrue(items[1].text().endswith("Domain viewer"))

    def test_active_window_is_checked(self):
        self.mgr.add2DWidget()
        self.mgr.add3DWidget()
        self._show()
        items = self._items()
        checked = [a for a in items if a.isChecked()]
        self.assertEqual(len(checked), 1)
        active = self.area.activeSubWindow()
        self.assertTrue(checked[0].text().endswith(active.windowTitle()))

    def test_selecting_item_activates_that_window(self):
        self.mgr.add2DWidget()
        first = self.area.subWindowList()[-1]
        self.mgr.add3DWidget()
        self._show()
        items = self._items()
        items[0].trigger()
        self.assertIs(self.area.activeSubWindow(), first)
        self.assertIsNotNone(self.mgr.currentR2DWidget())

    def test_list_updates_when_window_added(self):
        self.mgr.add2DWidget()
        self._show()
        self.assertEqual(len(self._items()), 1)
        self.mgr.add3DWidget()
        self._show()
        self.assertEqual(len(self._items()), 2)

    def test_closing_window_removes_its_entry(self):
        self.mgr.add2DWidget()
        self.mgr.add3DWidget()
        self._show()
        self.assertEqual(len(self._items()), 2)
        self.area.subWindowList()[-1].close()
        self._show()
        self.assertEqual(len(self._items()), 1)

    def test_solver_window_carries_its_title(self):
        # A 1D solver window used to carry no title at all, so the list
        # could only show it as a bare numbered "window" entry.
        app = _euler1d.Euler1DApp(mgr=self.mgr)
        app.run()
        self._show()
        items = self._items()
        self.assertTrue(items[-1].text().endswith("Euler solver"))


@unittest.skipIf(GITHUB_ACTIONS or not solvcon.HAS_PILOT,
                 "GUI is not available in GitHub Actions")
class WindowLayoutTC(unittest.TestCase):
    """Drive the layout actions in the "Window" menu."""

    def setUp(self):
        self.mgr = _gui.controller.build()
        self.model = self.mgr.menu_model
        self.area = self.mgr.mdiArea
        self.mgr.show()
        self.area.closeAllSubWindows()
        QtWidgets.QApplication.processEvents()

    def tearDown(self):
        self.area.closeAllSubWindows()
        QtWidgets.QApplication.processEvents()

    def _show(self):
        """Fire the real freshness hook that refreshes the menu."""
        self.model.menu("Window").aboutToShow.emit()

    def _layout_ids(self):
        """Every static layout action the menu is expected to carry.

        Not a class attribute: WindowManager is unbound when the GUI
        imports fail, and a class-body reference would then break
        collection of this skipped module.
        """
        return (WindowManager.TILE_ID, WindowManager.CASCADE_ID,
                WindowManager.HORIZONTAL_ID, WindowManager.VERTICAL_ID)

    def test_layout_actions_precede_the_list(self):
        # The static layout section sits above the separator; the
        # dynamic list follows it.
        acts = self.model.menu("Window").actions()
        names = [a.objectName() for a in acts]
        sep = [i for i, a in enumerate(acts) if a.isSeparator()]
        self.assertEqual(len(sep), 1)
        for aid in self._layout_ids():
            self.assertLess(names.index(aid), sep[0])

    def test_layout_actions_follow_open_windows(self):
        self._show()
        for aid in self._layout_ids():
            self.assertFalse(self.model.action(aid).isEnabled())
        self.mgr.add2DWidget()
        self._show()
        for aid in self._layout_ids():
            self.assertTrue(self.model.action(aid).isEnabled())

    def _stack(self):
        """Pile every sub-window onto one rectangle.

        The MDI area spreads new sub-windows out on its own, so a layout
        assertion made from that state could pass with the action doing
        nothing. Piling them up first makes the layout the only way the
        geometries can separate.
        """
        for subwin in self.area.subWindowList():
            subwin.setGeometry(0, 0, 200, 150)

    def test_tile_separates_the_windows(self):
        self.mgr.add2DWidget()
        self.mgr.add3DWidget()
        self._stack()
        self.model.action(WindowManager.TILE_ID).trigger()
        QtWidgets.QApplication.processEvents()
        one, two = [s.geometry() for s in self.area.subWindowList()]
        self.assertFalse(one.intersects(two))

    def test_cascade_offsets_the_windows(self):
        self.mgr.add2DWidget()
        self.mgr.add3DWidget()
        self._stack()
        self.model.action(WindowManager.CASCADE_ID).trigger()
        QtWidgets.QApplication.processEvents()
        one, two = [s.geometry() for s in self.area.subWindowList()]
        self.assertNotEqual(one.topLeft(), two.topLeft())

    def test_horizontal_tiling_forms_a_single_row(self):
        self.mgr.add2DWidget()
        self.mgr.add3DWidget()
        self._stack()
        self.model.action(WindowManager.HORIZONTAL_ID).trigger()
        QtWidgets.QApplication.processEvents()
        one, two = [s.geometry() for s in self.area.subWindowList()]
        self.assertFalse(one.intersects(two))
        self.assertEqual(one.y(), two.y())
        self.assertEqual(one.height(), two.height())
        self.assertNotEqual(one.x(), two.x())

    def test_vertical_tiling_forms_a_single_column(self):
        self.mgr.add2DWidget()
        self.mgr.add3DWidget()
        self._stack()
        self.model.action(WindowManager.VERTICAL_ID).trigger()
        QtWidgets.QApplication.processEvents()
        one, two = [s.geometry() for s in self.area.subWindowList()]
        self.assertFalse(one.intersects(two))
        self.assertEqual(one.x(), two.x())
        self.assertEqual(one.width(), two.width())
        self.assertNotEqual(one.y(), two.y())

    def test_directional_tiling_restores_a_minimized_window(self):
        self.mgr.add2DWidget()
        self.mgr.add3DWidget()
        minimized = self.area.subWindowList()[0]
        minimized.showMinimized()
        self.model.action(WindowManager.HORIZONTAL_ID).trigger()
        QtWidgets.QApplication.processEvents()
        self.assertFalse(minimized.isMinimized())
        one, two = [s.geometry() for s in self.area.subWindowList()]
        self.assertFalse(one.intersects(two))


if __name__ == '__main__':
    unittest.main()

# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
