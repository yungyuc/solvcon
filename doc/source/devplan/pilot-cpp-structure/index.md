# Reorganize the pilot C++ file structure

Move the pilot GUI C++ core (`cpp/solvcon/pilot/`) from a flat directory of
loose files to a hierarchical layout, so that the fast-growing feature set
gets a home that tells a reader where each piece belongs.

This is a design proposal for review. The submodule boundaries below are a
first guess drawn from the current code and the direction of recent pilot
work; they are meant to be argued about before any file moves.

## Problem

The pilot C++ core started small and grew fast. It now holds about forty
classes in one directory. It is flat: every file sits beside every other, and
the only structure is the file name. A reader opening `cpp/solvcon/pilot/`
cannot tell the 3D scene from the 2D canvas from the theme backends from the
Python console without reading each header.

The growth is not slowing. Open pilot issues each add code that lands in the
same flat directory:

- keyboard-shortcut system (menus, actions, keymap), already landing as
  `RShortcutManager`,
- SVG export and a native XY plot (the 2D canvas),
- terminal-style input and output (the console dock),
- window-geometry persistence (the app shell),
- cell picking, vector visuals, 3D boundary surfaces (the scene and its
  drawables).

Each of these has a natural neighborhood. A flat directory hides those
neighborhoods, so every new feature widens the same pile instead of deepening
a clear module. The goal of this plan is to name the neighborhoods and give
each one a directory before the next wave of features arrives.

## Proposed structure

The proposal groups files by the neighborhood they serve. The include-coupling
evidence behind these boundaries is in "Where the code lives today" below.

### Target tree

```text
cpp/solvcon/pilot/
  pilot.hpp                     umbrella include (unchanged location)
  wrap_pilot.{hpp,cpp}          pybind11 bindings (unchanged location)
  CMakeLists.txt
  shaders/                      GLSL sources (unchanged)
  common/
    common_detail.hpp
    platform.hpp                platform-id enum and per-platform records
    render_misc.{hpp,cpp}
  app/
    RManager.{hpp,cpp}          top-level window hub
    RMenuModel.{hpp,cpp}
    RAction.{hpp,cpp}
    RShortcutManager.{hpp,cpp}
    keymap.{hpp,cpp}            Qt-free keymap core
  visualization/
    RScene.{hpp,cpp}
    RDomainWidget.{hpp,cpp}
    RCameraController.{hpp,cpp}
    RMeshFrame.{hpp,cpp}
    RAxisGizmo.{hpp,cpp}
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
  canvas/
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
  the de-facto base, and `platform.hpp` is shared by both the keymap and the
  theme; `render_misc` is small shared render plumbing.
- **`app/`** is the shell: the window hub plus the menu, action, keymap, and
  shortcut-manager machinery. The keyboard-shortcut work has already begun to
  land (`RShortcutManager` alongside `keymap`), which is exactly the growth
  this neighborhood is meant to absorb. Keeping the Qt-free keymap core here
  puts its seam next to the menu and shortcuts it drives.
- **`visualization/`** is the 2D and 3D visualization: the RHI widget, the
  scene graph, the camera, the frame, and the axis gizmo, together with
  everything the scene draws. `RDrawable` is the base for the visual
  primitives, and the material, colormap, and scalar bar are the assets they
  render with. Vector visuals and 3D boundary surfaces from the open issues
  land here.
- **`canvas/`** is the 2D drawing surface that SVG export and the native XY
  plot extend.
- **`theme/`** and **`console/`** are self-contained islands, each a closed
  set of files with no scene or drawable edges, moved wholesale.

`pilot.hpp` and `wrap_pilot.*` stay at the root: the umbrella header and the
single binding translation unit are the package's front door and are easiest
to find there.

### File-to-home summary

| Neighborhood | Directory |
| --- | --- |
| shared base | `common/` |
| app shell | `app/` |
| visualization | `visualization/` |
| 2D canvas | `canvas/` |
| theme | `theme/` |
| Python console | `console/` |

## Planned steps

The move is carried out in small, independently reviewable steps, each one
keeping the tree buildable and the binding layout unchanged. The order is
island-first: the least-connected neighborhoods move before the hubs, so the
risky wiring changes land last against a tree that is already mostly sorted.
The mechanics for each step are in the Implementation section.

1. Land this development plan (this page).
   *Estimated diff: documentation only.*
2. **`common/`.** Move `common_detail.hpp`, `platform.hpp`, and `render_misc`.
   This is the most-included header set, so it exercises the include-path
   mechanics against nearly every file first.
   *Estimated diff: ~40 lines, mostly include-path edits across about 31
   files, plus four `CMakeLists.txt` entries.*
3. **`theme/` and `console/`.** The two self-contained islands, fewest external
   edges, moved wholesale.
   *Estimated diff: ~55 lines across the 16 moved files, their include sites,
   and the CMake lists.*
4. **`visualization/`.** The scene and its drawables move together: `RDrawable`
   is the base the scene draws, and the two are tightly coupled.
   *Estimated diff: ~90 lines, the largest step, covering 30 moved files, their
   heavy cross-includes, and about 30 CMake path edits.*
5. **`canvas/` and `app/`.** `app/` moves last because it holds the `RManager`
   hub plus the menu, action, keymap, and shortcut manager.
   *Estimated diff: ~55 lines across the 18 moved files, their include sites,
   and the CMake lists.*
6. **Final sweep.** Run `make lint`, rebuild the docs, and confirm each step's
   diff is renames plus include edits only.
   *Estimated diff: under 10 lines of include-order fixups.*

## Where the code lives today

About forty translation units plus a `shaders/` directory, all flat. A quick
scan of the intra-pilot includes shows the coupling is already clustered even
though the files are not:

- `common_detail.hpp` is the shared base, pulled in by roughly every source
  (about 25 include sites). It is a genuine common header. `platform.hpp`,
  the platform-id enum shared by the keymap and the theme, is a second one.
- `RDrawable.hpp` roots the visual primitives (about 10 include sites): the
  boundary, feature edges, field, scalar field, segments, and normals all
  derive from or lean on it.
- The theme files (`theme`, `RThemeManager`, `RThemeBackend`, and the three
  per-platform backends) form a closed island.
- The console files (`RPythonConsoleDockWidget`, `RPythonConsoleHistory`,
  `RPythonSyntaxHighlighter`, `RPythonSyntaxRules`) form another closed
  island.
- The 2D canvas (`R2DWidget`, `RWorldRenderer2d`, `DrawTool`, `RTextOverlay`)
  is a third.
- `RManager` and `RDomainWidget` are the hubs: they include the most siblings
  because they wire the app shell to the scene and the docks.

The clusters exist. They are simply not reflected in the directory tree.

## Implementation

The reorganization is mechanical but wide. The step sequence is in Planned
steps above; the mechanics for one neighborhood are below.

For each subdirectory:

1. `git mv` the `.hpp`/`.cpp` pair into the new directory.
2. Update the include paths. Includes are angle-bracket and rooted at the
   package (`#include <solvcon/pilot/RScene.hpp>` becomes
   `#include <solvcon/pilot/visualization/RScene.hpp>`), so every include site
   across the pilot, the binary, and the tests updates in lockstep.
