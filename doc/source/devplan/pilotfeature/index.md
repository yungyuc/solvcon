# Review of the pilot feature base class

`solvcon.pilot.base._gui_common.PilotFeature` is the base class every pilot
GUI feature derives from.  It was added in January 2025 as "a house for a
feature that is being prototyped in pilot" and served three derived classes.
It now serves twenty.  This page asks whether the class still earns its
shape, compares it with the feature and plug-in layers of comparable
desktop applications, and proposes a staged redesign.

## Summary

The class works, and nothing here is a bug report.  The concern is that
`PilotFeature` has stopped growing with the code it hosts.  It is a
convenience base (it stores one attribute and exposes three helpers) that
sits under twenty classes playing four different roles, while the real
contract that binds those classes together, the `populate_menu` hook and
the build order, lives in the controller rather than in the base class.

Three conclusions:

1. **Keep a base class, but give it a contract.**  The lifecycle hook
   (`populate_menu`) and the teardown that the C++ side already supports
   (`RManager::reset` clears the menu bar) belong in `PilotFeature`, not in
   an unwritten convention that fourteen of seventeen subclasses happen to
   follow.
2. **Split the roles.**  A dock panel, a file dialog, a menu-only mesh
   provider, and a 1-D sub-application do not want the same base.  The
   panel role alone is copied verbatim in four features; the dialog role in
   five.  That duplication is the strongest single argument that the base
   class is under-specified.
3. **Replace the hand-written controller wiring with a registry.**  Adding
   a feature today means editing `_gui.py` in four places, in an order that
   is load-bearing and undocumented.

The rest of this page is the evidence, the outside comparison, and a
six-stage plan whose first three stages are mechanical and independently
landable.

## What the class is today

### The code

`PilotFeature` (69 lines) derives from `QtCore.QObject` and offers:

- `__init__(self, *args, **kw)`, which pops `mgr` out of the keyword
  arguments, checks it with `isinstance` against the C++ `RManager`, stores
  it as `self._mgr`, and forwards the rest to `QObject`.
- `_pycon` and `_mainWindow`, two read-only properties that forward to
  `self._mgr.pycon` and `self._mgr.mainWindow`.
- `add_action(path, text, tip, func, *, id, weight=50, ...)`, a thin
  wrapper over the module-level `build_action` plus
  `RMenuModel::place`.
- `_add_menu_item(menu, text, tip, func)`, a legacy helper with no callers.

Everything else a feature does, it does by reaching through `self._mgr`.

### The origin

The introducing commit describes the intent plainly: a house for prototype
features, serving `SampleMesh`, `GmshFileDialog`, and `Naca4Airfoil`.  For
three prototypes, a base class that stores the manager and adds a menu item
is exactly the right size.  The question this page answers is whether the
same shape holds at twenty.

### The inventory

Seventeen classes derive from `PilotFeature` directly; three more
(`Euler1DApp`, `Burgers1DApp`, `LinearWave1DApp`) derive from
`OneDimBaseApp`, which is itself a `PilotFeature`.

| Class | Role | `populate_menu` | `_mgr` | `add_action` | lines |
| --- | --- | --- | --- | --- | --- |
| `SampleMeshFeature` | mesh provider | no | yes | no | 61 |
| `SampleMeshDialog` | dialog | yes | no | yes | 79 |
| `GmshFileDialog` | dialog | yes | yes | yes | 70 |
| `SVGFileDialog` | dialog | yes | yes | yes | 81 |
| `Save2DCanvasDialog` | dialog | yes | yes | yes | 152 |
| `TreePanel` | dock panel | yes | yes | yes | 102 |
| `SolutionInfo` | dock panel | yes | yes | yes | 187 |
| `AgentPanel` | dock panel | yes | yes | yes | 145 |
| `Painter` | dock panel | yes | yes | yes | 115 |
| `Profiling` | dialog + panel | yes | yes | yes | 101 |
| `RunProfiling` | dialog + panel | yes | yes | yes | 148 |
| `Canvas` | menu provider | yes | yes | yes | 213 |
| `ThemeMenu` | menu provider | yes | yes | no | 59 |
| `WindowManager` | menu provider | yes | yes | no | 79 |
| `ObliqueShockMesh` | mesh provider | no | yes | no | 44 |
| `Naca4Airfoil` | mesh provider | no | yes | no | 31 |
| `OneDimBaseApp` | sub-application | yes | yes | no | 368 |

