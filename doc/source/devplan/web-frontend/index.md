# A web frontend as a secondary pilot UI

A plan for a browser-based frontend that reuses solvcon's existing C++/pybind
compute core while replacing the Qt viewer with a web client. The computation
stays on the backend; the browser only draws. Draft for review and discussion;
the plan proposes a direction, not a committed roadmap.

## What this plans

The pilot GUI today is a Qt6 desktop application. This plan adds a *second*,
independent frontend that runs in a web browser and talks to a headless backend
over the network. The desktop pilot is not replaced or modified; the web UI is
an alternative surface over the same numerical core.

Two properties frame the whole plan:

- **Compute on the backend.** All numerical work stays in the existing C++
  solvers behind the `_solvcon` extension. The browser never runs a solver; it
  renders results the backend computes and streams.
- **Reuse the existing seams.** solvcon already separates computation from
  rendering at a clean, numpy-array boundary, and it already speaks JSON at the
  edge of its agent layer. The web frontend targets those seams rather than
  inventing new ones.

## Where the code lives

The pilot is a Qt6 viewer *embedded inside a Python interpreter*, not a Python
app that imports Qt. `cpp/binary/pilot/pilot.cpp` boots CPython and registers
the `_solvcon` module; `RManager` (a singleton `QObject`) then owns the
`QApplication`, the `QMainWindow`, and a `QMdiArea` of canvas subwindows. The
GUI above the compute layer is driven from Python through PySide6.

The important finding is that the code is already organized in three tiers with
clean boundaries.

```{mermaid}
flowchart TB
    subgraph compute["Compute / model  (Qt-free C++, reuse verbatim)"]
        A1["Euler1DCore, LinearScalarSolver"]
        A2["StaticMesh (unstructured mesh)"]
        A3["World / WorldFp64, ViewTransform2d"]
        A4["multidim/euler drivers"]
    end
    subgraph logic["Widget-free render logic  (Qt math only, port lightly)"]
        B1["RScene (scene graph, framing)"]
        B2["RCameraController (pure matrix math)"]
        B3["RColormap (LUT), CPU triangulation builders"]
    end
    subgraph shells["Qt UI shells  (replace entirely)"]
        C1["RDomainWidget : QRhiWidget (GPU)"]
        C2["R2DWidget : QWidget (QPainter 2D)"]
        C3["RManager, docks, menus, themes"]
    end
    compute -->|numpy arrays| logic --> shells
```

The compute/render boundary is already numpy arrays, and it lives in Python,
not C++. The C++ solvers know nothing about widgets. The reference compute to
view flow is `solvcon/pilot/_oblique.py`: build a `StaticMesh`, march the Euler
driver, read the density field from the solver array, triangulate once, map
values to colors in numpy, then push to the GPU with
`widget.updateColorField(verts, colors, indices)` on a timer.

### The render stacks

There are two independent render stacks, plus a third path for the 1D demos.

- **GPU mesh/field viewer.** `RDomainWidget : QRhiWidget` drives an `RScene` of
  `RDrawable` subclasses (`RMeshFrame`, `RField`, `RScalarField`, `RSegments`,
  `RBoundary`). Geometry is interleaved float vertex buffers plus uint32 index
  buffers; scalar fields are colored on the GPU by a 256x1 colormap lookup
  texture and baked GLSL 440 shaders under `cpp/solvcon/pilot/shaders/`.
- **CPU 2D geometry canvas.** `R2DWidget : QWidget` paints a `World` (points,
  lines, Bezier curves, shapes) through `RWorldRenderer2d.paint_canvas`. Used
  by the drawing, SVG, and airfoil tools.
- **1D solver demos.** The Euler, Burgers, and linear-wave demos embed
  matplotlib in Qt (`backend_qtagg.FigureCanvas`), with `QuantityLine` wrapping
  the solver's numpy arrays.

### The data contract to reproduce on the web

The pybind surface of the two widgets *is* the contract a web client must
reproduce. It is already array-shaped and serializes directly onto GPU buffers:

- `updateMesh(StaticMesh)`
- `updateColorField(vertices, colors, indices)` (float positions, per-vertex
  RGB, uint32 indices)
- `updateScalarField(vertices, scalars, indices)` with a `colormap` name and
  `setScalarRange` (the mesh sends a scalar per vertex; coloring happens on the
  GPU)
