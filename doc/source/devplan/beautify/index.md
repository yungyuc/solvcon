# Elegant cross-platform theming for the pilot

A development plan for giving the pilot GUI a polished, consistent look on both
Linux and macOS, with a light and a dark theme the user can switch at runtime.

## Problem

The pilot rides whatever style and palette the platform hands it. On Linux it
inherits the desktop's default; on macOS it renders through a different native
style; and one widget, the Python console, hardcodes black text on a white
background. The result is threefold:

- The application looks different on each platform, and neither look is under
  our control.
- There is no way to choose a light or a dark appearance.
- The hardcoded console glares white and cannot follow any theme, so even if
  the rest of the window went dark the console would stay bright.

The goal is a single, curated look that renders the same on Linux and macOS,
offers light and dark, and switches at runtime, including following the
operating system's own light or dark setting.

## Approach

The design standardizes on Qt's **Fusion** style and drives every color from a
curated **QPalette**, one for light and one for dark, applied to the whole
application. Fusion draws from the same code on every platform and honors a
custom palette, so the palette becomes the single lever for the entire look.
A small menu, **View > Theme**, offers three modes:

- **Follow system** tracks the operating system's color scheme and changes with
  it live.
- **Light** and **Dark** pin the palette regardless of the operating system.

This mirrors the recommendation that emerged from surveying current Qt practice:
Fusion plus a palette is the low-maintenance path to reliable, identical
theming on Linux and macOS, and it is the approach that actually works on Linux,
where Qt's own `setColorScheme` override is a no-op on most desktops.[^fusion]
[^colorscheme]

## Where the code lives

The pilot is coordinated by a single manager and a live menu model:

- `RManager` (`cpp/solvcon/pilot/RManager.hpp`) is the singleton that owns the
  `QApplication` and the `QMainWindow`, and builds the menus in `setUpMenu()`.
  Its constructor is where the application object is created, so it is the
  natural place to install a style and paint a palette before any widget
  exists.
- `RMenuModel` (`cpp/solvcon/pilot/RMenuModel.hpp`) is the slash-path menu model.
  The **View** menu already exists (weight 20), and the exclusive
  `QActionGroup` used for the camera controllers is a ready template for a
  Light/Dark/System radio group.
- `RPythonConsoleDockWidget` (`cpp/solvcon/pilot/RPythonConsoleDockWidget.cpp`)
  is the one widget that forced its own light palette and had to be reconciled.
- The gtest seam (`gtests/CMakeLists.txt`, `test_nopython`) compiles individual
  Qt-free pilot sources directly, so a Qt-free color model can be unit tested
  without a GUI, the same way `RPythonSyntaxRules` is.

## Design

The work splits into a Qt-free model that can be tested headlessly and a thin
Qt adapter that applies it, wired into the manager.