Fourteen of seventeen define `populate_menu`.  Sixteen touch `self._mgr`.
Eleven use `add_action`; the other six place actions by other means.

### The wiring

`_Controller` in `solvcon/pilot/base/_gui.py` owns the whole lifecycle by
hand:

```python
class _Controller(metaclass=_Singleton):
    def __init__(self):
        self.mesh_sample_dialog = None
        self.gmsh_dialog = None
        # ... 18 more attributes, all None

    def build(self, name="pilot", size=(1000, 600)):
        # ... 20 constructor calls, in a load-bearing order
        self.tree_panel = _tree_panel.TreePanel(
            mgr=self._rmgr, style_status=self.mesh_style_status)
        self.solution_info = _solution_info.SolutionInfo(mgr=self._rmgr)
        self.solution_info.viewer_updated = self.tree_panel.resync
        # ...
        self.populate_menu()

    def populate_menu(self):
        self.gmsh_dialog.populate_menu()
        # ... 16 more calls, in yet another order
```

Adding a feature means an import, an attribute in `__init__`, a
construction in `build`, and a call in `populate_menu`.

## Findings

### F1.  The lifecycle contract is not in the base class

`populate_menu` is the de-facto lifecycle hook: the controller calls it on
seventeen objects after building all of them.  It is declared nowhere.  It
is not in `PilotFeature`, not an abstract method, not a documented
protocol.  A subclass that misspells it gets no menu entry and no error,
and a reader of `PilotFeature` cannot learn that the hook exists.

There is also no teardown hook, even though the C++ side supports a reset:
`RManager::reset()` calls `RMenuModel::clear()` so a repeated `setUp` on
the singleton starts from a clean bar.  Python features that placed actions
have no chance to drop their own state in that window; the controller's
`_built` flag simply refuses to run twice.

### F2.  The constructor signature hides its own parameter

```python
def __init__(self, *args, **kw):
    self._mgr = kw.pop('mgr')
    if not isinstance(self._mgr, _pcore.RManager):
        raise TypeError(...)
    super(PilotFeature, self).__init__(*args, **kw)
```

The one required parameter does not appear in the signature.  Tooling,
`help()`, and Sphinx all show `(*args, **kw)`.  A missing `mgr` raises
`KeyError` rather than the usual `TypeError` for a missing argument.  Every
subclass repeats the `*args, **kw` boilerplate to pass it through, and the
attribute is assigned before `super().__init__`, which works under PySide
but inverts the usual order.

An explicit `def __init__(self, mgr, *, parent=None)` says the same thing
in one line and costs nothing.

### F3.  `mgr` is a service locator

Features do not receive what they need; they receive an object from which
everything is reachable: `pycon`, `pyterm`, `mainWindow`, `mdiArea`,
`menu_model`, `themeManager`, `shortcutManager`, `add3DWidget`,
`add2DWidget`, `currentR2DWidget`, `addSubWindow`, `quit`.  The base class
then re-exports two of those paths as `_pycon` and `_mainWindow`, which
encourages the habit.

This is the service-locator pattern, and the standard criticism applies
here in a concrete form: a feature's real dependencies are invisible at its
constructor, so nothing can be substituted.[^ploeh][^baeldung]  The
`isinstance` check makes the substitution impossible rather than merely
awkward: a feature cannot be constructed against a stub manager at all, so
feature logic is only reachable from a test that has stood up a real Qt
application.  For a project whose own guidance is to prefer Python tests,
that is a real cost.

