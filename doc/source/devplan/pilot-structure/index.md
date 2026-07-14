# Reorganize the pilot file structure

Move the pilot GUI from a flat directory of loose files to a hierarchical
layout, in both the C++ core (`cpp/solvcon/pilot/`) and the Python package
(`solvcon/pilot/`), so that the fast-growing feature set gets a home that
tells a reader where each piece belongs.

This is a design proposal for review. The submodule boundaries below are a
first guess drawn from the current code and the direction of recent pilot
work; they are meant to be argued about before any file moves.

## Problem

The pilot subsystem started small and grew fast. The C++ side now holds about
forty classes in one directory, and the Python side about twenty modules in
another. Both are flat: every file sits beside every other, and the only
structure is the file name. A reader opening `cpp/solvcon/pilot/` cannot tell
the 3D scene from the 2D canvas from the theme backends from the Python
console without reading each header.

The growth is not slowing. Open pilot issues each add code that lands in the
same flat directory:

- keyboard-shortcut system (menus, actions, keymap),
- SVG export and a native XY plot (the 2D canvas),
- window-geometry persistence and terminal I/O (the console and app shell),
- cell picking, vector visuals, 3D boundary surfaces (the scene and its
  drawables),
- the controlling agent.

Each of these has a natural neighborhood. A flat directory hides those
neighborhoods, so every new feature widens the same pile instead of deepening
a clear module. The goal of this plan is to name the neighborhoods and give
each one a directory before the next wave of features arrives.

## Where the code lives today

### C++ core: `cpp/solvcon/pilot/`

About forty translation units plus a `shaders/` directory, all flat. A quick
scan of the intra-pilot includes shows the coupling is already clustered even
though the files are not:

- `common_detail.hpp` is the shared base, pulled in by roughly every source
  (about 25 include sites). It is a genuine common header.
- `RDrawable.hpp` roots the visual primitives (about 10 include sites): the
  boundary, feature edges, field, scalar field, segments, and normals all
  derive from or lean on it.
- The theme files (`theme`, `RThemeManager`, `RThemeBackend`, and the three
  per-platform backends) form a closed island.
- The Python-console files (`RPythonConsoleDockWidget`,
  `RPythonConsoleHistory`, `RPythonSyntaxHighlighter`, `RPythonSyntaxRules`)
  form another closed island.
- The 2D canvas (`R2DWidget`, `RWorldRenderer2d`, `DrawTool`, `RTextOverlay`)
  is a third.
- `RManager` and `RDomainWidget` are the hubs: they include the most siblings
  because they wire the app shell to the scene and the docks.

The clusters exist. They are simply not reflected in the directory tree.

### Python package: `solvcon/pilot/`

About twenty modules, flat, dominated by `_base_app.py` (over 1000 lines).
The package entry (`__init__.py`) imports `_pilot_core` first for the C++
extension, then the GUI modules behind the `enable` flag. `airfoil/` is
already a subpackage and is the model this plan generalizes.

## Proposed structure

The proposal groups files by the neighborhood they serve. Names are
deliberately parallel between C++ and Python where the concept is shared
(scene, canvas, theme, console, app), so a contributor who learns one side
can navigate the other.

### C++ target tree

