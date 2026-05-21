# Vanta

Vanta is an agent-native IDE platform.

The goal is not to embed a chat assistant into a C++ editor. Vanta exposes
structured IDE semantics to AI agents so they can understand and operate on a
workspace through the same core model that powers the UI.

```text
AI agent
  -> Vanta Core
  -> Workspace / Project / Index / Language / Build / Execution / ChangeSet
  -> UI projection and approval when needed
```

C++ is the first proving ground because it makes structured context valuable:
compiler arguments, include paths, macros, generated files, diagnostics,
multi-target builds, and module boundaries all affect whether an edit is
correct. The platform is designed to support other project families through the
same provider and plugin model.

## Position

Vanta is an agent-native IDE platform where AI agents operate on structured IDE
semantics instead of raw files and shell output.

Most coding agents can read files, grep text, ask LSP for symbols, run shell
commands, and apply patches. Vanta aims to give agents a richer IDE kernel:

- workspace and virtual file access
- project model, facets, modules, and attachments
- language resolution and language services
- compiler and index data
- build plans, execution targets, jobs, and diagnostics
- run configurations and debug sessions
- Git state and change sets
- auditable agent operations and approval flow

## Architecture

Vanta is split into a headless core, an IDE/UI layer, lightweight clients, and
plugins.

```text
Vanta Core
  Workspace / Project / Document / Language / Index
  Build / Execution / Job / Debug / Git
  Agent / ChangeSet / Settings / Plugin lifecycle

Vanta IDE
  UI state / layout / editor tabs / command palette
  Future Qt desktop frontend

Clients
  CLI
  Headless agent session
  Remote or CI session

Plugins
  Language providers
  Build providers
  Project model providers
  Execution providers
  Index providers
  Agent tools and context providers
  UI and command contributions
```

`WorkspaceContext` is the public capability surface for plugins, components,
providers, agents, and UI-facing command code. `WorkspaceRuntime` owns lifecycle
and concrete service instances behind that context.

UI clients should react to core events and state projections. Agents should
operate on Vanta Core directly rather than driving UI automation. Agent edits
should create `ChangeSet` values, which can be previewed, approved, applied, and
rolled back.

UI session state, including tabs, active editor state, keybindings, and command
palette projection, lives in the IDE/UI layer rather than `WorkspaceRuntime`.
The CLI is a core debugging shell and assembles `WorkspaceRuntime` directly
without depending on UI state.

## First-Stage Scope

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

## Plugin Model

Built-in plugins may run in process and use C++ interfaces directly.
Built-in plugin packages live under `plugins/builtin/vanta.*`; their manifests,
resources, and implementation sources stay with the package, while the current
build still compiles them into the core target.

Third-party plugin boundaries should remain ABI-stable:

- Stable native plugins should use C handles and function tables.
- Cross-language plugins should use out-of-process RPC.
- Public binary plugin ABI should not expose STL containers, smart pointers, or
  virtual-class binary boundaries.

Provider interfaces are the main extension points:

- `ProjectModelProvider`
- `LanguageService`
- `BuildProvider`
- `ExecutionProvider`
- `IndexProvider`
- `DebugProvider`
- model providers, agent tools, and agent context providers

## Build

```sh
cmake -S . -B build
cmake --build build --target vanta
```

CMake options:

- `BUILD_VANTA_STATIC`: build static libraries, enabled by default.
- `BUILD_VANTA_SHARED`: build shared libraries, disabled by default.
- `BUILD_VANTA_TESTS`: build tests, enabled by default.

CMake targets:

- `vanta_core_static`: static headless IDE platform kernel.
- `vanta_core_shared`: shared headless IDE platform kernel when
  `BUILD_VANTA_SHARED` is enabled.
- `vanta_core`: alias to the enabled core target used by in-tree targets.
- `vanta_ide`: IDE/UI layer for the future Qt frontend.
- `vanta_cli`: CLI debugging shell. The executable output is still named
  `vanta`.

## Tests

```sh
cmake --build build --target vanta_tests
./build/vanta_tests
```

## Architecture Docs

- [Agent-native platform](docs/architecture/agent-native-platform.md)
- [Service boundaries](docs/architecture/services.md)
- [Data boundaries](docs/architecture/data-boundaries.md)
- [Settings](docs/architecture/settings.md)
