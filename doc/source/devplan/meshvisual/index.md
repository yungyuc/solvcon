# Mesh Visualization (Basic 3D Visualization, Part 1)

This development plan records the mesh-visualization work that grows the
pilot's QRhi domain viewer ({cpp:class}`~solvcon::RDomainWidget`) toward
VTK-class and Blender-class inspection of unstructured meshes. It is a
prototype: every item is a small, reviewable step landed as its own pull
request into the `meshvisual` integration branch. Solution (field) animation
is deferred future work and out of scope here.

## Problem

The viewer could draw a mesh as a lit surface, a wireframe, or a point cloud
(the M1 baseline) and orbit it, but it had none of the inspection tools a mesh
review needs: no way to color cells by an attribute or a quality metric, no
picking or measurement, no standard view presets, no cube axes, no geometric
cuts, no multi-object scene, and no publication-resolution capture. This plan
adds those, one step at a time, reusing the existing drawable and overlay
infrastructure.

## Where the code lives

The viewer is a `QRhiWidget` under `cpp/solvcon/pilot/`:

- `RDomainWidget` -- the widget: owns the scene, drives the render loop, and
  exposes the whole control surface to Python through `wrap_pilot.cpp`.
- `RScene` -- the drawable list, the domain bounding box, the projection
  choice, and the framing camera.
- `RCameraController` -- the pose and the pan/orbit/first-person interaction.
- `RDrawable` -- the base drawable: owns its buffers, its per-object uniform
  (MVP, color, light direction), and its pipeline built from an `RMaterial`.
  Subclasses supply geometry: `RMeshFrame` (surface/wireframe/points),
  `RField` (per-vertex color), `RScalarField` (scalar through a LUT),
  `RBoundary` / `RRibbon` / `RFeatureEdges` / `RNormals` (overlays).
- Overlays painted with `QPainter` into a texture and drawn as a quad follow
  the `RAxisGizmo` / `RScalarBar` pattern.

The Python tests live in `tests/test_pilot_domain_widget.py`; render-dependent
tests grab an offscreen frame and skip where a graphics surface is
unavailable, so they run on the Linux CI build (Xvfb) and elsewhere skip
rather than fail.

## Design and implementation

Each milestone below is one step PR. `M1` (lit surface and the
surface/wireframe/points representations) already shipped and is the baseline.

### Mesh rendering

- **M2 -- Surface with edges and per-object opacity.** A `setRepresentation`
  preset (`surface`, `wireframe`, `points`, `surface_edges`) over the existing
  independent styles, and a clamped per-drawable opacity that scales the color
  alpha with source-over blending always on, so a runtime opacity change needs
  no pipeline rebuild. Python: `setRepresentation`, `setMeshOpacity`,
  `setFieldOpacity`.
- **M3 -- Feature edges and normal glyphs.** `showFeatureEdges` draws the
  boundary rim as one bold orange ribbon; `showNormals` draws a green arrow at
  each face center. Both reuse a shared `RRibbon` edge helper. The mesh panel
  gains a check box for each.
- **M4 -- GPU lookup table and scalar bar.** `RColormap` (viridis, coolwarm,
  jet, grayscale) baked into a `QRhiTexture`; `RScalarField` samples the LUT in
  the fragment stage; `RScalarBar` reports the mapping. Python:
  `updateScalarField`, the `colormap` property, `setScalarRange`,
  `scalarRange`, `showScalarBar`, `setScalarBarTitle`.

### Coloring the mesh by its own quantities

- **M5 -- Color by cell attribute.** A qualitative `RColormap::categorical()`
  palette and a per-cell-flat scalar field over the surface, coloring by
  element type, cell group, or boundary set with a legend. Python:
  `colorByCellType`, `colorByCellGroup`, `colorByBoundary`,
  `clearCellColoring`.
- **M6 -- Quality metrics.** Per-cell `volume`, `aspect_ratio`, `skewness`,
  `min_angle`, `max_angle` computed on the `SimpleArray` path and colored
  through the continuous LUT; the surface-scalar geometry is factored into a
  shared `collectSurfaceScalars`. Python: `colorByQuality`, `qualityRange`.

### Navigation and view

- **M7 -- View presets and projection toggle.** Axis-aligned and isometric
  presets that frame the scene, and an explicit parallel/perspective override
  independent of the 2D/3D default. Python: `setView`, the `projection`
  property.
- **M8 -- Trackball orbit and pivot.** A free trackball orbit that rolls the up
  axis beside the level turntable, and `setPivot` to move the orbit center.
  Python: the `orbitStyle` property, `setOrbitStyle`, `setPivot`,
  `frameSelected`.
