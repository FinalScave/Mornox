# Vanta Service Boundaries

Vanta services are grouped by the API surface they belong to. New APIs should
declare their intended layer before being exposed to plugins or UI clients.

The guiding rule is that `WorkspaceContext` is the public IDE capability
surface, while `WorkspaceRuntime` is a lifecycle owner for concrete service
instances. Plugins, components, providers, agents, and UI projections should
receive `WorkspaceContext&` and depend on service interfaces, not concrete
runtime implementations.

## Workspace Core

Core workspace services are available from `WorkspaceContext` and are not tied
to a specific language, build system, or UI.

- `Workspace` owns the opened root, root file, and basic file access helpers.
- `VirtualFileSystem` owns URI-backed file access and file-system providers.
- `IdeEventBus` owns typed workspace events.
- `DocumentService` owns open document overlays and save state.
- `CommandRegistry` owns command registration and command invocation.
- `GitService` owns source-control queries for the workspace.
- `JobService` owns job records, progress, output, cancellation, dependencies,
  waiting, and change events. It does not depend on build or execution events.
- `IndexService` owns index providers, refresh jobs, snapshots, index change
  events, and query routing for file, text, symbol, reference, include, and
  custom index lookups.
- `CapabilityRegistry` owns readiness and capability state derived from runtime
  services.
- `WorkspaceInitializationPipeline` owns startup and project refresh stages.
- `SettingsService` owns typed setting definitions, scoped values, search, and
  effective value resolution.
- `ChangeSetService` owns proposed workspace edits, approval status, preflight
  validation, apply, inverse edits, and undo tokens.
- `WorkspaceTrustService` owns workspace trust policy.
- `ApprovalService` owns user approval flow for sensitive operations and consults
  workspace trust before remembering approvals.

## Project Model

Project services describe the current workspace as a structured project rather
than a raw file tree.

- `ProjectManager` owns project model providers, current project-model refresh,
  project view providers, and project structure views such as Files, CMake,
  Android, packages, scratch files, and generated files.
- `ProjectModelProvider` contributes project facets, modules, roots,
  attachments, and summaries.
- `ProjectViewProvider` contributes one or more project views and answers
  `TopLevelNodes` and `Children` queries. Providers own complete views rather
  than merging nodes into another provider's tree.
- `Project` owns the current `ProjectModel` and project components.
- `Component` instances follow the project lifecycle and may persist state in
  `ProjectState`.
- `ProjectRunConfigurations` is a project component that owns persisted run
  configurations for the current project.
- `ProjectTemplateService` owns project template categories and template
  materialization.
- `ScratchFileService` creates lightweight scratch files inside the workspace
  runtime.

## Language And Code Intelligence

Language APIs are platform-level contracts. Language servers and compiler
integrations are implementations or providers.

- `LanguageRegistry` resolves language definitions and services for files with
  optional project context.
- `LanguageService` is Vanta's language-service contract for completion, hover,
  definition, semantic tokens, and document lifecycle events.
- `CodeIntelligenceService` owns completion, inline completion, and routed
  language queries. It internally uses the language request pipeline.
- `LspLanguageService` adapts an LSP client to Vanta's `LanguageService` API.
- clice or other compiler-index integrations should feed Vanta services rather
  than becoming the public API themselves.

## Build, Execution, Run, And Jobs

Build, execution, and jobs are related but separate.

- `JobService` records task state and owns cancellation visibility. It is
  intentionally unaware of `ExecutionEvent`.
- `ExecutionService` owns execution providers, execution targets, process
  start, remote or device execution, live output, and execution events.
- `BuildService` owns build provider detection and converts build or test intent
  into a `BuildPlan`. It delegates process execution to `ExecutionService` and
  reports progress through `JobService`.
- `RunConfigurationRegistry` owns run configuration types and producers.
- `ProjectRunConfigurations` owns persisted run configurations as project state.

This separation lets a remote executor, local process runner, ADB device, or
container provider use the same job and build surfaces.

## Agent Platform

Agent APIs should remain auditable and should not mutate the workspace without a
`ChangeSet`.

- `AgentContextCollector` gathers structured context for model prompts.
- `AgentToolRegistry` exposes dynamic tools for model tool calls.
- `AgentOperationService` owns the structured operation protocol for auditable
  IDE mutations.
- `AgentOperationJournal` records operation input summaries, events, results,
  and change sets.
- `ModelService` owns model provider registration, model discovery, completion,
  and streaming callbacks.
