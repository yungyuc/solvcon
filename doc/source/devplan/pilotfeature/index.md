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

The name itself is worth keeping.  `PilotFeature` stutters against the
package it lives in, but "feature" would be ambiguous in a mesh library
without the qualifier, and the two names that do mislead are the
`_gui_common` module and several of the subclasses.

The rest of this page is the evidence, the outside comparison, a six-stage
plan whose first two stages are mechanical and independently landable, and
an appendix naming the design patterns each stage draws on.

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

Nothing here is invented.  The shape is four named patterns put together:
a Layer Supertype[^layersupertype] for the feature layer, Template
Method[^templatemethod] for the lifecycle hooks, a
Registry[^poeaa-registry] for the controller's table, and the Interface
Segregation Principle[^isp] for the role split.  "Appendix: the patterns
behind the proposal" explains each one, says which part of the code it
lands on, and names the two patterns this proposal deliberately declines.

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

### Is `PilotFeature` a good name?

Mostly yes, and it should stay.  The naming defects in this area are next
to the class, not in it.

The case against the name is the stutter.  The class lives in
`solvcon.pilot`, so `pilot.base.PilotFeature` repeats a word the namespace
already supplies, which is the repetition Go's package-naming guidance
warns about and Python's namespaces make just as unnecessary.[^gonames]

Three things outweigh it.  First, "feature" is loaded vocabulary in a mesh
library: a feature edge, a feature angle, feature detection.  A bare
`Feature` exported from a CFD code base invites exactly that misreading,
and this class is exported (it is in `solvcon.pilot.base.__all__`).  The
qualifier is doing real work.  Second, comparable projects prefix the same
way for the same reason: Spyder's base class is `SpyderPluginV2`, KDE's is
`KXmlGuiClient`, and Qt's is `QWidget`.  Third, "feature" is honest about
what these objects are.  They are in-tree units of application
functionality that are always constructed, not units that are discovered
and loaded.

The alternatives, and why each loses:

- `Feature`.  No stutter, and it reads well as `_feature.Feature`.  Loses on
  the mesh-feature collision in the exported namespace.
- `PilotPlugin`.  Names the pattern and matches `IPlugin` and
  `SpyderPluginV2`.  Loses because it promises discovery and third-party
  loading that the pilot does not do, and a name that promises what the
  code does not deliver is worse than a vague one.
- `PilotExtension`.  A softer version of the same idea.  Loses because
  "extension" already means the `_solvcon` C++ extension module here.
- `PilotComponent`.  Neutral and makes no false promise.  Loses because it
  says nothing; "component" is the vaguest word available.
- `PilotFeatureBase`.  Explicit about being a base.  Loses because a
  `Base` suffix is noise that every base class would then have to carry.

There is one condition under which the name should change: if the stage-3
registry ever grows into out-of-tree discovery, in the napari or VS Code
direction, then `PilotPlugin` becomes the accurate name and the rename
belongs to that change, not before it.

Two renames nearby do pay, and both are about saying the role out loud.

**The module.**  `_gui_common.py` holds 2-D overlay label helpers, the
toggle-to-action bridge, the shortcut applier, the action builder, and the
base class.  "Common" tells a reader nothing about any of them, and a
module named for what it is not (not specific to anything) is where
unrelated code accumulates.[^gonames]  Split it along the seams that are
already there: `_feature.py` for the base class and the role bases,
`_action.py` for `build_action`, `apply_shortcut`, and
`ToggleActionBridge`, and `_overlay.py` for the two label helpers, whose
only callers are the canvas dialog and the tree panel.

**The subclasses.**  The feature names do not say their role, and after the
role split they should.  `Painter` is a dock panel.  `Canvas` is a menu
provider; the thing that actually is a canvas is `R2DWidget`.  `Profiling`
and `RunProfiling` are two dialogs named as a noun and a verb phrase.
`SampleMeshFeature` is the only class carrying the base class's word, and
it is one of the three that contribute no menu of their own.  The
convention worth adopting with stages 4 and 5: name a feature for its role,
`<Thing>Panel`, `<Thing>Dialog`, or `<Thing>Menu`, and reserve a plain
`<Thing>Feature` for one that only provides entries to another feature's
menu.

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

## Appendix: the patterns behind the proposal

Each pattern below is named where the proposal uses it, with what it says,
where it lands in the pilot, and what it does not solve.  The last two are
patterns this plan declines, recorded so a later reader does not have to
rediscover why.

### Layer Supertype

*A type that acts as the supertype for all types in its
layer.*[^layersupertype]  Fowler's remedy for behaviour that would
otherwise be duplicated across every object in one layer: move it into one
superclass for that layer, and keep that superclass small, because it holds
only what is common to the layer.

This is what `PilotFeature` already is, and naming it explains both its
virtue and its problem.  A Layer Supertype is defined by the layer, so the
first question it forces is what the pilot's feature layer actually is.
The answer today is four different things (F5), which is why the single
supertype holds so little: everything specific to a role had nowhere to go
but the subclasses, where it was copied.  The proposal keeps the pattern
and repairs the premise, one thin supertype for what is genuinely common
plus one per role.

The pattern also warns against the opposite failure.  A Layer Supertype
that grows past the layer's common behaviour becomes the god base class,
so `PilotFeature` should stay at roughly the size shown in "Target shape",
and anything used by only some features belongs in a role base or a
collaborator.

### Template Method

*Define the skeleton of an algorithm in a base class and let subclasses
override individual steps without changing the structure.*[^templatemethod]