- **M9 -- Blender navigation mapping.** A selectable mapping (`default` or
  `blender`: middle orbits, Shift/Ctrl/Alt+middle pan/zoom/pivot, Alt+left
  aliases middle), a rotate sensitivity, and a discrete fixed-angle orbit step.
  Python: the `navigationMapping` property, `setNavigationMapping`,
  `setOrbitSensitivity`, `orbitStep`.

### Inspection

- **M10 -- Picking.** A CPU ray-cast (back-projecting the click through the
  same view-projection) resolves the cell, node, or face under the cursor,
  reports its id and geometry, highlights the picked cell, and keeps it as the
  selection. Python: `pickCell`, `pickNode`, `pickFace`, `clearSelection`,
  `hasSelection`.
- **M11 -- Measurement.** `measureDistance` and `measureAngle` return the value
  and draw a magenta ruler (`RSegments`). Python: `measureDistance`,
  `measureAngle`, `clearMeasurements`.
- **M12 -- Zoom to selection and reset.** `RScene` gains a framing box so the
  projection sizes to the selection, not the whole scene. Python:
  `zoomToSelection`, `resetCamera`.

### Scene and annotation

- **M13 -- Cube axes and title.** A bounding-box grid with tick marks
  (`RSegments`) whose coordinates read back through `cubeAxesTicks`, and an
  `RTextOverlay` figure title. Python: `showCubeAxes`, `cubeAxesTicks`, the
  `title` property, `setTitle`.
- **M14 -- Slice and clip.** `addClip` keeps the surface on one side of a
  plane; `addSlice` intersects the plane with the cell edges and draws the
  cross-section outline. Python: `addClip`, `addSlice`, `clearFilters`.
- **M15 -- Multi-mesh scene.** `RDrawable` gains a model transform and a name;
  the widget registers named objects. Python: `addObject`,
  `setObjectTransform`, `setObjectVisible`, `setObjectOpacity`, `objectNames`.

### Capture

- **M16 -- Offscreen capture.** `renderToImage` renders at an arbitrary
  resolution decoupled from the widget, with an optional transparent
  background. Python: `renderToImage(path, width, height, transparent)`.

## Verification

Each step builds with `make` (the Qt pilot), passes `make lint`, and adds
Python tests to `tests/test_pilot_domain_widget.py`. Geometry and value tests
(representation presets, opacity clamping, metric ranges, tick coordinates,
pick ids and geometry, measured distances and angles, clip/slice counts,
object registry) are exact and run everywhere. Render-dependent pixel tests
grab an offscreen frame and skip where no graphics surface is available; the
Linux CI build exercises them under Xvfb.

## Prototype simplifications (out of scope for the first pass)

These are deliberate cuts kept small so each step stays reviewable; each is a
natural follow-up:

- **M9:** the View > Camera menu radio tracking the live mode is not yet wired;
  the button routing is exercised interactively (a pybind-created widget is not
  a PySide `QObject` that `sendEvent` accepts).
- **M10:** picking is a CPU ray-cast rather than an offscreen integer-id
  target; adequate for the sample meshes, to be swapped for the id buffer when
  picking must scale.
- **M11 / M13:** the ruler and the cube-axes tick labels draw their geometry;
  their `QPainter` numeric labels reuse the `RTextOverlay` / `RScalarBar`
  pattern and are a follow-up. The figure title does render.
- **M14:** slice and clip are implemented directly on the widget; the `RFilter`
  base that re-evaluates a pipeline of filters is a later refactor.
- **M15:** the outliner dock UI is a follow-up; the by-name object API that
  drives it is in place.

## Delivery status

The work lands as step PRs into the `meshvisual` branch on the personal fork,
each run through CI before merge. M1 is the baseline; M2, M3, and M4 have
merged; M5 through M16 are implemented and land in order as each PR goes green.
Every milestone builds, lints, and passes the pilot test suite locally.

## Appendix: chat history

- *"Use yyc repository to prototype for the Part 1 mesh-visualization plan; do
  not reference any issue; M1 is done, skip it; create a PR wrt `meshvisual`
  for each item to make sure CI works; merge the step PRs to `meshvisual`; use
  the skip-ci label when CI is not needed; record everything in a devplan doc
  and include it in the commit; update the project document."* -- drove the
  whole effort: the per-milestone step-PR workflow into `meshvisual`, this
  devplan, and the domain-viewer documentation updates.
- *"All twelve fresh milestones, autonomously; full CI on each step PR."* --
  set the pace: implement M5 through M16 in order, each with its own CI run
  before merge.

<!-- vim: set ft=markdown ff=unix fenc=utf8 et sw=2 ts=2 sts=2 tw=79: -->