- `updateWorld(World)` for 2D geometry
- camera and projection state, `pickCell/pickNode/pickFace`, and
  `renderToImage`/`saveImage`/`clipImage` for offscreen capture

The GLSL 440 shaders translate to WebGL2 GLSL ES 3.0 or WebGPU almost verbatim,
and `RCameraController` is pure matrix math that reimplements trivially in
JavaScript.

### An already-serializable command seam

`solvcon/agent/` is a fully Qt-free JSON command seam. Pluggable language-model
backends turn a prompt into a list of drawing-command dicts; `AgentSession`
runs them against a `World`; `CommandSet.tool_definitions()` emits MCP-style
schemas. It already speaks the serializable language a web client needs, and
`World.describe_state(level=...)` returns JSON. The only networking anywhere in
the tree is an outbound `http.client` call in the OpenAI-compatible backend;
there is no inbound server yet.

## Design

The core decision is where rendering happens. This is the same fork that
Kitware's trame exposes for its VTK views, and it frames the choice cleanly for
solvcon.[^trame]

```{mermaid}
flowchart LR
    subgraph backend["Backend (CPython + _solvcon, no Qt for options B/D)"]
        S["Solver core"]
    end
    subgraph optA["Option A: server-side pixel streaming"]
        RA["Offscreen QRhi render (renderToImage)"]
    end
    subgraph optB["Option B: client-side geometry"]
        GB["Serialize verts/indices/scalars"]
    end
    S --> RA
    S --> GB
    RA -->|"image frames (WebSocket)"| BA["Browser: image + input"]
    GB -->|"typed-array frames (WebSocket)"| BB["Browser: WebGL renderer"]
```

**Option A: server-side pixel streaming (remote rendering).** The backend
renders offscreen and streams frames to the browser, which is a thin image plus
input surface. This is trame's remote view.[^trame]

- Reuses the entire existing QRhi renderer verbatim; `renderToImage` and
  `clipImage` already exist, so the render seam is nearly free. Pixel-identical
  to the desktop, and it handles arbitrarily large meshes with no JavaScript
  render code.
- Needs a GPU or an offscreen software rasterizer on the server, one render
  context per session, and a network round-trip per interaction (latency).

**Option B: client-side geometry streaming (WebGL/WebGPU).** The backend
serializes geometry and streams it; the browser renders with a JavaScript
engine. This is trame's local view, where the server needs no GPU and only
geometry crosses the wire.[^trame]

- No server GPU; smooth local pan, zoom, and rotate with no round-trip; scales
  to many clients cheaply. The interleaved float arrays and uint32 indices map
  one-to-one onto WebGL buffers, and the shaders and LUT-based scalar coloring
  port almost directly.
- Requires reimplementing the render tier in JavaScript (drawables, camera,
  colorbar, axes, picking). Large meshes cost transfer bandwidth; progressive
  and compressed streaming of scientific meshes is a well-studied
  pattern.[^webviz][^collab]

**Option C: compile the render tier to WebAssembly** (the VTK.wasm approach,
where the same pipeline runs client-side).[^wasm] Highest reuse in theory, but
QRhi does not target the web cleanly and the port to WebGL/WebGPU remains. Not
worth it near-term.

**Backend shape, common to every option.** Run an ordinary CPython process that
imports the `_solvcon` extension, wrapped by an async web framework
(FastAPI is a natural fit for native WebSocket support). For the client-render
options the backend needs no Qt at all: `HAS_PILOT` is false and the numerics
still work. WebSocket carries interactive and streaming state; REST carries
one-shot configuration; geometry travels as binary typed-array frames rather
than JSON once meshes grow.

## Implementation

Phased, lowest risk first, so each phase is independently useful.

```{mermaid}
flowchart LR
    P1["Phase 1: 1D demos"] --> P2["Phase 2: field viewer"]
    P2 -.fallback.-> P2A["Phase 2-alt: pixel streaming"]
    P2 --> P3["Phase 3: agent + 2D World"]
```

1. **Phase 1: 1D solver web demo.** Highest demo value, least entanglement.
   A FastAPI endpoint drives `ShockTube` and `march_alpha2`, streams the
   `coord_field` and per-quantity arrays over WebSocket, and the browser plots
   with a fast charting library. This keeps the `OneDimBaseApp` control logic
   and replaces only the matplotlib `QuantityLine` surface. Pure numpy out,
   almost nothing to port.