Two smaller points in the same area.  The two accessors are camelCase
(`_mainWindow`) in a PEP-8 code base, and they are underscore-prefixed
members consumed by subclasses in six other sub-packages, so "protected"
here means "public with a warning sign".  And the access path is not even
consistent: three of the four dock panels reach the window as
`self._mgr.mainWindow`, one as `self._mainWindow`.

### F4.  The base is inherited for reuse, not for an is-a relation

What a subclass gains by deriving is one stored attribute, two forwarding
properties, and one menu helper.  That is delegation wearing an inheritance
costume, the case Fowler's *Replace Superclass with Delegate* is written
for.[^fowler]  The tell is in the code base itself: `MeshStyleStatus` needs
the same manager and defines the same `populate_menu`, yet derives from
`QObject` directly and re-implements the `kw.pop('mgr')` line, because it
wants a Qt signal and does not want the rest.  When a class that fits the
concept opts out, the base class is not carrying the concept.

The `QObject` base is also only half used.  Four dialog features want it
for `@QtCore.Slot()` on their `on_finished` callbacks, which is a genuine
reason to keep it.  But no `PilotFeature` subclass declares a signal of its
own; cross-feature notification is done with plain callable attributes
assigned from outside (`solution_info.viewer_updated = tree_panel.resync`,
`self._mesh_tree.boundary_toggled = ...`).  Plain callbacks are a defensible
choice, and they are easy to test.  The problem is that the code base makes
both choices at once without saying which is the convention.

### F5.  One base class, four roles, and the duplication that follows

The dock-panel role is copied four times:

```python
# TreePanel, SolutionInfo, AgentPanel, Painter, near-identical
def populate_menu(self):
    self._action = self.add_action(
        "View/Panels", ..., None, id=..., checkable=True)
    self._action.toggled.connect(self._on_toggled)

def _on_toggled(self, checked):
    if checked:
        self._ensure_panel()
        self._dock.show()
    elif self._dock is not None:
        self._dock.hide()

def _ensure_panel(self):
    if self._panel is not None:
        return
    ...
    self._dock = QDockWidget(...)
    self._mgr.mainWindow.addDockWidget(Qt.LeftDockWidgetArea, self._dock)
    self._dock.visibilityChanged.connect(self._action.setChecked)
```

The file-dialog role is copied five times: a `QFileDialog` member, a `run`
that shows it, a `@Slot() on_finished` that reads `selectedFiles`, and a
static `_get_initial_path`.

This is the clearest signal in the review.  A base class exists to absorb
exactly this kind of repetition, and `PilotFeature` absorbs none of it,
because it was specified for the smallest common denominator of three
prototypes rather than for the roles that actually emerged.

At the other end of the range, `OneDimBaseApp` is a 368-line
sub-application with solver configuration, timers, and plot layouts, and it
sits on the same base as the 31-line `Naca4Airfoil`.  A base class that
spans that range says very little about either.

### F6.  Registration is manual, ordered, and triplicated

Twenty attributes initialised to `None`, twenty constructions, seventeen
`populate_menu` calls, in two different hand-maintained orders.  The order
matters: `SampleMeshDialog` needs the three mesh providers already built so
it can collect their dialog entries, `TreePanel` needs `MeshStyleStatus`,
`Canvas` needs `Painter`, and `SolutionInfo` is wired to `TreePanel` by
attribute assignment after both exist.  None of this is declared, checked,
or discoverable from the features themselves.

### F7.  Dead and diverging menu paths

`_add_menu_item` has no callers.  It calls `menu.addAction` directly, so it
bypasses `RMenuModel`, and it builds an action with no id, so the item gets
no keymap binding, no shortcut, and no toggle store binding.  It is a
working path to the wrong place.

`MeshStyleStatus.populate_menu` takes that path today: it builds its three
`QAction` objects by hand and calls `submenu.addAction`, so its items sit
outside the id vocabulary and outside the weight ordering.

### F8.  The headless build binds the name to `None`