```text
cpp/solvcon/pilot/
  pilot.hpp                     umbrella include (unchanged location)
  wrap_pilot.{hpp,cpp}          pybind11 bindings (unchanged location)
  CMakeLists.txt
  shaders/                      GLSL sources (unchanged)
  common/
    common_detail.hpp
    render_misc.{hpp,cpp}
  app/
    RManager.{hpp,cpp}          top-level window hub
    RMenuModel.{hpp,cpp}
    RAction.{hpp,cpp}
    keymap.{hpp,cpp}            Qt-free keymap core
  scene/
    RScene.{hpp,cpp}
    RDomainWidget.{hpp,cpp}
    RCameraController.{hpp,cpp}
    RMeshFrame.{hpp,cpp}
    RAxisGizmo.{hpp,cpp}
  drawable/
    RDrawable.{hpp,cpp}         base for the visual primitives
    RBoundary.{hpp,cpp}
    RFeatureEdges.{hpp,cpp}
    RField.{hpp,cpp}
    RScalarField.{hpp,cpp}
    RSegments.{hpp,cpp}
    RNormals.{hpp,cpp}
    RMaterial.{hpp,cpp}
    RColormap.{hpp,cpp}
    RScalarBar.{hpp,cpp}
  canvas2d/
    R2DWidget.{hpp,cpp}
    RWorldRenderer2d.{hpp,cpp}
    DrawTool.{hpp,cpp}
    RTextOverlay.{hpp,cpp}
  theme/
    theme.{hpp,cpp}
    RThemeManager.{hpp,cpp}
    RThemeBackend.{hpp,cpp}
    RLinuxThemeBackend.{hpp,cpp}
    RMacThemeBackend.{hpp,cpp}
    RWindowsThemeBackend.{hpp,cpp}
  console/
    RPythonConsoleDockWidget.{hpp,cpp}
    RPythonConsoleHistory.{hpp,cpp}
    RPythonSyntaxHighlighter.{hpp,cpp}
    RPythonSyntaxRules.{hpp,cpp}
```

Rationale for the boundaries:

- **`common/`** holds what everyone includes. `common_detail.hpp` is already
  the de-facto base; `render_misc` is small shared render plumbing.
- **`app/`** is the shell: the window hub plus the menu, action, and keymap
  machinery that the keyboard-shortcut work will grow. Keeping the keymap
  core here keeps its Qt-free seam next to the menu it drives.
- **`scene/`** is the 3D world: the RHI widget, the scene graph, the camera,
  the frame, and the axis gizmo.
- **`drawable/`** is everything the scene draws. `RDrawable` is the base, and
  the material, colormap, and scalar bar are the assets those primitives
  render with. Vector visuals and 3D boundary surfaces from the open issues
  land here.
- **`canvas2d/`** is the 2D drawing surface that SVG export and the native XY
  plot extend.
- **`theme/`** and **`console/`** are the two closed islands identified
  above, moved wholesale.

`pilot.hpp` and `wrap_pilot.*` stay at the root: the umbrella header and the
single binding translation unit are the package's front door and are easiest
to find there.

### Python target tree

```text
solvcon/pilot/
  __init__.py                   package entry, re-exports (unchanged location)
  _pilot_core.py                C++ extension shim (unchanged location)
  app/
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

Rationale:

- **`__init__.py`** and **`_pilot_core.py`** stay at the package root so the
  public import surface (`solvcon.pilot.enable`, `RDomainWidget`, ...) and the
  extension shim do not move.
- **`app/`** is the application core, including the oversized `_base_app.py`
  and the theme shim.
- **`onedim/`** collects the 1D CESE demo solvers, which are a family with no
  business sitting beside the mesh or the canvas.
- **`canvas/`**, **`mesh/`**, **`panel/`**, and **`agent/`** mirror the C++
  neighborhoods and the tree/window/profiling docks.

### File-to-home summary

| Neighborhood | C++ (`cpp/solvcon/pilot/`) | Python (`solvcon/pilot/`) |
| --- | --- | --- |
| shared base | `common/` | root (`_pilot_core`) |
| app shell | `app/` | `app/` |
| 3D scene | `scene/` | (in `app/` + `mesh/`) |
| drawables | `drawable/` | (in `mesh/`) |
| 2D canvas | `canvas2d/` | `canvas/` |
| theme | `theme/` | `app/_theme` |
| Python console | `console/` | (C++ only) |
| 1D solvers | (Python only) | `onedim/` |
| docks / panels | (in `app/`) | `panel/` |
| agent | (Python only) | `agent/` |

## Implementation

The reorganization is mechanical but wide, so the plan is to move one
neighborhood at a time, each as its own reviewable change, and keep the tree
buildable after every step.

### C++ moves

For each subdirectory:

1. `git mv` the `.hpp`/`.cpp` pair into the new directory.
2. Update the include paths. Includes are angle-bracket and rooted at the
   package (`#include <solvcon/pilot/RScene.hpp>` becomes
   `#include <solvcon/pilot/scene/RScene.hpp>`), so every include site across
   the pilot, the binary, and the tests updates in lockstep.