```{raw} html
<figure style="margin:1.5em 0;">
<svg viewBox="0 0 720 250" xmlns="http://www.w3.org/2000/svg" role="img"
     aria-label="Three-layer theme architecture" style="max-width:100%;height:auto;font-family:sans-serif;">
  <style>
    .box{rx:8;stroke-width:1.5;}
    .lbl{font-size:14px;font-weight:600;}
    .sub{font-size:11px;fill:#555;}
    .arr{stroke:#888;stroke-width:1.5;marker-end:url(#ah);fill:none;}
  </style>
  <defs>
    <marker id="ah" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto">
      <path d="M0,0 L6,3 L0,6 Z" fill="#888"/>
    </marker>
  </defs>
  <rect x="20" y="30" width="200" height="190" rx="8" fill="#eef3fb" stroke="#3574f0"/>
  <text x="120" y="52" text-anchor="middle" class="lbl">RTheme</text>
  <text x="120" y="70" text-anchor="middle" class="sub">Qt-free model</text>
  <text x="120" y="98" text-anchor="middle" class="sub">ThemeMode / ThemeVariant</text>
  <text x="120" y="116" text-anchor="middle" class="sub">light &amp; dark color tables</text>
  <text x="120" y="134" text-anchor="middle" class="sub">resolveThemeVariant()</text>
  <text x="120" y="168" text-anchor="middle" class="sub" style="font-style:italic;">unit tested by</text>
  <text x="120" y="184" text-anchor="middle" class="sub" style="font-style:italic;">test_nopython (no GUI)</text>

  <rect x="270" y="30" width="200" height="190" rx="8" fill="#eef3fb" stroke="#3574f0"/>
  <text x="370" y="52" text-anchor="middle" class="lbl">RThemeManager</text>
  <text x="370" y="70" text-anchor="middle" class="sub">Qt adapter (QObject)</text>
  <text x="370" y="98" text-anchor="middle" class="sub">installs Fusion once</text>
  <text x="370" y="116" text-anchor="middle" class="sub">builds a QPalette</text>
  <text x="370" y="134" text-anchor="middle" class="sub">follows the OS scheme</text>
  <text x="370" y="152" text-anchor="middle" class="sub">emits themeChanged</text>

  <rect x="520" y="30" width="180" height="190" rx="8" fill="#f3f3f4" stroke="#888"/>
  <text x="610" y="52" text-anchor="middle" class="lbl">RManager</text>
  <text x="610" y="70" text-anchor="middle" class="sub">coordinator</text>
  <text x="610" y="98" text-anchor="middle" class="sub">applies before widgets</text>
  <text x="610" y="116" text-anchor="middle" class="sub">View &gt; Theme menu</text>
  <text x="610" y="134" text-anchor="middle" class="sub">MDI backdrop follows</text>
  <text x="610" y="152" text-anchor="middle" class="sub">menu check follows</text>

  <line x1="220" y1="110" x2="270" y2="110" class="arr"/>
  <line x1="470" y1="110" x2="520" y2="110" class="arr"/>
</svg>
<figcaption style="font-size:12px;color:#555;">The Qt-free <code>RTheme</code> holds the color
tables and the resolution rule; <code>RThemeManager</code> turns them into a live QPalette;
<code>RManager</code> wires it to the menu and the widgets.</figcaption>
</figure>
```

### The Qt-free model: `RTheme`

`RTheme` keeps everything that does not need Qt: the `ThemeMode` (System, Light,
Dark) and `ThemeVariant` (Light, Dark) enums, a plain `ThemeColor` (three bytes)
and a `ThemePalette` table of the color roles the pilot uses, the curated light
and dark tables, and the rule `resolveThemeVariant(mode, os_prefers_dark)` that
maps a requested mode to a concrete variant. Because it never mentions Qt, it
compiles into `test_nopython` and is unit tested without a display.

### The Qt adapter: `RThemeManager`

`RThemeManager` installs Fusion once, copies a `ThemePalette` field-by-field into
a `QPalette` (including the disabled color group, so greyed-out controls stay
legible), and applies it to the `QApplication`. In **Follow system** mode it
reads `QStyleHints::colorScheme()` and re-applies on `colorSchemeChanged`,
deferring the repaint by one event-loop turn because the old palette is still in
effect when that signal fires.[^signal] On Qt 6.8 and newer it also hints the
platform scheme with `setColorScheme`, so the native macOS titlebar tracks a
forced Light or Dark choice; the applied palette carries the theme on Linux,
where that hint does nothing. Each application emits `themeChanged`, so widgets
that cache colors can refresh.

### Wiring in `RManager`

`RManager` applies the theme at the top of `setUp()`, before any widget is
built, so every widget is created already themed. It adds the exclusive
**View > Theme** group, and it connects `themeChanged` to two follow-ups: it
repaints the `QMdiArea` backdrop (which paints itself from its own brush rather
than the palette) with the window color, and it keeps the menu's radio check in
step with the manager however the theme moves, a menu click, a console
`set_theme()`, or an operating-system change.

### The palette

Both variants are neutral, low-saturation greys lifted a step off pure black and
pure white so large fills do not glare, sharing one calm blue accent. The values
track the conventions of well-regarded cross-platform themes rather than any
single platform.