`solvcon/pilot/base/__init__.py` sets `PilotFeature = None` when the GUI is
disabled.  Any module that subclasses it at import time in a no-GUI build
raises `TypeError: NoneType is not an acceptable base type`.  Today that is
safe only because every feature module is itself behind the same `enable`
guard.  It is a fragile arrangement to hand to a new feature author; an
empty stub base class would fail more gracefully.

### F9.  What is already good

The placement layer is not the problem, and a redesign should not disturb
it.  `RMenuModel` addresses menus by slash-separated path with integer
weights and gaps (the convention contributed-item systems converge on),
registers actions by `objectName` for later lookup and removal, and owns
groups and separators.  `build_action` resolves a command id through
`RShortcutManager` for the platform binding and menu role, and
`ToggleActionBridge` gives a checkable action a two-way binding to the
`Toggle` store.  That is a competent equivalent of an action manager.  The
gap is on the other side: there is no equivalent of the plug-in object that
contributes to it.

## How comparable applications solve this

### Qt Creator: a lifecycle interface plus an action manager

Qt Creator splits the two concerns solvcon has merged into the controller.
`ExtensionSystem::IPlugin` defines the lifecycle: `initialize` (set up
internal state and export interfaces, called in dependency order),
`extensionsInitialized` (finish the parts other plug-ins may have extended,
called in reverse order), `delayedInitialize` (after the event loop
starts), and `aboutToShutdown`.[^qtc-lifecycle]  Dependencies declared in
plug-in metadata determine the load queue, so ordering is derived rather
than hand-maintained.

`Core::ActionManager` holds `Command` objects registered under stable ids
and `ActionContainer` objects for menus, and the documentation's advice is
to register actions in `initialize` so dependent plug-ins can find
them.[^qtc-actions]  This is close to what `RMenuModel` plus the keymap
already do in solvcon.

The lesson: solvcon has the action manager and is missing the
`IPlugin`-shaped half.

### Spyder: a plug-in registry with declared dependencies

Spyder 5.1 replaced a single `register` method with a registry.  A plug-in
declares `REQUIRES` and `OPTIONAL` class constants, does its own setup in
`on_initialize`, and reacts to other plug-ins with
`@on_plugin_available(plugin=...)` and `@on_plugin_teardown`.[^spyder] The
documented motivation is exactly the situation in `_gui.py`: initialise
yourself first, coordinate with a dependency only when that dependency is
present, and make the relationship explicit instead of implicit in call
order.

The lesson: the `solution_info.viewer_updated = tree_panel.resync` line and
the load-bearing build order are the two symptoms this design removes.

### KDE KXMLGUI: actions in code, layout in data

KXMLGUI keeps actions in a `KActionCollection` and the menu layout in an
XML document, and merges the documents of parent and child clients so a
part or plug-in extends the host's menus without the host knowing.[^kxmlgui]

The lesson: solvcon already separates "what the action is" from "where it
goes" through path and weight.  What it does not yet have is a per-feature
declaration of its contributions as data.

### napari and VS Code: static manifests and lazy activation

napari's second-generation plug-in engine replaced import-and-scan
discovery with a static manifest of contributions, so the application can
show a plug-in's commands and menu items before importing any of its code,
and import only on first use.  The design is explicitly modelled on VS
Code's contribution points.[^npe2]

The lesson: this is the destination, not the next step.  It matters for a
third-party plug-in ecosystem and for start-up time; solvcon's features are
in-tree and its cost today is maintenance, not import time.  Worth keeping
in view so intermediate steps do not close the door.

### The general literature

- Passing a locator that can produce anything hides a component's real
  dependencies and pushes failures to run time.[^ploeh][^baeldung]
- When a subclass uses only part of a superclass and inherits for reuse,
  replace the superclass with a delegate.[^fowler]
- For a contract, an abstract base class enforces at instantiation time and
  a `Protocol` checks structurally without forcing
  inheritance.[^pep544][^realpython]  Both fit here, for different parts:
  an ABC for classes that opt into the feature lifecycle, a `Protocol` for
  ones like `MeshStyleStatus` that only need to be callable by the
  controller.

