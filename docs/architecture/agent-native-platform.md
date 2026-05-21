# Agent-Native IDE Platform

Vanta is an agent-native IDE platform. The goal is not to embed a chat agent
inside a C++ editor, but to expose structured IDE semantics to agents so they
can reason about projects through the same core model that powers the UI.

The first strong scenario is C++ because C++ makes the value of structured IDE
context obvious: compiler arguments, include paths, macros, generated files,
multi-target build graphs, diagnostics, and module boundaries all affect the
correctness of an edit. A grep-and-LSP agent can help in this environment, but
it usually lacks the project semantics needed to act reliably.

## Product Position

Vanta should be understood as:

> An agent-native IDE platform where AI agents operate on structured IDE
> semantics instead of raw files and shell output.

C++ is the first proving ground, not the platform boundary. Future language,
build-system, and project-family support should be contributed through the same
provider and plugin model used by the built-in C++, CMake, Python, clice, and
Git integrations.

## Core Principle

Vanta Core is the source of truth. UI clients and agents are clients of the
same core state and command surface.

```text
Vanta Core
  Workspace / Project / Document / Language / Index
  Build / Execution / Job / Debug / Git
  Agent / ChangeSet / Settings / Plugin lifecycle

Clients
  Desktop UI
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

Agents should operate on Vanta Core, not on UI automation. The UI reacts to core
events and state projections. When an agent opens context, proposes edits, runs
builds, or requests approval, it should do so through core APIs. The desktop UI
then renders the relevant state.

## Architectural Rules

- Core-first: capabilities that an agent, CLI, remote session, or CI job can use
  belong in Vanta Core. UI code should render and dispatch, not own core IDE
  behavior.
- Agent-visible semantics: project models, language resolution, indexes,
  diagnostics, build plans, execution targets, run configurations, jobs, debug
  state, Git state, settings, and change sets must be queryable without a UI.
- ChangeSet-first editing: agent edits should produce auditable `ChangeSet`
  values. Applying changes is a separate approval and execution step.
- Provider-based platform: CMake, clice, C++, Python, Git, and future Android or
  Java support are providers or built-in plugins, not hard-coded assumptions in
  the workspace kernel.
- Headless-ready: Vanta Core must run without a desktop UI. A headless process
  should be able to open a workspace, resolve project context, run indexing,
  execute builds, collect diagnostics, and produce change sets.
- UI state isolation: agents may maintain their own session state without
  stealing the user's editor focus, tabs, selection, or active diff. Shared UI
  state is synchronized only when the user needs to see or approve something.
- Stable plugin boundary: in-process built-ins may use C++ interfaces directly.
  Third-party plugin ABI should use C handles or out-of-process RPC rather than
  C++ STL, smart pointer, or virtual-class binary boundaries.

## Agent Capabilities

The agent should be able to ask high-level questions that are hard to answer
with raw grep:

- Which project model owns this file?
- Which build target or compile command applies to this source?
- Which headers, source roots, generated files, and excluded roots affect this
  module?
- Which diagnostics belong to the active file, target, or recent build?
- Which symbols, references, call sites, include edges, and index hits are
  relevant to the requested edit?
- Which tests or run configurations are related to the changed files?
- Which change set is pending approval, and what is its inverse operation?

These questions should be answered through typed Vanta services and providers,
not by scraping UI state or guessing from shell output.

## UI Synchronization

The UI should be a client of core state:

```text
Core event
  -> UiStateStore refresh
  -> UI renderer update
  -> user action
  -> command or typed service call
  -> Core state change
```

An agent session follows the same model:

```text
Agent request
  -> WorkspaceContext query
  -> Project / index / language / diagnostic context
  -> Agent operation
  -> ChangeSet, build, test, or explanation
  -> UI projection or approval request when needed
```

This keeps UI state and agent state separate while still letting the UI stay
live and synchronized.

## C++ Proving Ground

The C++ integration should demonstrate the platform advantage over grep/LSP
agents:

- clice or another compiler-index provider should feed semantic index data into
  Vanta's own services.
- CMake should contribute project model, build provider, build directory, target
  data, and compile database context.
- Language service adapters should remain adapters. Vanta's `LanguageRegistry`
  and `CodeIntelligenceService` are the platform API.
- Build and test loops should report structured diagnostics into
  `DiagnosticService` and `JobService`.
- Agent fixes should be generated as `ChangeSet` values, previewed through diff
  UI, and applied only after approval.

If this loop works well for C++, the same platform shape can support Android,
Java, Python, embedded projects, remote targets, and other project families.

