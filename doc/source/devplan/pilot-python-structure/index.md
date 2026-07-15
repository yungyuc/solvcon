# Reorganize the pilot Python file structure

Move the pilot GUI Python package (`solvcon/pilot/`) from a flat directory of
loose modules to a hierarchical layout, so that the fast-growing feature set
gets a home that tells a reader where each piece belongs.

This is a design proposal for review. The submodule boundaries below are a
first guess drawn from the current code and the direction of recent pilot
work; they are meant to be argued about before any file moves.

## Problem

The pilot Python package started small and grew fast. It now holds about
twenty modules in one directory, dominated by `_base_app.py` at over a
thousand lines. It is flat: every module sits beside every other, and the only
structure is the file name. A reader opening `solvcon/pilot/` cannot tell the
application shell from the 1D solvers from the 2D canvas from the dock panels
without reading each module.

The growth is not slowing. Open pilot issues each add code that lands in the
same flat directory:

- SVG export from the 2D canvas (`_svg_gui`, `_canvas_gui`),
- the controlling agent (`_agent_gui`),
- window-geometry persistence and dock management (`_window_manager`,
  `_tree_panel`),
- more 1D CESE solver demos beside the existing burgers, euler, linear-wave,
  and oblique-shock views.

Each of these has a natural neighborhood. A flat directory hides those
neighborhoods, so every new feature widens the same pile instead of deepening
a clear module. The goal of this plan is to name the neighborhoods and give
each one a directory before the next wave of features arrives.

## Proposed structure

The proposal groups modules by the neighborhood they serve. The `airfoil/`
subpackage that already exists is the model this plan generalizes.

### Target tree

```text
solvcon/pilot/
  __init__.py                   package entry, re-exports (unchanged location)
  _pilot_core.py                C++ extension shim (unchanged location)
  base/
    _base_app.py
    _gui.py
    _gui_common.py
    _theme.py
  onedim/
    _burgers1d.py
    _euler1d.py
    _linear_wave.py
    _oblique.py
  canvas/
    _canvas_gui.py
    _painter_gui.py
    _svg_gui.py
  mesh/
    _mesh.py
  panel/
    _tree_panel.py
    _window_manager.py
    _profiling.py
  agent/
    _agent_gui.py
  airfoil/                      unchanged (already a subpackage)
```

Rationale for the boundaries:

- **`__init__.py`** and **`_pilot_core.py`** stay at the package root so the
  public import surface (`solvcon.pilot.enable`, `RDomainWidget`, ...) and the
  extension shim do not move.
- **`base/`** is the base application layer: the oversized `_base_app.py`, the
  launch and controller wiring in `_gui`, the shared GUI helpers, and the theme
  shim.
- **`onedim/`** collects the 1D CESE demo solvers, a family with no business
  sitting beside the mesh or the canvas, and the place new demos land.
- **`canvas/`** is the 2D drawing surface that SVG export extends.
- **`mesh/`** holds the mesh view and its helpers.
- **`panel/`** holds the dock widgets: the entity tree, the window manager,
  and the profiling view.
- **`agent/`** holds the controlling agent GUI.

### File-to-home summary

| Neighborhood | Module home |
| --- | --- |
| package entry and extension shim | root (`__init__`, `_pilot_core`) |
| base application layer | `base/` |
| 1D solvers | `onedim/` |
| 2D canvas | `canvas/` |
| mesh | `mesh/` |
| docks / panels | `panel/` |
| agent | `agent/` |
| airfoil | `airfoil/` (unchanged) |

## Planned steps

The move is carried out in small, independently reviewable steps, each one
keeping the package importable and the public surface unchanged. The order is
island-first: the least-connected neighborhoods move before the hub, so the
risky import changes land last against a tree that is already mostly sorted.
The mechanics for each step are in the Implementation section.

1. Land this development plan (this page).
   *Estimated diff: documentation only.*
2. **`onedim/` and `agent/`.** Nearly self-contained, moved first.
   *Estimated diff: ~35 lines: five moved modules, two new `__init__.py`
   re-export files, their import sites, and two `setup.py` entries.*
3. **`canvas/`, `mesh/`, and `panel/`.** The feature neighborhoods.
   *Estimated diff: ~50 lines: seven moved modules, three new `__init__.py`
   files, their import sites, and three `setup.py` entries.*
4. **`base/`.** Last, because it holds `_base_app` and the hub wiring;
   `__init__.py` and `_pilot_core.py` stay at the package root throughout.
   *Estimated diff: ~45 lines: four moved modules that are widely imported, one
   new `__init__.py`, and the `setup.py` entry.*