3. Regroup the moved files in `cpp/solvcon/pilot/CMakeLists.txt`. The
   `SOLVCON_PILOT_PYMODHEADERS` and `SOLVCON_PILOT_PYMODSOURCES` lists stay
   flat variables; only the paths inside them change. A per-subdirectory
   comment keeps the list readable.
4. Rebuild (`make`) and run the C++ suite (`make gtest`).

`common_detail.hpp` moves first, because it is the most-included header and
flushing out its new path exercises nearly every file. `wrap_pilot.cpp`
updates its includes but does not move.

### Python moves

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

### Ordering

A safe order is: C++ `common/` first (proves the include-path mechanics),
then the closed islands (`theme/`, `console/`) which have the fewest external
edges, then `scene/` and `drawable/` together (they are coupled), then
`canvas2d/` and `app/`. The Python side follows the same island-first idea:
`onedim/` and `agent/` first (nearly self-contained), then `canvas/`,
`mesh/`, `panel/`, and `app/` last (it holds the hub).

## Verification

- `make` and `make pilot` build clean after each neighborhood moves.
- `make gtest` and `make run_pilot_pytest` stay green.
- `make lint` passes (the include-order check `make cinclude` is the one most
  affected by the path changes).
- A headless launch under `QT_QPA_PLATFORM=offscreen` opens the domain widget,
  the 2D canvas, the console dock, and switches theme, confirming the moved
  translation units still wire together.
- `git diff --stat` on each step is dominated by renames plus include-path
  edits, with no logic changes; the review can lean on `git log --follow`.

## Out of scope

- **No behavior change.** This is a pure move-and-rewire. Splitting the
  oversized `_base_app.py` or `RManager` into smaller units is a tempting
  neighbor but belongs in its own issue; doing it here would hide real logic
  changes inside a rename diff.
- **No public API change.** The Python import surface and the pybind11 module
  layout stay identical.
- **No new features.** The open pilot issues motivate the boundaries but are
  implemented separately, on top of the new tree.

## Open questions for review

1. **Depth.** Is one level of subdirectory the right amount, or should
   `drawable/` nest under `scene/` (a `scene/drawable/`) to signal that
   drawables only live inside a scene?
2. **`app/` versus `panel/` on the Python side.** `_profiling`,
   `_tree_panel`, and `_window_manager` are docks; is `panel/` the right name,
   or should they sit in `app/` as application chrome?
3. **Naming.** `canvas2d/` in C++ versus `canvas/` in Python: keep the `2d`
   suffix on the C++ side (to pair with a future `scene` = 3D), or drop it for
   symmetry?
4. **`common/` scope.** Should `render_misc` live in `common/`, or in
   `drawable/` next to its callers?

## Delivery status

- Branch: `plan/pilot-structure` on the personal fork.
- Contents: this development-plan page only. No file moves yet; the moves
  begin once the submodule boundaries are agreed.
- CI: draft pull request opened on the fork for review.

## Appendix: chat history

- "Make a devplan for the pilot file-structure reorganization. Analyze the
  latest code base and recent issues to guess the submodules and propose a
  structure. Push to the fork and open a pull request for review." Drove the
  code survey (flat C++ and Python inventories, the include-coupling scan),
  the submodule proposal above, and this page.

<!-- vim: set ft=markdown ff=unix fenc=utf8 et sw=2 ts=2 sts=2 tw=79: -->