| Role | Light | Dark |
| --- | --- | --- |
| Window | `#f2f2f3` | `#2d2f33` |
| Window text | `#1c1e21` | `#e6e6e7` |
| Base (inputs) | `#ffffff` | `#232427` |
| Alternate base | `#f6f6f7` | `#2b2d31` |
| Button | `#eaeaec` | `#35373b` |
| Highlight (accent) | `#3574f0` | `#3d82e0` |
| Placeholder text | `#9aa0a6` | `#808489` |
| Link | `#1a5fb4` | `#3daee9` |

```{raw} html
<figure style="margin:1.5em 0;">
<svg viewBox="0 0 720 250" xmlns="http://www.w3.org/2000/svg" role="img"
     aria-label="Pilot window mockup in light and dark" style="max-width:100%;height:auto;font-family:sans-serif;">
  <!-- Light window -->
  <g>
    <rect x="10" y="20" width="330" height="210" rx="6" fill="#f2f2f3" stroke="#c8cbcf"/>
    <rect x="10" y="20" width="330" height="24" rx="6" fill="#eaeaec"/>
    <text x="26" y="37" font-size="11" fill="#1c1e21">File  Edit  View  Mesh  Canvas</text>
    <rect x="24" y="58" width="150" height="120" fill="#111417" stroke="#c8cbcf"/>
    <line x1="99" y1="58" x2="99" y2="178" stroke="#c9b458" stroke-width="1"/>
    <line x1="24" y1="118" x2="174" y2="118" stroke="#c9b458" stroke-width="1"/>
    <rect x="24" y="58" width="150" height="16" fill="#3574f0"/>
    <text x="32" y="70" font-size="9" fill="#ffffff">2D canvas</text>
    <rect x="24" y="190" width="306" height="30" fill="#ffffff" stroke="#c8cbcf"/>
    <text x="30" y="209" font-size="10" fill="#9aa0a6" font-family="monospace">Enter to execute.</text>
    <text x="175" y="248" text-anchor="middle" font-size="12" fill="#555">Light</text>
  </g>
  <!-- Dark window -->
  <g>
    <rect x="380" y="20" width="330" height="210" rx="6" fill="#2d2f33" stroke="#1c1d20"/>
    <rect x="380" y="20" width="330" height="24" rx="6" fill="#35373b"/>
    <text x="396" y="37" font-size="11" fill="#e6e6e7">File  Edit  View  Mesh  Canvas</text>
    <rect x="394" y="58" width="150" height="120" fill="#111417" stroke="#1c1d20"/>
    <line x1="469" y1="58" x2="469" y2="178" stroke="#c9b458" stroke-width="1"/>
    <line x1="394" y1="118" x2="544" y2="118" stroke="#c9b458" stroke-width="1"/>
    <rect x="394" y="58" width="150" height="16" fill="#3d82e0"/>
    <text x="402" y="70" font-size="9" fill="#ffffff">2D canvas</text>
    <rect x="394" y="190" width="306" height="30" fill="#232427" stroke="#1c1d20"/>
    <text x="400" y="209" font-size="10" fill="#808489" font-family="monospace">Enter to execute.</text>
    <text x="545" y="248" text-anchor="middle" font-size="12" fill="#555">Dark</text>
  </g>
</svg>
<figcaption style="font-size:12px;color:#555;">The same window under both palettes. The menu
bar, the console strip (previously forced white), and the MDI backdrop all switch together;
the accent tints the active sub-window title.</figcaption>
</figure>
```

## Implementation

New files:

- `cpp/solvcon/pilot/RTheme.hpp`, `RTheme.cpp`: the Qt-free model and the two
  curated color tables.
- `cpp/solvcon/pilot/RThemeManager.hpp`, `RThemeManager.cpp`: the Qt adapter.
- `gtests/test_nopython_pilot_theme.cpp`: unit tests for the model.
- `tests/test_pilot_theme.py`: end-to-end tests through the menu and palette.

Changed files:

- `RManager.hpp`, `RManager.cpp`: own the manager, apply before widgets, add the
  **View > Theme** group, follow the theme for the MDI backdrop and the menu
  check.
