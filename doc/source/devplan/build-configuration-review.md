# Build Configuration Review

solvcon builds through CMake, but the arguments handed to CMake are not
written down in one place. This page records where they are written, where
the copies disagree, and what the preset file does not currently say. It
describes the repository as it stands.

## The configuration is written out four times

- `Makefile` at the repository root, the driver for Linux and macOS. It owns
  `BUILD_PATH`, expands a fixed `CMAKE_CMD` with twelve `-D` flags, and
  wraps every developer workflow (`buildext`, `pilot`, `gtest`, `pytest`,
  `run_pilot_pytest`, the lint targets) as a make target.
- `CMakePresets.json`, added for Windows. It defines `win-rel` and `win-dbg`
  with a small subset of the same knobs.
- `build.ps1`, the Windows driver. It selects a preset from `-BuildType`,
  recomputes the preset's binary directory, adds the dependency-prefix cache
  variables, and picks the build targets.
- The workflow files under `.github/workflows/`, which drive neither the
  Makefile nor the presets on Windows: `devbuild.yml` spells out its own
  `cmake -GNinja -D...` invocation twice.

Nothing keeps the four in agreement, and they have already drifted. The rest
of this page records how.

## The schema version is older than the project needs

The file declares `"version": 6` and `cmakeMinimumRequired` 4.0.1. Schema
version 6 is the CMake 3.25 level. Since the project already requires CMake
4.0.1, every schema feature up to **version 10** (CMake 3.31) is available at
no cost:

| Version | CMake  | What it adds                                  |
| ------- | ------ | --------------------------------------------- |
| 6       | 3.25   | package and workflow presets                  |
| 7       | 3.27   | `trace`, macro expansion inside `include`     |
| 8       | 3.28   | `$schema` for editor validation               |
| 9       | 3.30   | extended macro expansion inside `include`     |
| 10      | 3.31   | `$comment` documentation, `graphviz`          |

Versions 11 and 12 need CMake 4.3 and 4.4, above the project floor, so 10 is
the ceiling to adopt now. Version 8 also enables the `$schema` key, which is
what makes an editor validate the file while it is being edited.

## The Windows presets carry no `condition`