- `AgentRuntime` owns interactive agent sessions and bridges context collection,
  model calls, plan events, and agent operation calls.
- Agent writes should create `ChangeSet` values and rely on approval before
  apply.
- Long-lived IDE operations should prefer `AgentOperationService` over raw tool
  calls.

## Plugin API

`ExtensionContext` is the plugin lifecycle context. It should not mirror every
IDE service.

- `ExtensionContext` exposes extension info, workspace info, logger,
  permissions, lifecycle tracking, component contribution, and
  `workspaceContext()`.
- Plugins access IDE capabilities through `WorkspaceContext`.
- Runtime contribution metadata is exposed as DTO snapshots, not as a mutable
  registry service.
- Plugin APIs should depend on service interfaces such as `BuildService`,
  `LanguageRegistry`, `CommandRegistry`, `RunConfigurationRegistry`, and
  `GitService`.
- Default implementations live in `vanta::internal` and are owned by
  `WorkspaceRuntime`.
- In-process built-in plugins may use C++ interfaces directly.
- Third-party plugin ABI should use C handles or out-of-process RPC. Stable
  plugin ABI must not expose STL containers, smart pointers, or virtual-class
  binary boundaries.
- Plugins may contribute setting nodes and setting definitions, but plugin
  identity is an owner, not a settings scope.
- Plugin activation is driven by activation events such as startup, workspace
  file presence, language availability, and command availability.
- Plugin manifests declare `minApiVersion`, optional `targetApiVersion`, and
  capabilities.
- Process plugins return `PluginRegistration` values during activation. Vanta
  stores those registrations as runtime contribution metadata and bridges
  supported registrations into commands, agent tools, build providers, model
  providers, debug providers, and language services.
- Hot unload must explicitly release registrations tracked by plugin activation
  state.

## Localization

Localization is a core registry with UI-owned locale selection.

- `LocalizedText` stores `ownerId`, `key`, and ordered `Value` arguments.
- `LocalizationCatalog` stores messages for one owner and one locale.
- `LocalizationRegistry` resolves text by current locale, falls back to the
  default locale, and returns the key when no message is found.
- Message patterns use simple ordered `{}` placeholders. `{{` and `}}` escape
  braces.
- Built-in catalogs should live under `resources/i18n/<owner-id>/<locale>.properties`.
- Plugin catalogs live under `<plugin-root>/i18n/<locale>.properties`.
  `PluginManager` loads them during activation and uses the plugin id as
  `ownerId`.
- Default locale resources are required for product-owned text. The initial
  default locale is `en-US`.

## Built-In Providers

Built-ins can provide default implementations, but core services should not
depend on one built-in implementation directly.

- Built-in plugin packages live under `plugins/builtin/vanta.*` with their
  manifests, resources, and implementation sources.
- `src/plugin` contains plugin infrastructure only, such as lifecycle,
  manifests, permissions, contribution tracking, and process hosting.
- CMake is a build and project-model provider that produces `BuildPlan` steps.
- C++ and Python single-file run support and project templates are built-in
  plugin contributions, not core platform behavior.
- clice is a C++ language and index integration.
- Git is exposed through `GitService`; the command-backed implementation is
  platform-owned, while the built-in Git extension contributes commands and
  agent tools.
- LSP is a language-service adapter, not the platform language API.

## UI-Facing Facades

UI code should prefer stable query APIs and event streams rather than reaching
into implementation details.

UI session state is owned by UI clients, not by `WorkspaceRuntime`.
`UiStateStore` instances belong to the IDE/UI layer, subscribe to core events,
and project workspace state together with client-owned tabs, keybindings, and
palette state. The CLI uses `WorkspaceRuntime` directly and does not depend on
UI state. Different frontends can keep their own view state, such as panel sizes
or scroll positions, without pushing pixel-level details into core.

- Background UI should read `JobService` and `WorkspaceInitializationPipeline`.
- Readiness UI should read `CapabilityRegistry`.
- Settings UI should read `SettingsService` nodes, definitions, scope
  descriptors, and search results without depending on a fixed UI layout.
- Index UI and search UI should read `IndexService` snapshots and query results.
- Project/explorer UI should read `ProjectManager::Views`, `TopLevelNodes`, and
  `Children`; UI-owned expansion, selection, scroll, and panel dimensions stay
  outside core.
- Agent timeline UI should read `AgentOperationJournal`.
- Agent session UI should read `AgentRuntime`.
- Diff and approval UI should read `ChangeSetService`.
- Debug UI should read `DebugService`.
