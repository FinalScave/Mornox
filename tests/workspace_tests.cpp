#include "test_support.h"

namespace vanta::tests {

void TestWorkspace() {
    const auto root = MakeTempRoot();
    WriteFile(root / "src" / "main.cpp", "int main() { return 0; }\n");

    vanta::Workspace workspace;
    vanta::VirtualFileSystem vfs;
    workspace.BindFileSystem(vfs);
    std::string error;
    REQUIRE(workspace.Open(root, &error));
    REQUIRE(workspace.Info().name == "vanta-tests");
    REQUIRE(workspace.ReadTextFile("src/main.cpp").has_value());
    const auto root_children = workspace.RootFile().ListChildren();
    const auto src = std::find_if(root_children.begin(), root_children.end(), [](const vanta::VirtualFile& file) {
        return file.DisplayName() == "src";
    });
    REQUIRE(src != root_children.end());
    const auto source_children = src->ListChildren();
    REQUIRE(std::any_of(source_children.begin(), source_children.end(), [](const vanta::VirtualFile& file) {
        return file.DisplayName() == "main.cpp";
    }));
}

void TestWorkspaceRuntimeEvents() {
    const auto root = MakeTempRoot();
    WriteFile(root / "CMakeLists.txt", "cmake_minimum_required(VERSION 3.20)\nproject(Sample)\n");
    WriteFile(root / "main.cpp", "int main() { return 0; }\n");
    WriteFile(root / "plugins" / "cmake" / "vanta.plugin.json", R"({
      "id": "vanta.cmake",
      "name": "CMake Support",
      "version": "0.1.0",
      "publisher": "Vanta",
      "runtime": {"kind": "core", "entry": "builtin:cmake"},
      "permissions": ["process.execute", "build.provider", "agent.tool"]
    })");

    vanta::VirtualFileSystem vfs;
    vanta::AsyncRuntime async_runtime(1);
    vanta::WorkspaceRuntime session(vfs, async_runtime);

    std::vector<vanta::IdeEventKind> events;
    session.Context().Events().Subscribe([&](const vanta::IdeEvent& event) {
        events.push_back(event.kind);
    });

    std::string error;
    REQUIRE(session.Open(root, &error));
    vanta::ConsoleLogger logger;
    vanta::PluginManager manager;
    vanta::CorePluginRegistry registry = vanta::CreateDefaultCorePluginRegistry();
    manager.Scan(root / "plugins");
    manager.ActivateCorePlugins(registry, logger, session.Context());
    session.RefreshProject();
    vanta::UiStateStore ui(session.Context());

    const vanta::VirtualFile main_file = session.Context().CurrentWorkspace().File("main.cpp");
    REQUIRE(session.Context().Documents().OpenDocument(main_file, &error) != nullptr);

    vanta::Diagnostic diagnostic;
    diagnostic.location.file = main_file;
    diagnostic.source = "test";
    diagnostic.message = "sample";
    session.Context().Diagnostics().Publish("test", {diagnostic});

    const vanta::JobId job = session.Context().Jobs().Start(vanta::JobKind::Build, "build");
    session.Context().Jobs().Complete(job, true);
    ui.Refresh();
    REQUIRE(ui.State().workspace_open);
    REQUIRE(ui.State().project.HasFacet("cmake"));
    REQUIRE(ui.State().problems.size() == 1);
    REQUIRE(std::any_of(ui.State().jobs.begin(), ui.State().jobs.end(), [&](const vanta::JobRecord& record) {
        return record.id == job && record.status == vanta::JobStatus::Succeeded;
    }));
    manager.DeactivateAll();
    session.Close();
    REQUIRE(!ui.State().workspace_open);

    REQUIRE(std::find(events.begin(), events.end(), vanta::IdeEventKind::WorkspaceOpened) != events.end());
    REQUIRE(std::find(events.begin(), events.end(), vanta::IdeEventKind::ProjectChanged) != events.end());
    REQUIRE(std::find(events.begin(), events.end(), vanta::IdeEventKind::DocumentOpened) != events.end());
    REQUIRE(std::find(events.begin(), events.end(), vanta::IdeEventKind::DiagnosticsChanged) != events.end());
    REQUIRE(std::find(events.begin(), events.end(), vanta::IdeEventKind::JobStarted) != events.end());
    REQUIRE(std::find(events.begin(), events.end(), vanta::IdeEventKind::JobCompleted) != events.end());
    REQUIRE(std::find(events.begin(), events.end(), vanta::IdeEventKind::WorkspaceClosed) != events.end());
}