5. **Final sweep.** Run `make flake8` and the pilot tests, rebuild the docs,
   and confirm each step's diff is renames plus import edits only.
   *Estimated diff: under 10 lines of import-order fixups.*

## Where the code lives today

About twenty modules, flat, dominated by `_base_app.py` (over 1000 lines).
The package entry (`__init__.py`) imports `_pilot_core` first for the C++
extension, then the GUI modules behind the `enable` flag. The families are
already visible in the file names even though the directory is flat:

- The application shell is `_base_app`, `_gui`, `_gui_common`, and `_theme`.
- The 1D solver demos are `_burgers1d`, `_euler1d`, `_linear_wave`, and
  `_oblique`, a clear family.
- The 2D drawing modules are `_canvas_gui`, `_painter_gui`, and `_svg_gui`.
- The dock widgets are `_tree_panel`, `_window_manager`, and `_profiling`.
- `_mesh` and `_agent_gui` each stand largely on their own.

`airfoil/` is already a subpackage and shows the target shape: a directory
with an `__init__.py` that re-exports its public names.

## Implementation

The reorganization is mechanical but wide. The step sequence is in Planned
steps above; the mechanics for one neighborhood are below.

For each subpackage:

1. `git mv` the modules into the new directory and add an `__init__.py` that
   re-exports the public names.
2. Fix the relative imports. Intra-package imports gain a level
   (`from ._mesh import ...` becomes `from ..mesh._mesh import ...`, or a
   re-export flattens it).
3. Keep `solvcon/pilot/__init__.py` re-exporting the same public symbols so
   `from solvcon.pilot import RDomainWidget` and friends keep working. The
   public surface must not change.
4. Update `setup.py` `packages` to list the new subpackages.
5. Run the pilot tests (`make run_pilot_pytest`) and a headless smoke launch
   under `QT_QPA_PLATFORM=offscreen`.

## Verification

- The package imports clean after each neighborhood moves, and
  `make run_pilot_pytest` stays green.
- `make flake8` passes; the 79-character limit and import ordering are the
  checks most affected by the moves.
- A headless launch under `QT_QPA_PLATFORM=offscreen` opens the main window,
  a 1D solver view, and the 2D canvas, confirming the moved modules still
  import and wire together.
- `git diff --stat` on each step is dominated by renames plus import edits,
  with no logic changes; the review can lean on `git log --follow`.

## Out of scope

- **No behavior change.** This is a pure move-and-rewire. Splitting the
  oversized `_base_app.py` into smaller modules is a tempting neighbor but
  belongs in its own issue; doing it here would hide real logic changes inside
  a rename diff.
- **No public API change.** The `solvcon.pilot` import surface stays identical.
- **No new features.** The open pilot issues motivate the boundaries but are
  implemented separately, on top of the new tree.

## Open questions for review

1. **`base/` versus `panel/`.** `_profiling`, `_tree_panel`, and
   `_window_manager` are docks; is `panel/` the right name, or should they sit
   in `base/`?
2. **Theme placement.** `_theme` is a single small module. Does it belong in
   `base/`, or in its own `theme/` subpackage?

## Decisions from review

- **Base naming.** The application-core subpackage is named `base/`, not
  `app/`: the whole pilot is the app, so `base/` reads as the base layer the
  feature neighborhoods build on.
- **Document order.** The proposed structure leads, the planned steps follow,
  and the code survey that backs the boundaries comes after them, so a
  reviewer sees the target and the roadmap before the supporting analysis.

## Delivery status

- Branch: `plan/pilot-structure` on the personal fork.
- Contents: this development-plan page only. No file moves yet; the moves
  begin once the submodule boundaries are agreed.
- CI: draft pull request opened on the fork for review.

## Appendix: chat history

- "Propose a hierarchical structure for the pilot, guessing the submodules
  from the current code and recent issues." Drove the code survey (the flat
  inventory and the module families) and the submodule proposal above.
- "Lead with the structure and the step roadmap." Reordered the page so the
  proposal and the step roadmap lead, with the code survey below them.
- "Focus this plan on the Python package." Scoped the page to
  `solvcon/pilot/`.
- Review on the pull request: "Now everything is an app. Rename `app` to
  `base`." Renamed the application-core subpackage from `app/` to `base/` and
  recorded the decision.
- "Add a diff-line estimate in each of the steps." Added an estimated diff
  size to every planned step.

<!-- vim: set ft=markdown ff=unix fenc=utf8 et sw=2 ts=2 sts=2 tw=79: -->