### Mapping

| Mechanism | Elsewhere | In solvcon today |
| --- | --- | --- |
| Action registry by id | `ActionManager`, `KActionCollection` | `RMenuModel` |
| Menu placement | `ActionContainer`, XMLGUI merge | path plus weight |
| Lifecycle hooks | `IPlugin`, `on_initialize` | `populate_menu`, undeclared |
| Dependency order | metadata, `REQUIRES` | order of lines in `build()` |
| Teardown | `aboutToShutdown`, `on_plugin_teardown` | none in Python |
| Discovery | plug-in scan, static manifest | 20 hand-written attributes |
| Role-specific bases | panel, dialog, editor bases | one base for all roles |

## Proposal

The target is a base class that states the contract, a small set of
role-specific bases that absorb the repeated code, and a registry that
replaces the hand-written controller lists.  Each stage stands alone and
keeps the tree green.

### Target shape

```python
class PilotFeature(QtCore.QObject):
    """A pilot feature: a unit of GUI contributed to the main window."""

    def __init__(self, mgr, *, parent=None):
        super().__init__(parent)
        self._mgr = mgr

    @property
    def mgr(self):
        return self._mgr

    @property
    def main_window(self):
        return self._mgr.mainWindow

    @property
    def console(self):
        return self._mgr.pycon

    def populate_menu(self):
        """Place this feature's menu entries.  The default places none."""

    def teardown(self):
        """Drop the entries this feature placed.  Reverses populate_menu."""
        for oid in self._placed_ids:
            self._mgr.menu_model.remove(oid)
        self._placed_ids.clear()


class PanelFeature(PilotFeature):
    """A feature whose UI is one dock widget toggled from View/Panels."""

    def add_panel_toggle(self, text, tip, *, id, weight=50):
        ...   # the twenty lines the four panels repeat today

    def build_panel(self):
        """Build and return the dock's widget.  Called once, lazily."""
        raise NotImplementedError
```

`add_action` records each id it places, which makes `teardown` free and
gives the C++ `reset` path a Python counterpart.

### Stages

**Stage 1, the signature.**  Explicit `__init__(self, mgr, *, parent=None)`,
a public `mgr` property, PEP-8 accessor names with the old ones kept as
aliases for one cycle, and `_add_menu_item` deleted.  Low risk, mechanical,
around 120 touched lines across twenty files.

**Stage 2, the contract.**  Declare `populate_menu` and `teardown` on the
base with default bodies, and track placed ids in `add_action` so
`teardown` needs no per-feature code.  Low risk, around 60 lines.

**Stage 3, the registry.**  A declared table of (attribute, class,
dependencies); the controller builds and populates from it in dependency
order.  Medium risk, around 150 lines, most of them deleted from `_gui.py`.

**Stage 4, the panel role.**  Add `PanelFeature` and move the four dock
panels onto it.  Medium risk, net negative line count.

**Stage 5, the dialog role.**  Add `DialogFeature` and move the five file
dialogs onto it.  Medium risk, net negative line count.

**Stage 6, testability.**  Relax the `isinstance` check to a documented
protocol so a feature can be exercised against a stub manager, and add
headless tests for the two role bases.  Medium risk, around 150 lines.

Stages 1 and 2 are worth doing whatever is decided about the rest: they
cost little and they write down the contract that already exists.  Stage 3
is where the maintenance win is.  Stages 4 and 5 are where the line count
goes down.  Stage 6 is the one that changes what can be tested, and it is
the one to argue about, because it trades a strict run-time check for
reach.

`OneDimBaseApp` is deliberately left alone until stage 4 exists; it is the
one subclass large enough to deserve its own decision.

### Alternatives considered

**Delete the base class and use plain classes with an explicit `mgr`
argument.**  Honest, and it would make the delegation visible.  Rejected
because the roles do share real behaviour once stages 4 and 5 exist, and
because `@Slot` needs a `QObject` somewhere.

