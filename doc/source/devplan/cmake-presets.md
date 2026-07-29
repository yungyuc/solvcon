# CMake Presets Development Plan

solvcon's build arguments are written out in four places that nothing keeps in
agreement: the root `Makefile`, `CMakePresets.json`, `build.ps1`, and the CI
workflow files. They have already drifted, and none of them is readable by an
IDE.

This plan makes `CMakePresets.json` the single source of truth for build
configuration, and makes a contributor who opens the repository in
[VS Code](https://github.com/microsoft/vscode-cmake-tools) or
[CLion](https://www.jetbrains.com/help/clion/cmake-presets.html) get a correct
build by selecting a preset. It states the goal, the steps that reach it
without breaking an existing workflow, and the features those steps move.

## Goal

One checked-in preset file is the single source of truth for build
configuration. The Makefile, `build.ps1`, and the CI workflows become thin
drivers over it, and both supported IDEs read it directly.

- Goal 1: Layering. A hidden `base` preset carries the knobs that are the same
  everywhere (`HIDE_SYMBOL`, `DEBUG_SYMBOL`, `LINT_AS_ERRORS`,
  `CMAKE_EXPORT_COMPILE_COMMANDS`, the library output directory). Hidden
  per-host presets add the generator and the `condition`. Visible leaf presets
  name a build type and a purpose.
- Goal 2: Naming. `<host>-<purpose>`, with the host omitted where the preset
  is portable: `dev-rel`, `dev-dbg`, `dev-noqt`, `win-rel`, `win-dbg`, `lint`,
  `sanitize`, and a `ci-` prefix for the configurations CI runs.
- Goal 3: Machine-specific values stay out. Dependency-prefix paths
  (`PYTHON_EXECUTABLE`, `pybind11_path`, `CMAKE_PREFIX_PATH`) and compiler
  launchers (`ccache`, `sccache`) do not belong in a checked-in preset. They
  go to `CMakeUserPresets.json` for developers, and to the CI job's `-D`
  overrides or its own `ci-` presets for the runners.
- Goal 4: CTest is the test entry point. The gtest binary is already
  discovered; adding `add_test()` for the two pytest suites makes `ctest`
  cover everything, after which test presets and workflow presets describe the
  full loop.
- Goal 5: An IDE contributor gets a working configuration by selecting a
  preset. No IDE settings file, no manually reproduced flags, no wrapper
  script.

### Design rules

These rules are binding on every step. Each names the constraint it answers,
and all of them are cheap to follow from the start and expensive to
retrofit.

- Rule 1: Cap the schema at version 10. Versions 11 and 12 raise the CMake
  floor above the project's own, and 11 already puts the file outside what
  CLion reads. Version 10 is the common ceiling.
- Rule 2: Filter twice. Every host-specific preset carries both a CMake
  `condition` on `${hostSystemName}` (honoured by the command line and by
  CLion) and a `hostOS` entry in the Microsoft vendor map (honoured by VS
  Code and Visual Studio). Neither alone hides the preset everywhere.
- Rule 3: Use only `equals` and `notEquals` in conditions. CLion supports no
  other condition type, so `inList`, `matches`, and `anyOf` would silently
  change which presets a CLion user sees.
- Rule 4: Keep configure and build presets self-sufficient. They are all CLion
  reads. Test and workflow presets are additive conveniences for the command
  line, CI, and VS Code, never the only way to reach a capability.
- Rule 5: Give every visible preset a `displayName` and a `description`. They
  are the preset picker's only labels. Conversely, keep intermediate presets
  `hidden`, which both IDEs honour, so the picker lists only presets a person
  would actually choose.
- Rule 6: Put the run environment in the preset. `PYTHONPATH` pointing at the
  source directory belongs in the configure preset's `environment` map, so a
  target launched from the IDE behaves like one launched from the Makefile.
- Rule 7: Make `CMakeUserPresets.json` the documented seam for machine paths.
  Both IDEs read it automatically and it implicitly includes
  `CMakePresets.json`, so a developer's dependency prefix reaches the IDE
  with no IDE-specific configuration at all. It must be gitignored and it
  must be documented, because in an IDE there is no wrapper script to fall
  back on.
- Rule 8: Do not ship IDE settings that fight the presets. When preset
  integration is active, VS Code CMake Tools ignores `cmake.buildDirectory`,
  `cmake.generator`, and `cmake.configureSettings` in `settings.json`, and
  disables kit selection entirely. Any `.vscode/settings.json` the repository
  grows must respect that.
- Rule 9: One build directory per configure preset. Both IDEs assume it. This
  settles the binary-directory question in favour of
  `${sourceDir}/build/${presetName}` over the Makefile's ABI-tagged path,
  leaving step 5 to pin the interpreter.

Two CLion vendor keys are worth using: `toolchain`, naming which of the user's
configured toolchains a preset expects, and `enablePythonIntegration`, which
passes the IDE's selected Python interpreter to the configure step. The latter
is directly relevant, since `PYTHON_EXECUTABLE` is the one cache variable
solvcon cannot hardcode.

### Preset skeleton

A sketch of the shape, not the final content:

```json
{
  "version": 10,
  "$schema": "https://json.schemastore.org/cmake-presets.json",
  "cmakeMinimumRequired": { "major": 4, "minor": 0, "patch": 1 },
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "HIDE_SYMBOL": { "type": "BOOL", "value": "ON" },
        "DEBUG_SYMBOL": { "type": "BOOL", "value": "ON" },
        "LINT_AS_ERRORS": { "type": "BOOL", "value": "ON" },
        "CMAKE_LIBRARY_OUTPUT_DIRECTORY": "${sourceDir}/solvcon"
      },
      "environment": {
        "PYTHONPATH": "${sourceDir}"
      },
      "vendor": {
        "jetbrains.com/clion": { "enablePythonIntegration": true }
      }
    },
    {
      "name": "win-base",
      "hidden": true,
      "inherits": "base",
      "generator": "Ninja",
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Windows"
      },
      "vendor": {
        "microsoft.com/VisualStudioSettings/CMake/1.0": {
          "hostOS": ["Windows"],
          "intelliSenseMode": "windows-msvc-x64"
        }
      }
    },
    {
      "name": "win-rel",
      "inherits": "win-base",
      "displayName": "Windows Release (Ninja, MSVC)",
      "description": "Release _solvcon and pilot against an MSVC toolset",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    }
  ],
  "buildPresets": [
    {
      "name": "win-rel",
      "configurePreset": "win-rel",
      "targets": ["_solvcon", "pilot"]
    }
  ],
  "testPresets": [
    { "name": "win-rel", "configurePreset": "win-rel" }
  ],
  "workflowPresets": [
    {
      "name": "win-rel",
      "steps": [
        { "type": "configure", "name": "win-rel" },
        { "type": "build", "name": "win-rel" },
        { "type": "test", "name": "win-rel" }
      ]
    }
  ]
}
```

## Implementation steps

Each step is a separate pull request, each leaves every existing workflow
working, and each is verifiable on its own. Every step declares its scope as
a set of the features listed in the next section rather than as prose:
*Moves* names the features whose destination changes, *Touches* names the
features that stay where they are but get edited, and *Needs* names the
steps that must land first. The coverage table below accounts for all
seventeen features, so a feature that no step claims is a gap in the plan
rather than an oversight in the reading.

### Step 1: Modernize the file without changing the build

*Moves:* feature 6. *Needs:* nothing.

Raise `"version"` to 10, add `$schema`, retype the cache variables, and add
`$comment` entries recording why each knob is set. Add the presentation and
filtering metadata in the same pass, since none of it touches the build: a
`description` next to each `displayName`, a `condition` on `${hostSystemName}`
for the Windows presets, and a `microsoft.com/VisualStudioSettings/CMake/1.0`
vendor map carrying `hostOS` and `intelliSenseMode`.

Feature 6 is the only one that is new capability rather than migrated
capability, which is why it can land first: there is no Makefile behaviour to
preserve.

*Verify:* `cmake --list-presets` shows the Windows presets on Windows and
omits them elsewhere; a Windows configure and build produce the same cache as
before, compared with `cmake -LAH`. Open the repository in VS Code and in
CLion on Linux and confirm neither offers an MSVC preset.

*Risk:* none to the build. The only externally visible change is which
presets are listed on which host.

### Step 2: Close the knob gap with the Makefile

*Moves:* features 1 and 4. *Needs:* step 1.

Introduce the hidden `base` preset carrying `HIDE_SYMBOL`, `DEBUG_SYMBOL`,
`LINT_AS_ERRORS`, `SKIP_PYTHON_EXECUTABLE`, and `SOLVCON_PROFILE` at the
Makefile's values, and have the Windows presets inherit it. Decide
`USE_GOOGLETEST` and `BLA_VENDOR` deliberately: either match the project
default and let `build.ps1` and CI opt out, or keep the override and record
the reason in a `$comment`. Give `base` an `environment` map setting
`PYTHONPATH` to `${sourceDir}`, the preset counterpart of the Makefile's
`RUNENV`, so a target launched from an IDE finds the in-tree package.

Feature 1 is the largest of the set and the one the rest of the plan rests on,
so it moves whole rather than knob by knob. The Makefile keeps passing its own
`-D` flags until step 8; the two agreeing is the point of the check below.

*Verify:* diff `cmake -LAH` output between a make-configured tree and a
preset-configured tree on Linux; the two should differ only in the knobs that
are genuinely platform-specific. For feature 4, run a preset-built target that
imports the in-tree package with no `PYTHONPATH` in the calling shell.

*Risk:* turning on `HIDE_SYMBOL` changes the symbol visibility of the Windows
`_solvcon`. On MSVC visibility attributes are inert, so the change is a no-op
there, but it should be confirmed by running the pytest suite once.

### Step 3: Establish the `CMakeUserPresets.json` seam

*Moves:* feature 7. *Touches:* feature 17. *Needs:* step 2.

This is the step that makes the IDEs work, so it comes before the mechanical
tidying below. Document the file in the developer guide with a worked example
for a dependency prefix, ship a copyable template under `contrib/`, add it to
`.gitignore`, and stop `build.ps1` from injecting the three path variables
when the user file already supplies them. Record the CLion
`enablePythonIntegration` key here as the alternative to writing
`PYTHON_EXECUTABLE` out by hand.

Feature 7 is the only one whose destination is `CMakeUserPresets.json`, and
the only one that cannot be checked in, so its correctness rests entirely on
documentation rather than on a file in the repository.

*Verify:* a fresh checkout plus a user preset configures with a bare
`cmake --preset <name>`, and the same checkout configures from the VS Code and
CLion preset pickers with no IDE settings touched. Without a user preset,
`build.ps1` and the Makefile still work as they do today.

*Risk:* low, since the fallback path in `build.ps1` stays. The real work is
documentation, and it is the documentation that determines whether the IDE
story actually holds for a new contributor.

### Step 4: Make the build presets carry the build

*Moves:* feature 5. *Touches:* feature 17. *Needs:* step 2.

Move `targets` into the build presets and add `jobs`. Rewrite the target
selection in `build.ps1` as a choice between build presets rather than an
array assembled in PowerShell, and have the script read `binaryDir` from the
JSON instead of recomputing `build\$preset`. Build presets are the second and
last preset kind CLion reads, so name them for what a person would pick:
one that builds the module alone, one that adds the pilot.

*Verify:* every `build.ps1` switch combination (`-NoQt`, `-Gtest`, `-Test`,
`-PilotTest`, `-Sanitize`) still produces and runs the same binaries; the
build presets appear in both IDE pickers and are filtered to the active
configure preset.

*Risk:* the switch matrix in `build.ps1` is the delicate part; the sanitizer
paths in particular assert on combinations that must keep throwing.

### Step 5: Host presets for Linux and macOS

*Moves:* features 2 and 3. *Needs:* steps 2 and 3.

Add `dev-rel`, `dev-dbg`, and `dev-noqt`, conditioned on `Linux` and
`Darwin`, inheriting `base`. Resolve the binary-directory question here: either
keep `${sourceDir}/build/${presetName}` and document that switching
interpreters needs `--fresh`, or derive an ABI-tagged directory the way the
Makefile does.

Feature 3 is the one a preset cannot fully absorb, because `BUILD_PATH`
carries an ABI tag computed by `$(shell ...)`. Whatever is decided here binds
feature 15, which stays in the Makefile either way but only stays *necessary*
if the ABI tag is kept. Settle both in this step rather than leaving feature
15's fate implicit.

*Verify:* configure, build, `pytest tests/`, and `run_pilot_pytest` from the
preset trees on both hosts, alongside the existing make targets. This is the
step where IDE accordance becomes real for most contributors, so verify it
from the IDEs as well: pick `dev-dbg` in VS Code and in CLion on Linux and on
macOS, build the pilot, and run it, all without editing IDE settings.

*Risk:* the interpreter question is the real one. `PYTHON_EXECUTABLE` cannot
be a static value in a checked-in preset; it comes from the user file, from
CLion's Python integration, from `$penv{}`, or stays a command-line override.

### Step 6: Register the test suites with CTest

*Moves:* features 9 and 10. *Touches:* feature 16. *Needs:* step 5.

Add `add_test()` for `pytest tests/` and for `pilot --mode=pytest`, with the
environment (`PYTHONPATH`, `QT_QPA_PLATFORM`, `SOLVCON_TEST_SHOW_WINDOWS`)
set through test properties. The last of those is why the environment has to
be per-test rather than per-preset: `tests/conftest.py` keeps GUI windows off
the screen unless it is set, and the workflows set it for the jobs that run
the pilot suite while leaving it default for the job that runs the plain one.
Then add test presets that select them by label. Keep `run_gtest`,
`run_pilot_pytest`, and `make pytest` working as thin wrappers.

Feature 10 rides along because it is the same kind of change and removes the
same kind of duplication: a custom target in
`cpp/binary/pymod_solvcon/CMakeLists.txt` deletes the built extension using
the `PYEXTSUFFIX` that file already computes, so `clean` and `cmakeclean` stop
shelling out to `python3-config` a second time. This is the first step that
touches feature 16, and it sets the pattern step 8 generalizes: the make
target keeps its name and becomes a one-line wrapper.

*Verify:* `ctest --preset <name>` runs the C++ and Python suites and reports
the same pass and fail set as the existing targets, on all three hosts; VS
Code lists the test presets and runs them from the Project Status view. For
feature 10, `make clean` removes the same files it removes today.

*Risk:* the pilot suite needs a display policy and a library path; both are
already solved in the Makefile and the workflows, and the values move into
test properties. Note that CLion reads no test presets, so a CLion user
reaches the suites through the CTest integration against the configured build
directory rather than through a preset. The `add_test()` registration is what
makes that work, and it is the part of this step that benefits CLion.

### Step 7: Workflow presets and the CI cut-over

*Moves:* feature 8. *Needs:* steps 4 and 6.

Add workflow presets chaining configure, build, and test, plus `ci-` configure
presets matching what the workflows build. Replace the hand-written
`cmake -GNinja -D...` blocks in `devbuild.yml` with `cmake --workflow`,
keeping the compiler-launcher and dependency-path overrides as `-D` flags on
the configure step.

The dependency on step 6 is not optional: a workflow preset's third stage is a
test preset, and there is nothing for it to run until the suites are
registered. Attempting this step earlier yields workflows that configure and
build only, which is not worth the churn.

Two IDE consequences fall out here. Workflow presets are invisible to CLion,
which is acceptable because they are a CI and command-line convenience rather
than a capability. The `ci-` configure presets are not: a preset that CI names
on the command line cannot be `hidden`, so it will appear in both IDE pickers
whether or not anyone wants it there. The `ci-` prefix plus a description
saying so is the minimum; moving them to a separate file pulled in with
`include` is the alternative, at the cost of a second file to keep in step.

*Verify:* a full CI run on every job, compared against the previous run's
configure log for cache-variable equality; the IDE pickers still lead with the
developer presets.

*Risk:* the highest of the plan. It should land last and be split per job if
the diff is large.

### Step 8: Make the Makefile a preset driver

*Touches:* feature 16. *Leaves alone:* features 12 to 15. *Needs:* steps 2
to 6.

Have the Makefile's configure step call `cmake --preset` with a preset chosen
from `CMAKE_BUILD_TYPE` and `BUILD_QT`, keeping every target name and every
`make VAR=value` override working through command-line `-D` flags layered on
the preset.

This step moves nothing. Feature 16 says the make targets keep their names,
and that is exactly what is preserved here; what changes is only what sits
behind them. Features 12 to 15 are explicitly out of scope: the lint targets
in particular must keep running without a configure, so they are not rewired
to go through a preset, and a pull request for this step that touches them has
overreached.

*Verify:* the full documented make surface, including the `PYTEST_OPTS`
forwarding and the lint targets. Confirm `make flake8` still succeeds in a
checkout that has never been configured.

*Risk:* the Makefile is the interface every contributor uses; this step is
worth deferring until steps 1 through 7 have settled, and it can be dropped
without losing the rest of the plan.

### Coverage

| Step | Moves     | Touches | Leaves alone |
| ---- | --------- | ------- | ------------ |
| 1    | 6         |         |              |
| 2    | 1, 4      |         |              |
| 3    | 7         | 17      |              |
| 4    | 5         | 17      |              |
| 5    | 2, 3      |         |              |
| 6    | 9, 10     | 16      |              |
| 7    | 8         |         |              |
| 8    |           | 16      | 12 to 15     |

Feature 11 is deliberately unscheduled: `make pyprof` needs a driver script
wherever it lives, so it is revisited after step 6 or not at all. Features 12
to 15 and 17 are never moved. Every other feature is claimed by exactly one
step, and no step claims a feature twice.

### Verifying IDE accordance

IDE behaviour is not covered by CI and cannot be, so it is checked by hand.
The same short pass applies to every step that touches the preset file, on a
clean clone with no IDE settings and no prior build directory:

1. Open the repository. VS Code CMake Tools activates preset mode on its own
   because `CMakePresets.json` exists; CLion imports the presets as read-only
   profiles, with new ones disabled until enabled.
2. Check the picker contents. Only the leaf presets for the current host
   appear, each with a readable name and description. No `base`, no
   `win-base`, no MSVC preset on Linux.
3. Configure from the picker with no other setup beyond the documented
   `CMakeUserPresets.json`. It must succeed, including `find_package(pybind11
   ...)`.
4. Build the pilot from the picker's build preset, then run it. It must
   import the in-tree package, which is what the preset `environment` map
   buys.
5. Confirm code navigation works, which is what `CMAKE_EXPORT_COMPILE_COMMANDS`
   (already on in `CMakeLists.txt`) and `intelliSenseMode` are for.

A step is not done until this passes in both IDEs on the hosts that step
claims to serve.

## Features

A feature is one piece of the build surface with one destination. The
implementation steps above are scoped by feature number, so this table is
the plan's unit of work.

Presets are a declarative configuration format: they cannot run a command,
expand a shell substitution, or name anything that is not already a CMake
target, and their macro expansion is a fixed set plus `$env{}` and `$penv{}`.
So the destination follows a simple criterion. Presets hold declarative
configuration, `CMakeLists.txt` holds what must run and can be a node in the
build graph, and the Makefile keeps what is not a CMake build at all or needs
a shell.

| ID | Feature                | Destination  | Purpose                       |
| -- | ---------------------- | ------------ | ----------------------------- |
| 1  | Configure knobs        | presets      | How the build is configured   |
| 2  | Generator, build type  | presets      | Which toolchain and config    |
| 3  | Binary directory       | presets      | Where a build tree lives      |
| 4  | Run environment        | presets      | In-tree package importable    |
| 5  | Targets, parallelism   | presets      | What to build, and how wide   |
| 6  | Host and IDE metadata  | presets      | Preset filtering and labels   |
| 7  | Machine paths          | user presets | The local dependency prefix   |
| 8  | CI configure arguments | presets      | One definition CI can name    |
| 9  | Python test suites     | CMakeLists   | Register the tests with CTest |
| 10 | Module removal         | CMakeLists   | Delete the built extension    |
| 11 | Profiling runs         | CMakeLists   | Run the profiling scripts     |
| 12 | Lint targets           | Makefile     | Checks that need no configure |
| 13 | Bundling               | Makefile     | Drive the packaging script    |
| 14 | Standalone buffer      | Makefile     | A separate, non-CMake build   |
| 15 | Interpreter arithmetic | Makefile     | The ABI suffix and version    |
| 16 | Target names           | Makefile     | The contributor-facing verbs  |
| 17 | Windows toolchain      | build.ps1    | MSVC env, CMake, module copy  |

- Feature 1: Configure knobs. The whole `CMAKE_CMD` flag block: `HIDE_SYMBOL`,
  `DEBUG_SYMBOL`, `SKIP_PYTHON_EXECUTABLE`, `BUILD_METAL`, `BUILD_QT`,
  `USE_CLANG_TIDY`, `LINT_AS_ERRORS`, `SOLVCON_PROFILE`,
  `CMAKE_INSTALL_PREFIX`, and `CMAKE_LIBRARY_OUTPUT_DIRECTORY`. These are
  `cacheVariables` and nothing else; they are the clearest case in the table.
- Feature 2: Generator and build type. Today the choice is implicit in the
  Makefile's selection of `build/rel*` against `build/dbg*`. A preset states
  it, which is also what lets an IDE offer the two as separate entries.
- Feature 3: Binary directory. `binaryDir` replaces the computed
  `BUILD_PATH`, at the cost of the ABI tag `BUILD_PATH` carries and a preset
  cannot compute.
- Feature 4: Run environment. `RUNENV`'s `PYTHONPATH` becomes the configure
  preset's `environment` map, so a target launched from an IDE or from `ctest`
  finds the in-tree package the way a make-launched one does.
- Feature 5: Targets and parallelism. The per-target `cmake --build`
  invocations become build presets with `targets`, `MAKE_PARALLEL` becomes
  `jobs`, and `lint`'s keep-going behaviour becomes `nativeToolOptions`.
- Feature 6: Host and IDE metadata. The `condition` field, the vendor maps,
  and the display strings. The Makefile has no way to express any of this, so
  this feature is new capability rather than migrated capability.
- Feature 7: Machine paths. `CMAKE_PREFIX_PATH`, `PYTHON_EXECUTABLE`, and
  `pybind11_path` go to `CMakeUserPresets.json`. They are the only values
  that must not be checked in, and the only ones an IDE cannot otherwise
  obtain.
- Feature 8: CI configure arguments. The hand-written `cmake -GNinja -D...`
  blocks in the workflow files become `ci-` presets that the workflows name.
- Feature 9: Python test suites. `make pytest` and `make run_pilot_pytest`
  become `add_test()` registrations, joining the gtest binary that
  `gtest_discover_tests` already registers. Each suite carries its own
  environment, which is what a test property is for: the GUI tests read
  `SOLVCON_TEST_SHOW_WINDOWS` and the two suites want opposite answers.
  This is what makes `ctest` the one test entry point, and it is the
  feature that most benefits CLion, which reads no test presets.
- Feature 10: Module removal. The `_solvcon` deletion in `clean` and
  `cmakeclean`. The Makefile recomputes the extension suffix with
  `python3-config` for this, while `cpp/binary/pymod_solvcon/CMakeLists.txt`
  already computes the same `PYEXTSUFFIX`; a custom target there drops a
  duplicated shell-out.
- Feature 11: Profiling runs. `make pyprof`, if it moves at all. The loop over
  `profile_*.py` with per-file output redirection needs a driver script
  either way, so this is a low-priority candidate rather than a clear win.
- Feature 12: Lint targets. `cformat` finds the C++ sources, compares the
  local clang-format major version against the CI pin, and drives `xargs -P`;
  `cinclude` is a `grep`; `checkascii` and `checktws` run
  `contrib/lint/check_ascii.py`; `flake8` runs flake8. None of them is a
  build step. Making them CMake targets would require a successful
  configure, and so pybind11, NumPy, PySide6, and Qt6 all resolving, before
  a contributor who touched only `solvcon/` or `tests/` could lint. That is
  a real regression and the strongest reason the Makefile does not
  disappear.
- Feature 13: Bundling. `bundle`, `bundle-precheck`, and `bundle-test` invoke
  `contrib/bundle/bundle-with-homebrew.sh`.
- Feature 14: Standalone buffer. `standalone_buffer_setup` and
  `standalone_buffer` recurse into `contrib/standalone_buffer` and its own
  Makefile. That build is deliberately independent of the CMake project.
- Feature 15: Interpreter arithmetic. `pyvminor` and `pyextsuffix` come from
  `$(shell ...)`. If the ABI-tagged build path is kept, this has no preset
  equivalent and stays where it is.
- Feature 16: Target names. Whatever moves, the make targets contributors
  already use should keep working as thin wrappers over the preset
  invocations.
- Feature 17: Windows toolchain. `build.ps1` keeps everything that is not
  configuration: importing the MSVC environment through `vcvars64.bat`,
  locating a CMake new enough for the project, activating the dependency
  prefix, copying the built module to the repository root, and launching the
  test and GUI runs.

The Makefile therefore shrinks substantially but does not go away, which is
why step 8 is optional. Replacing it outright would mean adopting a different
task runner, not adopting presets.

## References

The CMake documentation defines the preset format and the commands that
consume it.[^presets] [^cmake1] The IDE limits that Rules 1 to 5 encode come
from the CLion documentation,[^clion] the VS Code CMake Tools
documentation,[^vscode] and the Microsoft vendor-map reference.[^msvendor]
The layering and the CI staging follow community practice.[^softwarecraft]
[^moderncpp]

[^presets]: *cmake-presets(7)*, CMake documentation.
    <https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html>

[^cmake1]: *cmake(1)*, CMake documentation, on `--preset`,
    `--list-presets`, `--build --preset`, `--workflow`, and `--fresh`.
    <https://cmake.org/cmake/help/latest/manual/cmake.1.html>

[^clion]: *CMake presets*, CLion documentation.
    <https://www.jetbrains.com/help/clion/cmake-presets.html>

[^vscode]: *CMake Presets*, vscode-cmake-tools documentation.
    <https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/cmake-presets.md>

[^msvendor]: *CMakePresets.json and CMakeUserPresets.json Microsoft vendor
    maps*, Microsoft Learn.
    <https://learn.microsoft.com/en-us/cpp/build/cmake-presets-json-reference>

[^softwarecraft]: *Organizing CMake presets*, SoftwareCraft.
    <https://softwarecraft.ch/cmake-presets-best-practices/>

[^moderncpp]: *Optimizing CI Build Scripts and Enhancing Developer Experience
    with CMake Presets*, Modern C++ DevOps.
    <https://moderncppdevops.com/simple-ci-with-presets/>

<!-- vim: set ft=markdown ff=unix fenc=utf8 et sw=2 ts=2 sts=2 tw=79: -->