`win-rel` and `win-dbg` are unconditional, so `cmake --list-presets` offers
them on Linux and macOS, where the Ninja + MSVC combination they describe
cannot configure. A
[`condition`](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
comparing `${hostSystemName}` against `Windows` removes them from the listing
on other hosts. This matters more once host presets for Linux and macOS are
added and the listing has to stay readable.

## The knob set does not match the Makefile's

The Makefile passes these on every configure; the preset passes none of them,
so the preset build silently takes the `CMakeLists.txt` defaults:

| Knob                     | Makefile | `CMakeLists.txt` default | Preset  |
| ------------------------ | -------- | ------------------------ | ------- |
| `HIDE_SYMBOL`            | `ON`     | `OFF`                    | absent  |
| `DEBUG_SYMBOL`           | `ON`     | `ON`                     | absent  |
| `LINT_AS_ERRORS`         | `ON`     | `OFF`                    | absent  |
| `SKIP_PYTHON_EXECUTABLE` | `OFF`    | unset                    | absent  |
| `SOLVCON_PROFILE`        | `OFF`    | `OFF`                    | absent  |
| `USE_CLANG_TIDY`         | `OFF`    | `OFF`                    | absent  |
| `BUILD_METAL`            | `OFF`    | `OFF`                    | absent  |

`HIDE_SYMBOL` is the one that changes the artifact: the Makefile builds
`_solvcon` with hidden C++ visibility, the preset builds it with default
visibility. `LINT_AS_ERRORS` is the one that changes the diagnostics. A
developer comparing a preset build against a make build is not comparing the
same build.

## The preset does not describe what CI builds

Two cache variables disagree with the Windows CI jobs:

- `BLA_VENDOR: OpenBLAS`. Both Windows jobs in `devbuild.yml` build against
  Intel MKL instead, the `build_windows` job through `-DSOLVCON_USE_MKL=ON`
  and `sanitizer_windows` through the vcpkg toolchain. `BLA_VENDOR` only
  reaches solvcon's `find_package(LAPACK QUIET)` fallback in
  `cpp/solvcon/CMakeLists.txt`; the CBLAS search there is a hand-written
  `find_library`, so the variable does less than its presence suggests.
- `USE_GOOGLETEST: OFF`. The project default is `ON` and both Windows CI jobs
  turn it back on. `build.ps1` also has to re-add `-DUSE_GOOGLETEST=ON` for
  its `-Gtest` switch. The preset is the only place it is off.

## The build presets are empty shells

```json
{ "name": "win-rel", "configurePreset": "win-rel" }
```

A [build
preset](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) can
carry `targets` and `jobs`. Both are supplied elsewhere today: `build.ps1`
assembles the target list (`_solvcon`, `pilot`, `test_nopython`) in
PowerShell, and CI caps parallelism with `MAKE_PARALLEL: -j2` and `NPROC: 2`
in the workflow environment. Neither fact is visible to anyone reading the
preset file.

## There are no test presets, though CTest is already wired

`gtests/CMakeLists.txt` calls `enable_testing()` and
[`gtest_discover_tests(test_nopython)`](https://cmake.org/cmake/help/latest/module/GoogleTest.html),
so the C++ suite is fully registered with CTest whenever `USE_GOOGLETEST` is
on. Nothing uses it. Every caller instead builds the `run_gtest` custom
target, which runs the binary directly and loses CTest's per-test reporting,
filtering, and parallel execution.

The Python suites are not registered at all: `make pytest` shells out to
pytest, and `run_pilot_pytest` is another custom target. Their environments
have also grown apart. `tests/conftest.py` keeps GUI windows off the screen
unless `SOLVCON_TEST_SHOW_WINDOWS` is set, and the workflows now export it
for the jobs that run the pilot suite while leaving it at the default for
the job that runs the plain one, on top of the `PYTHONPATH` the Makefile
exports and the `QT_QPA_PLATFORM` the setup actions choose. Every one of
those is a per-suite value that a CTest test property could carry. With
`add_test()` wrappers, `ctest` becomes the single test entry point on every
platform, and [test
presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
become worth writing.

## There are no workflow presets

Configure, build, and test are chained by hand in `build.ps1`, in the
Makefile's target dependencies, and again in each CI job. A [workflow
preset](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
expresses the chain once and runs it with `cmake --workflow --preset <name>`.

## The binary directory drops the Python ABI tag

The Makefile computes `build/rel<pyvminor>` or `build/dbg<pyvminor>`, so two
interpreters get two build trees. The preset hardcodes `build/win-rel`. Since
`_solvcon` is an ABI-tagged extension module and `PYTHON_EXECUTABLE` is a
cache variable, reconfiguring the same preset directory against a different
interpreter reuses a stale cache. This is latent on Windows today because
`build.ps1` always configures against the active dependency prefix, but it
becomes a real hazard the moment the presets serve Linux and macOS too.

## `build.ps1` recomputes what the preset already states

The script maps `-BuildType Debug` to the name `win-dbg` and rebuilds the
binary directory as `build\$preset`. Both are restatements of JSON the script
has already located. If a preset's `binaryDir` changes, the script points at
the wrong directory and reports the wrong `pilot.exe` path. Reading the file
with `ConvertFrom-Json`, or querying CMake, removes the duplication.

## `CMakeUserPresets.json` is unused and unmentioned

`build.ps1` injects `PYTHON_EXECUTABLE`, `pybind11_path`, and
`CMAKE_PREFIX_PATH` on the command line because those paths belong to the
developer's machine. That is precisely what
[`CMakeUserPresets.json`](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
is for: it is read automatically, it implicitly includes `CMakePresets.json`,
and it is not meant to be checked in. It is absent from the file, from
`.gitignore`, and from the documentation.

## No preset is usable from an IDE

This is the finding with the widest consequences. Both `build.ps1` and the
Makefile inject the dependency-prefix cache variables (`PYTHON_EXECUTABLE`,
`pybind11_path`, `CMAKE_PREFIX_PATH`) on the command line, and the Makefile
additionally exports `PYTHONPATH` through its `RUNENV` variable before it runs
anything. An IDE runs `cmake --preset <name>` with nothing added. So:

- Configure fails at `find_package(pybind11 2.12.0 REQUIRED PATHS
  ${pybind11_path})` unless pybind11 happens to sit on the default search
  path, and `CMakeLists.txt` shells out to a bare `python3` for the PySide6
  and shiboken6 paths regardless of which interpreter the developer intends.
- Even after a successful build, running `_solvcon_py`, `run_pilot`, or
  `run_pilot_pytest` from the IDE has no `PYTHONPATH`, so the in-tree package
  is not importable. The preset schema has an `environment` map for exactly
  this, and the file uses none.

The wrapper scripts are not available to the IDE, and asking every contributor
to reproduce their flags by hand in IDE settings defeats the point of having a
preset file.

## The IDE-specific keys are all absent

Preset consumers read more than the CMake-defined keys:

- `displayName` is set on the two leaf presets but `description` is set
  nowhere. Both IDEs show both strings in their preset pickers.
- There is no `microsoft.com/VisualStudioSettings/CMake/1.0` vendor map, so VS
  Code has no `hostOS` and offers the MSVC presets on Linux and macOS. The
  CMake `condition` field fixes this for the command line and for CLion, but
  VS Code filters on the vendor `hostOS` key, so both are needed.
- There is no `jetbrains.com/clion` vendor map, so CLion binds every preset to
  its default toolchain and cannot be told which one a preset expects.
- There is no `$schema` key, so neither editor validates the file while it is
  being edited. This is available from schema version 8.

## The IDEs cap the schema version and the preset kinds

The two IDEs support narrower ranges than CMake does, and CLion is the binding
constraint on both axes:

| Consumer          | Schema versions | Preset kinds supported            |
| ----------------- | --------------- | --------------------------------- |
| CMake 4.0.1       | 1 -- 10         | all five                          |
| VS Code CMake     | 2 -- 11         | configure, build, test, package,  |
| Tools             |                 | workflow                          |
| CLion             | 2 -- 10         | configure and build only          |

Version 10 is therefore the right target, and it is the highest version that
keeps CLion working. More importantly, **CLion shows no test, package, or
workflow presets at all.** Any capability expressed only as a workflow preset
is invisible to a CLion user, so the configure and build presets have to stay
self-sufficient rather than becoming stubs that only a workflow ties
together.

## Cache variables are untyped and undocumented

`"BUILD_QT": "ON"` works, but the typed spelling
`{ "type": "BOOL", "value": "ON" }` is what makes the entry render correctly
in `ccmake` and the IDE cache editors. With schema version 10 the `$comment`
key can also record why a knob is set, which the current file has no way to
say.

## References

The preset format and its consumers are documented by CMake,[^presets] and the
CTest registration this page refers to comes from the GoogleTest CMake
module.[^gtest] The IDE limits are stated by the tools themselves: the schema
versions and preset kinds each reads, from the CLion[^clion] and VS Code CMake
Tools[^vscode] documentation, and the `hostOS` and `intelliSenseMode` keys,
from the Microsoft vendor-map reference[^msvendor] and the Visual Studio preset
guide.[^msvc]

[^presets]: *cmake-presets(7)*, CMake documentation.
    <https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html>

[^gtest]: *GoogleTest* CMake module, on `gtest_discover_tests`.
    <https://cmake.org/cmake/help/latest/module/GoogleTest.html>

[^clion]: *CMake presets*, CLion documentation.
    <https://www.jetbrains.com/help/clion/cmake-presets.html>

[^vscode]: *CMake Presets*, vscode-cmake-tools documentation.
    <https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/cmake-presets.md>

[^msvendor]: *CMakePresets.json and CMakeUserPresets.json Microsoft vendor
    maps*, Microsoft Learn.
    <https://learn.microsoft.com/en-us/cpp/build/cmake-presets-json-reference>

[^msvc]: *Configure and build with CMake Presets in Visual Studio*, Microsoft
    Learn.
    <https://learn.microsoft.com/en-us/cpp/build/cmake-presets-vs>

<!-- vim: set ft=markdown ff=unix fenc=utf8 et sw=2 ts=2 sts=2 tw=79: -->