**Go straight to a declarative manifest.**  It is where napari and VS Code
landed, and it would subsume stages 3 to 5.  Rejected as a first step: it
is a large change for in-tree features that are all imported anyway, and
the registry in stage 3 is a compatible intermediate point.

**Dynamic plug-in loading (entry points, external packages).**  Out of
scope.  Nothing in the current code base asks for third-party pilot
features, and the cost is high.

**Leave it as it is.**  Defensible while the pilot is a prototype bench,
which is what the class was built for.  The argument against is the
duplication in F5: four panels and five dialogs of copied structure is
already more code than the stages that would remove it.

## Out of scope

- The C++ side.  `RMenuModel`, `RShortcutManager`, and the keymap are
  sound and this plan does not touch them.
- The `Toggle` store and `ToggleActionBridge`.
- Menu content, ordering, and shortcuts.  A redesign that changes what the
  menu bar looks like has failed.
- `_pilot_core` and the `enable` guard, beyond the note in F8.

## Verification

Every stage keeps the existing pilot tests as the contract, in particular
`tests/test_pilot_bar.py`, which builds the whole bar through
`_gui.controller.build()`, and the per-feature tests for the theme menu,
tree panel, solution info, window menu, and shortcuts.  The bar must come
out identical: same ids, same paths, same weights, same shortcuts.  Stages
4 and 5 add headless tests for the new role bases under
`QT_QPA_PLATFORM=offscreen`; stage 6 adds the first tests that construct a
feature without a live manager.

## Status

Report only.  No code change is proposed for review yet; this page is the
argument for the staged plan above, to be accepted, amended, or declined
before any stage is started.

## Appendix: chat history

The review was requested in one prompt: the author pointed at the
`PilotFeature` class in `solvcon/pilot/base/_gui_common.py`, said they were
not sure the design is good, and asked for online research and a devplan
report.  Everything in "Findings" is read out of the tree at that commit;
everything in "How comparable applications solve this" is from the sources
below.

[^qtc-lifecycle]: Qt Creator, *Plugin Life Cycle*, Extending Qt Creator
    Manual. https://doc.qt.io/qtcreator-extending/plugin-lifecycle.html

[^qtc-actions]: Qt Creator, *The Action Manager and Commands*, Extending Qt
    Creator Manual. https://doc.qt.io/qtcreator-extending/actionmanager.html

[^spyder]: Spyder, *New mechanism to register plugins in Spyder 5.1.0*.
    https://github.com/spyder-ide/spyder/wiki/New-mechanism-to-register-plugins-in-Spyder-5.1.0

[^kxmlgui]: KDE TechBase, *XMLGUI Technology*, and the KXmlGui API
    reference. https://api.kde.org/kxmlgui-module.html

[^npe2]: napari, *Migration to npe2* and the npe2 manifest specification.
    https://napari.org/dev/plugins/advanced_topics/npe2_migration_guide.html

[^ploeh]: Mark Seemann, *Service Locator is an Anti-Pattern*, 2010.
    https://blog.ploeh.dk/2010/02/03/ServiceLocatorisanAnti-Pattern/

[^baeldung]: Baeldung on Computer Science, *Dependency Injection vs.
    Service Locator*. https://www.baeldung.com/cs/dependency-injection-vs-service-locator

[^fowler]: Martin Fowler, *Replace Superclass with Delegate*, Refactoring
    catalog. https://refactoring.com/catalog/replaceSuperclassWithDelegate.html

[^pep544]: PEP 544, *Protocols: Structural subtyping (static duck typing)*.
    https://peps.python.org/pep-0544/

[^realpython]: Real Python, *Python Protocols: Leveraging Structural
    Subtyping*. https://realpython.com/python-protocol/

<!-- vim: set ft=markdown ff=unix fenc=utf8 et sw=2 ts=2 sts=2 tw=79: -->