The controller's build sequence is a template method spread across two
classes: the order (construct all, then populate all, then seed the
console) is fixed by the controller, and the step each feature fills in is
`populate_menu`.  The pattern's precondition is the part that is missing:
the base class must declare the steps.  A template method whose hooks are
undeclared is not a pattern, it is a convention, and finding F1 is exactly
what that costs.  Stage 2 declares `populate_menu` and `teardown` with
default bodies; stage 4 adds `build_panel` for the panel role, which is a
template method in the narrow sense, called once, lazily, by base-class
code the subclass does not touch.

### Registry

*A well-known object that other objects can use to find common objects and
services.*[^poeaa-registry]  Fowler's own caution about it is the relevant
part: a Registry is a global, and a global that can hand out anything is
how a design ends up with the Service Locator problem of F3.[^fowler-di]

Stage 3 uses the pattern in its narrow, safe form.  The registry holds
construction data (attribute name, class, dependencies) and is read by one
client, the controller, at build time.  It is not a lookup path for
features to reach each other at run time; if it became one, it would
recreate `mgr`'s problem one level up.  Cross-feature wiring stays explicit
and stays at construction, which is what Spyder's `REQUIRES` plus
`@on_plugin_available` also achieves.[^spyder]

### Interface Segregation Principle

*Clients should not be forced to depend on methods they do not
use.*[^isp]  Martin's fat-interface argument is usually applied to
interfaces; it applies to an inherited base just as directly, because a
subclass depends on everything its base offers.

`Naca4Airfoil`, at 31 lines and no menu of its own, currently inherits the
same surface as `OneDimBaseApp` at 368 lines with timers and solver
configuration.  Neither is served well.  Stages 4 and 5 segregate the two
fat roles (dock panel, file dialog) into their own bases, so a feature
depends on the lifecycle it actually has.

### Command, already in place

`QAction` is the Command pattern: a request packaged as an object, so the
menu, the toolbar, and the keyboard can all invoke the same thing without
knowing what it does.[^command]  `RMenuModel` is the container that places
those commands and looks them up by id, and `RShortcutManager` binds them
to key sequences, which is the same split Qt Creator draws between
`Command` and `ActionContainer`.[^qtc-actions]  This half of the design is
sound, and the proposal changes none of it; it is listed here so the
appendix covers the whole picture rather than only the parts under
revision.

### Not adopted: Component Configurator

*Allow an application to link and unlink component implementations at run
time without modifying, recompiling, or relinking it.*[^posa2]  This is the
pattern that a full plug-in system implements, and Qt Creator's `IPlugin`
lifecycle is a direct descendant of it.

The proposal takes the lifecycle vocabulary from that family (a declared
initialise and a declared teardown) without taking the dynamic linking.
Pilot features are in-tree, imported at start-up, and known at build time.
Adopting the full pattern would buy nothing today and would cost a
discovery mechanism, a versioning story, and a stable public API for
third-party authors.

### Not adopted: declarative manifest

napari's npe2 and VS Code's contribution points move the declaration of
what a feature contributes out of Python and into data, which is what makes
lazy activation possible.[^npe2]  It is the natural continuation of stage
3, and the stage-3 table is deliberately shaped so it could become one, but
it is not proposed now: every pilot feature is imported at start-up anyway,
so the benefit would be structural tidiness bought with a schema, a loader,
and a migration of twenty features.

## Appendix: chat history

The review was requested in one prompt: the author pointed at the
`PilotFeature` class in `solvcon/pilot/base/_gui_common.py`, said they were
not sure the design is good, and asked for online research and a devplan
report.  That produced the findings, the outside comparison, and the staged
proposal.  Everything in "Findings" is read out of the tree at that commit;
everything in "How comparable applications solve this" is from the sources
below.

Review of the first draft asked for two additions.  Name the existing
design patterns the proposed shape draws on, cite them, and explain them in
an appendix, which is "Appendix: the patterns behind the proposal" and the
pointer to it at the head of "Target shape".  And evaluate whether
`PilotFeature` is a good name, with a replacement if it is not, which is
"Is `PilotFeature` a good name?".  The verdict there is to keep the name
and rename the module and the subclasses instead.

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

[^layersupertype]: Martin Fowler, *Layer Supertype*, Patterns of Enterprise
    Application Architecture catalog.
    https://martinfowler.com/eaaCatalog/layerSupertype.html

[^poeaa-registry]: Martin Fowler, *Registry*, Patterns of Enterprise
    Application Architecture catalog.
    https://martinfowler.com/eaaCatalog/registry.html

[^templatemethod]: Gamma, Helm, Johnson, and Vlissides, *Design Patterns*,
    1994, Template Method.  Summary at
    https://refactoring.guru/design-patterns/template-method

[^command]: Gamma, Helm, Johnson, and Vlissides, *Design Patterns*, 1994,
    Command.  Summary at https://refactoring.guru/design-patterns/command

[^isp]: Robert C. Martin, *The Interface Segregation Principle*, C++
    Report, 1996.
    https://web.archive.org/web/20150905081110/http://www.objectmentor.com/resources/articles/isp.pdf

[^posa2]: Schmidt, Stal, Rohnert, and Buschmann, *Pattern-Oriented Software
    Architecture, Volume 2*, 2000, Component Configurator.
    https://www.dre.vanderbilt.edu/~schmidt/POSA/POSA2/

[^fowler-di]: Martin Fowler, *Inversion of Control Containers and the
    Dependency Injection pattern*, 2004, which introduces the Plugin
    pattern and Service Locator side by side.
    https://martinfowler.com/articles/injection.html

[^gonames]: The Go Blog, *Package names*, on avoiding repetition between a
    package name and its contents, and on `util`, `common`, and `misc` as
    package names. https://go.dev/blog/package-names

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