void TestWorkspacePlatformServices() {
    const auto root = MakeTempRoot();
    WriteFile(root / "main.cpp", "int main() { return 0; }\n");
    WriteFile(root / "plugins" / "languages" / "vanta.plugin.json", R"({
      "id": "vanta.languages",
      "name": "Core Languages",
      "version": "0.1.0",
      "publisher": "Vanta",
      "runtime": {"kind": "core", "entry": "builtin:languages"},
      "activationEvents": ["onStartup"],
      "permissions": ["language.service"]
    })");
    WriteFile(root / "plugins" / "cpp" / "vanta.plugin.json", R"({
      "id": "vanta.cpp",
      "name": "C++ Platform Support",
      "version": "0.1.0",
      "publisher": "Vanta",
      "runtime": {"kind": "core", "entry": "builtin:cpp"},
      "activationEvents": ["onLanguage:cpp"],
      "permissions": ["workspace.read", "process.execute"]
    })");
    WriteFile(root / "plugins" / "python" / "vanta.plugin.json", R"({
      "id": "vanta.python",
      "name": "Python Platform Support",
      "version": "0.1.0",
      "publisher": "Vanta",
      "runtime": {"kind": "core", "entry": "builtin:python"},
      "activationEvents": ["onLanguage:python"],
      "permissions": ["workspace.read", "process.execute"]
    })");

    vanta::VirtualFileSystem vfs;
    vanta::AsyncRuntime async_runtime(1);
    vanta::WorkspaceRuntime session(vfs, async_runtime);
    std::string error;
    REQUIRE(session.Open(root, &error));
    vanta::ConsoleLogger logger;
    vanta::PluginManager manager;
    vanta::CorePluginRegistry registry = vanta::CreateDefaultCorePluginRegistry();
    manager.Scan(root / "plugins");
    manager.ActivateCorePlugins(registry, logger, session.Context());

    REQUIRE(session.Context().Initialization().Completed(vanta::WorkspaceInitializationStage::WorkspaceOpened));
    REQUIRE(session.Context().Initialization().Completed(vanta::WorkspaceInitializationStage::FileIndexReady));
    REQUIRE(session.Context().Initialization().Completed(vanta::WorkspaceInitializationStage::AgentContextReady));
    REQUIRE(session.Context().Capabilities().Available("workspace.open"));
    REQUIRE(session.Context().Capabilities().Get("index.workspace").has_value());
    REQUIRE(!session.Context().Jobs().Jobs().empty());

    const auto snapshots = session.Context().Indexes().Snapshots();
    REQUIRE(!snapshots.empty());
    const auto search_snapshot = session.Context().Indexes().Snapshot("vanta.index.search");
    REQUIRE(search_snapshot.has_value());
    REQUIRE(search_snapshot->status == vanta::IndexStatus::Ready);
    REQUIRE(search_snapshot->item_count > 0);

    const auto categories = session.Context().ProjectTemplates().Categories();
    REQUIRE(std::any_of(categories.begin(), categories.end(), [](const vanta::ProjectTemplateCategory& category) {
        return category.id == "cpp";
    }));
    REQUIRE(std::any_of(categories.begin(), categories.end(), [](const vanta::ProjectTemplateCategory& category) {
        return category.id == "python";
    }));
    REQUIRE(std::none_of(categories.begin(), categories.end(), [](const vanta::ProjectTemplateCategory& category) {
        return category.id == "android";
    }));

    const auto created = session.Context().ProjectTemplates().CreateProject("cpp.console.cmake", root / "generated");
    REQUIRE(created.ok);
    REQUIRE(std::filesystem::exists(root / "generated" / "CMakeLists.txt"));

    const auto scratch = session.Context().ScratchFiles().CreateScratchFile(session.Context(), {
        .language_id = "python",
        .file_name = "note.py",
        .contents = "print('scratch')\n",
    });
    REQUIRE(scratch.ok);
    REQUIRE(scratch.file.Exists());
    REQUIRE(scratch.file.ReadText()->find("scratch") != std::string::npos);
    REQUIRE(!session.Context().Jobs().Jobs().empty());
    manager.DeactivateAll();
    session.Close();
}

void TestLayoutAndCommandPalette() {
    const auto root = MakeTempRoot();
    WriteFile(root / "main.cpp", "int main() { return 0; }\n");

    vanta::VirtualFileSystem vfs;
    vanta::AsyncRuntime async_runtime(1);
    vanta::WorkspaceRuntime session(vfs, async_runtime);
    std::string error;
    REQUIRE(session.Open(root, &error));
    vanta::UiStateStore ui(session.Context());

    const vanta::VirtualFile main_file = session.Context().CurrentWorkspace().File("main.cpp");
    ui.OpenFile(main_file);
    auto* layout = session.Context().RequireProject().GetComponent<vanta::LayoutStateStore>(vanta::LayoutStateStore::kComponentId);
    REQUIRE(layout != nullptr);
    layout->Capture(ui.State());
    layout->RememberBuildTarget("vanta_tests");
    REQUIRE(layout->State().open_tabs.size() == 1);
    REQUIRE(layout->State().last_build_target == "vanta_tests");

    session.Context().Commands().RegisterCommand("sample.run", [](const vanta::Value&) {
        return vanta::Value::ObjectValue();
    });
    session.Context().RegisterContribution({
        .kind = vanta::PluginRegistrationKind::Command,
        .id = "sample.run",
        .title = "Sample: Run",
        .plugin_id = "sample.plugin",
    });
    ui.Keybindings().Bind({.command_id = "sample.run", .key = "Cmd+R", .when = "workspaceOpen"});

    const auto items = vanta::CommandPaletteItems(session.Context().Commands(), session.Context().Contributions(), ui.Keybindings());
    const auto filtered = vanta::FilterCommandPaletteItems(items, "sample");
    REQUIRE(filtered.size() == 1);
    REQUIRE(filtered[0].keybinding == "Cmd+R");
    session.Close();
}

}

TEST_CASE("Workspace basics", "[workspace]") {
    vanta::tests::TestWorkspace();
}

TEST_CASE("Workspace runtime events", "[workspace]") {
    vanta::tests::TestWorkspaceRuntimeEvents();
}

TEST_CASE("Workspace platform services", "[workspace]") {
    vanta::tests::TestWorkspacePlatformServices();
}

TEST_CASE("Layout and command palette", "[workspace]") {
    vanta::tests::TestLayoutAndCommandPalette();
}