3. Regroup the moved files in `cpp/solvcon/pilot/CMakeLists.txt`. The
   `SOLVCON_PILOT_PYMODHEADERS` and `SOLVCON_PILOT_PYMODSOURCES` lists stay
   flat variables; only the paths inside them change. A per-subdirectory
   comment keeps the list readable.
4. Rebuild (`make`) and run the C++ suite (`make gtest`).

`common_detail.hpp` moves first, because it is the most-included header and
flushing out its new path exercises nearly every file. `wrap_pilot.cpp`
updates its includes but does not move.

## Verification

- `make` and `make pilot` build clean after each neighborhood moves.
- `make gtest` stays green.
- `make lint` passes (the include-order check `make cinclude` is the one most
  affected by the path changes).
- A headless launch under `QT_QPA_PLATFORM=offscreen` opens the domain widget,
  the 2D canvas, the console dock, and switches theme, confirming the moved
  translation units still wire together.
- `git diff --stat` on each step is dominated by renames plus include-path
  edits, with no logic changes; the review can lean on `git log --follow`.

## Out of scope

- **No behavior change.** This is a pure move-and-rewire. Splitting the
  oversized `RManager` into smaller units is a tempting neighbor but belongs
  in its own issue; doing it here would hide real logic changes inside a
  rename diff.
- **No binding change.** The pybind11 module layout stays identical.
- **No new features.** The open pilot issues motivate the boundaries but are
  implemented separately, on top of the new tree.

## Open questions for review

1. **`common/` scope.** Should `render_misc` live in `common/`, or in
   `visualization/` next to its callers?

## Decisions from review

- **Canvas naming.** The 2D directory is named `canvas/`, not `canvas2d/`. The
  `visualization/` directory already carries the general drawing meaning, so no
  `2d` suffix is needed to disambiguate.
- **Visualization grouping.** The scene graph and the drawable primitives share
  one `visualization/` directory rather than separate `scene/` and `drawable/`
  directories, since both serve the 2D and 3D visualization and are tightly
  coupled.
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
  inventory, the include-coupling scan) and the submodule proposal above.
- Review on the pull request: "Name it just `canvas`" on the 2D directory.
  Renamed `canvas2d/` to `canvas/` and recorded the decision.
- "Lead with the structure and the step roadmap." Reordered the page so the
  proposal and the step roadmap lead, with the code survey below them.
- "Focus this plan on the C++ core." Scoped the page to `cpp/solvcon/pilot/`.
- Review on the pull request: "`scene` and `drawable` are both for the 2/3D
  visualization. Combine and name as `visualization`." Merged the two into a
  single `visualization/` directory and recorded the decision.
- "Add a diff-line estimate in each of the steps." Added an estimated diff
  size to every planned step.

<!-- vim: set ft=markdown ff=unix fenc=utf8 et sw=2 ts=2 sts=2 tw=79: -->