- `RPythonConsoleDockWidget.cpp`: drop the forced light palette so the console
  follows the theme.
- `wrap_pilot.cpp`: expose `set_theme(mode)`, `theme_mode`, and `theme_variant`
  so the theme is scriptable from the console.
- `cpp/solvcon/pilot/CMakeLists.txt`, `gtests/CMakeLists.txt`: build the new
  sources, and compile the Qt-free `RTheme.cpp` into `test_nopython`.

## Verification

- **Unit (C++, no GUI):** seven `PilotTheme*` cases in `test_nopython` cover the
  resolution rule, the id round-trip and fallback, the labels, and that the
  light and dark tables differ and are not swapped. Runs under CI's headless
  build.
- **End-to-end (Python):** six `ThemeMenuTC` cases build the menu bar, trigger
  the Light and Dark actions, and assert the live `QApplication` palette turns
  dark or light, plus the scriptable `set_theme` API and the menu-check follow.
- **Headless render:** the pilot was driven under `QT_QPA_PLATFORM=offscreen`
  to grab the window in each theme and confirm the console, menu bar, and MDI
  backdrop all switch.
- **Lint:** `flake8`, ASCII, trailing-whitespace, `clang-format`, include
  order, and a diff-scoped `clang-tidy` all pass.

## Cross-platform notes

- **Linux:** forcing a scheme through `QStyleHints::setColorScheme` is a no-op on
  most desktops, so the manual Light/Dark override is carried entirely by the
  applied palette, which does work.[^colorscheme]
- **macOS:** on Qt 6.8 and newer the `setColorScheme` hint lets the native
  titlebar follow a forced mode. The app should not set
  `NSRequiresAquaSystemAppearance` to true in its `Info.plist`, or the titlebar
  would opt out of dark mode.[^macos]

## Out of scope

Deliberately left for later so the prototype stays small:

- The `QPalette::Accent` role (Qt 6.6) and a thin supplemental stylesheet for a
  tooltip border and focus ring.
- Teaching the console syntax highlighter and any custom `paintEvent` colors to
  recolor from `themeChanged`.
- Persisting the chosen theme across sessions.

## Delivery status

- **Branch:** `feat/beautify`.
- **Tests:** 7 gtest cases, 6 pytest cases, all green; full suites unaffected.
- **CI:** a draft pull request exercises the matrix.
- **Preview:** this page is served for review while the work is in progress.

## Appendix: development history

The prompts that drove this work and what each produced:

- *"Research the most elegant UI widget design on both Linux and Mac using Qt,
  support switching light and dark theme, make a prototype."* Set the goal:
  survey current Qt theming practice, then build a Fusion-plus-palette theme
  with a light/dark/follow-system switch. Produced `RTheme`, `RThemeManager`,
  the **View > Theme** menu, and the console reconciliation.
- *"Make a new devplan and serve; include the devplan in the PR."* Produced this
  page and its figures, added to the served documentation and to the pull
  request.

[^fusion]: Qt documentation, *Fusion Style*: platform-agnostic, uses the system
    palette, and honors a custom palette on every platform.
    <https://doc.qt.io/qt-6/qtquickcontrols-fusion.html>

[^colorscheme]: `QStyleHints::setColorScheme` is documented as not supported on
    all platforms, and is a no-op on most Linux desktops.
    <https://doc.qt.io/qt-6/qstylehints.html>,
    <https://bugreports.qt.io/browse/QTBUG-129917>

[^signal]: Qt documentation, `QStyleHints`: when `colorSchemeChanged` is emitted
    the old palette is still in effect, so a custom palette should be re-applied
    after the signal returns. <https://doc.qt.io/qt-6/qstylehints.html>

[^macos]: Apple, *NSAppearance*: build against a current SDK and leave
    `NSRequiresAquaSystemAppearance` unset (or false) to follow the system
    appearance. <https://developer.apple.com/documentation/AppKit/NSAppearance>

<!-- vim: set ft=markdown ff=unix fenc=utf8 et sw=2 ts=2 sts=2 tw=79: -->
