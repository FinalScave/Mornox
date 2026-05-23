# Mornox

Mornox is an agent-native IDE platform.

The goal is to expose structured IDE semantics to AI agents so they can operate
on a workspace through Mornox Core instead of raw files, shell output, and UI
automation.

```text
AI agent / UI / CLI
  -> Mornox Core
  -> Workspace / Project / Document / Language / Index
  -> Build / Execution / Job / Debug / Git / Agent / ChangeSet
```

C++ is the first proving ground because it makes structured context valuable:
compiler arguments, include paths, macros, generated files, diagnostics,
multi-target builds, and module boundaries all affect whether an edit is
correct. The platform is designed to support other project families through the
same provider and plugin model.

## At A Glance

- Mornox Core is a headless IDE kernel shared by agents, UI clients, CLI
  workflows, tests, remote sessions, and future CI workflows.
- `WorkspaceContext` is the public capability surface for workspace, project,
  document, language, index, build, execution, jobs, debug, Git, settings,
  plugins, agents, and change sets.
- Agents operate through typed core services and auditable operations rather
  than scraping UI state.
- Agent edits should produce `ChangeSet` values that can be previewed,
  approved, applied, and rolled back.
- Language, build-system, project-model, execution, index, debug, model, and
  agent-tool support is contributed through providers and plugins.
- UI session state lives in the IDE layer, while shared project and agent
  capabilities live in the core.

## Current Scope

The current first-stage architecture targets:

- workspace open and file-tree modeling
- virtual file system access
- document overlays and multi-tab/editor state foundations
- project modeling for workspace, single-file, and scratch origins
- CMake and `compile_commands.json` recognition
- clice/C++ language integration
- completion, hover, definition, diagnostics, and semantic tokens
- build and test execution with clickable diagnostics
- run configurations and execution targets
- agent context collection, tools, operations, and change sets
- Git diff support
- plugin lifecycle, external process plugins, and built-in plugins
- typed settings with hierarchical nodes, search, and scopes

The UI can be built as a client of these core capabilities. The core can also be
used headlessly by agents, tests, CLI workflows, or future remote sessions.

## Build

```sh
cmake -S . -B build
cmake --build build --target mornox
```

CMake options:

- `BUILD_MORNOX_STATIC`: build static libraries, enabled by default.
- `BUILD_MORNOX_SHARED`: build shared libraries, disabled by default.
- `BUILD_MORNOX_TESTS`: build tests, enabled by default.

CMake targets:

- `mornox_core_static`: static headless IDE platform kernel.
- `mornox_core_shared`: shared headless IDE platform kernel when
  `BUILD_MORNOX_SHARED` is enabled.
- `mornox_core`: alias to the enabled core target used by in-tree targets.
- `mornox_ide`: IDE/UI layer for the future Qt frontend.
- `mornox_cli`: CLI debugging shell. The executable output is still named
  `mornox`.

## Tests

```sh
cmake --build build --target mornox_tests
./build/mornox_tests
```

## Architecture Docs

- [Agent-native platform](docs/architecture/agent-native-platform.md)
- [Service boundaries](docs/architecture/services.md)
- [Data boundaries](docs/architecture/data-boundaries.md)
- [ABI boundaries](docs/architecture/abi.md)
- [Settings](docs/architecture/settings.md)
