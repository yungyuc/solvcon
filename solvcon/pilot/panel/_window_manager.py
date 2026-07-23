# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING


"""
Window manager feature for pilot.

Arrange the open MDI sub-windows through layout actions under the
"Window" menu, list every open sub-window there, and bring one to the
foreground when its entry is chosen.
"""

from PySide6 import QtGui

from ..base import _gui_common

__all__ = [
    'WindowManager',
]


class WindowManager(_gui_common.PilotFeature):
    """Arrange and list open MDI sub-windows under the "Window" menu.

    A static section on top holds the layout actions (Tile, Cascade)
    driving the QMdiArea built-in layouts. Below a separator follows one
    checkable action per open sub-window, labelled by its title.
    Triggering an action activates that sub-window; the active one is
    checked. The list is rebuilt, and the layout actions follow whether
    any sub-window is open, each time the menu is about to show.
    """

    #: objectName tagging every dynamic per-sub-window action.
    ITEM_ID = "window.subwindow"
    #: objectName of the tile layout action.
    TILE_ID = "window.layout.tile"
    #: objectName of the cascade layout action.
    CASCADE_ID = "window.layout.cascade"
    #: objectName of the single-row tiling action.
    HORIZONTAL_ID = "window.layout.horizontal"
    #: objectName of the single-column tiling action.
    VERTICAL_ID = "window.layout.vertical"

    def __init__(self, *args, **kw):
        super(WindowManager, self).__init__(*args, **kw)
        self._menu = None
        self._items = []
        self._layout_actions = []

    def populate_menu(self):
        """Build the static layout section and anchor the dynamic list.

        The layout actions and the separator go through the menu model
        by weight, so a later feature can slot an entry in between. The
        dynamic list is seeded right away: a native menu bar hides an
        empty menu, and a hidden menu can never fire aboutToShow to fill
        itself.
        """
        self._menu = self._mgr.menu_model.menu("Window")

        self._layout_actions = [
            self.add_action(
                "Window", "Tile",
                "Tile the open sub-windows to fill the area",
                self._tile, id=self.TILE_ID, weight=10),
            self.add_action(
                "Window", "Cascade",
                "Stack the open sub-windows with an offset",
                self._cascade, id=self.CASCADE_ID, weight=11),
            self.add_action(
                "Window", "Tile Horizontally",
                "Arrange the open sub-windows in a single row",
                self._tile_horizontal, id=self.HORIZONTAL_ID, weight=12),
            self.add_action(
                "Window", "Tile Vertically",
                "Arrange the open sub-windows in a single column",
                self._tile_vertical, id=self.VERTICAL_ID, weight=13),
        ]
        self._mgr.menu_model.place_separator("Window", weight=30)

        self._menu.aboutToShow.connect(self._rebuild)
        self._rebuild()

    def _tile(self):
        self._mgr.mdiArea.tileSubWindows()

    def _cascade(self):
        self._mgr.mdiArea.cascadeSubWindows()

    def _tile_horizontal(self):
        self._arrange(horizontal=True)

    def _tile_vertical(self):
        self._arrange(horizontal=False)

    def _arrange(self, horizontal):
        """Line the visible sub-windows up along one direction.

        Each gets an equal share of the MDI viewport: side by side over
        the full height when ``horizontal``, stacked over the full width
        otherwise. QMdiArea has no directional counterpart to
        tileSubWindows, so the geometry is dealt out by hand. A
        minimized or maximized sub-window is restored first, else the
        new geometry would not take effect.
        """
        mdi = self._mgr.mdiArea
        subwins = [s for s in mdi.subWindowList() if s.isVisible()]
        if not subwins:
            return

        area = mdi.contentsRect()
        width, height = area.width(), area.height()
        if horizontal:
            width //= len(subwins)
        else:
            height //= len(subwins)

        for index, subwin in enumerate(subwins):
            if subwin.isMinimized() or subwin.isMaximized():
                subwin.showNormal()
            x = area.x() + index * width if horizontal else area.x()
            y = area.y() if horizontal else area.y() + index * height
            subwin.setGeometry(x, y, width, height)

    def _rebuild(self):
        """Refresh the sub-window list to match the MDI area.

        Drop the actions from the previous show, then append one checkable
        action per visible sub-window in area order, checking the active
        one. A disabled placeholder is shown when none are open, and the
        layout actions are enabled only when a sub-window is open.
        """
        for act in self._items:
            self._menu.removeAction(act)
            act.deleteLater()
        self._items = []

        mdi = self._mgr.mdiArea
        active = mdi.activeSubWindow()
        subwins = [s for s in mdi.subWindowList() if s.isVisible()]

        for act in self._layout_actions:
            act.setEnabled(bool(subwins))

        if not subwins:
            self._append_placeholder()
            return

        for index, subwin in enumerate(subwins):
            self._append_item(index, subwin, subwin is active)

    def _append_item(self, index, subwin, is_active):
        """Append one checkable action that activates ``subwin``."""
        title = subwin.windowTitle() or "window"
        act = QtGui.QAction("%s" % (title), self._menu)
        act.setObjectName(self.ITEM_ID)
        act.setStatusTip("Bring '%s' to the foreground" % title)
        act.setCheckable(True)
        act.setChecked(is_active)
        act.triggered.connect(
            lambda checked=False, s=subwin: self._activate(s))
        self._menu.addAction(act)
        self._items.append(act)

    def _append_placeholder(self):
        """Append a disabled hint when no sub-window is open."""
        act = QtGui.QAction("(No open windows)", self._menu)
        act.setEnabled(False)
        self._menu.addAction(act)
        self._items.append(act)

    def _activate(self, subwin):
        """Bring ``subwin`` to the foreground, restoring if minimized."""
        if subwin.isMinimized():
            subwin.showNormal()
        self._mgr.mdiArea.setActiveSubWindow(subwin)


# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