2. **Phase 2: field viewer via client geometry streaming (Option B).**
   Serialize the `updateColorField` and `updateScalarField` payloads to the
   browser; render with a WebGL engine; do scalar colormapping on the GPU with
   a lookup texture exactly as `scalar.frag` does; reimplement
   `RCameraController` in JavaScript. Begin with a static frame, then add the
   `_oblique.py`-style animation loop.
3. **Phase 2 fallback: server-side pixel streaming (Option A).** If porting the
   render tier proves too heavy, ship pixel streaming first for desktop parity.
   `renderToImage` already exists, so this is the fastest path to any web
   pixels, at the cost of a server GPU and interaction latency. Useful as a
   parity reference even when Option B is the long-term target.
4. **Phase 3: agent panel and 2D geometry canvas.** Expose the Qt-free
   `AgentSession`, `CommandSet.tool_definitions()`, and `describe_state()` over
   the web, reusing the existing command seam.

## Verification

- **Phase 1** verifies against the existing desktop plots: the same solver
  configuration must produce the same curves in the browser as in the
  matplotlib demo, checked numerically on the streamed arrays, not by eye.
- **Phase 2** verifies geometry payloads against the desktop `updateColorField`
  input (identical vertex, color, and index arrays) and compares a browser
  render to a `renderToImage` capture for the same camera.
- **Backend** verification runs headless (`HAS_PILOT` false), so the compute
  path is covered by the existing pytest suite without a display.
- The offscreen render path for Option A must respect the platform constraints
  the CI already encodes for the QRhi viewer (offscreen render-skip on some
  platforms).

## Out of scope

- No changes to the desktop pilot, the solvers, or the pybind surface beyond
  additive serialization helpers.
- No authentication, multi-user sessions, or deployment hardening in this plan;
  the first target is a single-user local backend.
- No WebAssembly port (Option C) in this plan.
- No picking or filtering on the web client in the first phases; those move
  server-side or arrive in a later increment.

## Risks and open questions

- **Server GPU for Option A.** Offscreen QRhi rendering needs a GPU or a
  software rasterizer and one context per session; scaling is the open
  question.
- **Render-tier reimplementation for Option B.** Shaders and camera math port
  easily, but drawables, the colorbar, axes, and picking are real work.
- **Bandwidth for large meshes.** Client geometry streaming must compress and
  stream progressively for meshes beyond a few million cells.
- **Reuse of `RScene`/`RCameraController`.** Their Qt-free classification was
  read from headers and includes, not the `.cpp` bodies; confirm before betting
  reuse on them.
- **Security surface.** Exposing compute plus language-model backends over a
  network needs a threat model before anything leaves localhost.

## Delivery status

- Branch: `worktree-qtweb` on the personal fork.
- This document is the deliverable; no implementation code has landed yet.
- Preview served on the trusted network during review.
- CI: draft opened with the full matrix skipped by label.

## Appendix: how this plan was produced

This plan was produced in a single research session against the qtweb worktree.
The prompts that drove it, in order:

- "Analyze the existing qt code in the repo. Research how to build a web
  frontend as a secondary UI. Have the computation done on backend." Drove the
  three-tier analysis, the two-render-stack finding, the pybind data contract,
  and the survey of server-side versus client-side rendering.
- "Make a devplan doc for your research. Build it and serve on the trusted
  network. Push to the fork and track. Create a PR for review. Skip CI." Drove
  this document, its build and preview, and the draft pull request.

[^trame]: Kitware, "trame" tutorial, local versus remote VTK views.
    https://kitware.github.io/trame/guide/tutorial/vtk.html

[^wasm]: Kitware, "VTK.wasm and its trame integration," 2024.
    https://www.kitware.com/vtk-wasm-and-its-trame-integration/

[^webviz]: E. Evans et al., "Modern Scientific Visualizations on the Web,"
    *Informatics* 7(4), 37, 2020. https://www.mdpi.com/2227-9709/7/4/37

[^collab]: C. Marion and J. Jomier, "Real-time collaborative scientific WebGL
    visualization with WebSocket," *Proc. 17th Int. Conf. on 3D Web
    Technology*, 2012. https://dl.acm.org/doi/10.1145/2338714.2338721

<!-- vim: set ft=markdown ff=unix fenc=utf8 et sw=2 ts=2 sts=2 tw=79: -->
