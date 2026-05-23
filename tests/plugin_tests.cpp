#include "test_support.h"

#include "mornox/plugin/native_plugin_abi.h"

namespace mornox::tests {

void TestPluginComponentRegistrationLifecycle() {
    const auto root = MakeTempRoot();
    WriteFile(root / "plugins" / "sample" / "mornox.plugin.json", R"({
      "id": "sample.component.plugin",
      "name": "Sample Component",
      "version": "0.1.0",
      "publisher": "Mornox",
      "runtime": {"kind": "core", "entry": "builtin:sample-component"}
    })");

    mornox::VirtualFileSystem vfs;
    mornox::WorkspaceRuntime session(vfs, mornox::InlineJobDispatcher());
    std::string error;
    REQUIRE(session.Open(root, &error));

    ComponentTestStats stats;
    mornox::PluginManager manager;
    mornox::CorePluginRegistry registry;
    registry.Add("builtin:sample-component", [&stats] {
        return std::make_unique<RuntimeComponentExtension>(stats);
    });

    mornox::ConsoleLogger logger;

    manager.Scan(root / "plugins");
    auto lifecycle = manager.PluginLifecycle();
    REQUIRE(lifecycle.size() == 1);
    REQUIRE(lifecycle[0].state == mornox::PluginLifecycleState::Discovered);
    const auto messages = manager.ActivateCorePlugins(
        registry,
        logger,
        session.Context());

    REQUIRE(messages.size() == 1);
    lifecycle = manager.PluginLifecycle();
    REQUIRE(lifecycle.size() == 1);
    REQUIRE(lifecycle[0].state == mornox::PluginLifecycleState::Active);
    REQUIRE(lifecycle[0].registration_count == 2);
    REQUIRE(session.Context().RequireProject().GetComponent("sample.runtime") != nullptr);
    REQUIRE(session.Context().RequireProject().GetComponent("sample.cpp.only") == nullptr);
    REQUIRE(stats.attached == 1);
    session.RefreshProject();
    REQUIRE(stats.opened == 1);
    REQUIRE(session.Context().RequireProject().GetComponent("sample.cpp.only") == nullptr);
    const int events_before_publish = stats.events;
    session.Context().Publish({.kind = mornox::IdeEventKind::ProjectChanged, .file = session.Context().CurrentWorkspace().RootFile()});
    REQUIRE(stats.events == events_before_publish + 1);

    manager.DeactivateAll();
    lifecycle = manager.PluginLifecycle();
    REQUIRE(lifecycle.size() == 1);
    REQUIRE(lifecycle[0].state == mornox::PluginLifecycleState::Inactive);
    REQUIRE(session.Context().RequireProject().GetComponent("sample.runtime") == nullptr);
    REQUIRE(stats.closed == 1);
    REQUIRE(stats.detached == 1);
    const int events_after_deactivate = stats.events;
    session.Context().Publish({.kind = mornox::IdeEventKind::ProjectChanged, .file = session.Context().CurrentWorkspace().RootFile()});
    REQUIRE(stats.events == events_after_deactivate);
    session.Close();
}

void TestPluginComponentProviderBeforeProjectAttach() {
    const auto root = MakeTempRoot();
    WriteFile(root / "plugins" / "sample" / "mornox.plugin.json", R"({
      "id": "sample.component.plugin",
      "name": "Sample Component",
      "version": "0.1.0",
      "publisher": "Mornox",
      "runtime": {"kind": "core", "entry": "builtin:sample-component"}
    })");

    mornox::VirtualFileSystem vfs;
    mornox::WorkspaceRuntime session(vfs, mornox::InlineJobDispatcher());
    std::string error;
    REQUIRE(session.Open(root, &error, false));

    ComponentTestStats stats;
    mornox::PluginManager manager;
    mornox::CorePluginRegistry registry;
    registry.Add("builtin:sample-component", [&stats] {
        return std::make_unique<RuntimeComponentExtension>(stats);
    });

    mornox::ConsoleLogger logger;
    manager.Scan(root / "plugins");
    const auto messages = manager.ActivateCorePlugins(registry, logger, session.Context());
    REQUIRE(messages.size() == 1);
    REQUIRE(session.Context().RequireProject().GetComponent("sample.runtime") == nullptr);
    REQUIRE(stats.attached == 0);

    session.InitializeWorkspace();
    REQUIRE(session.Context().RequireProject().GetComponent("sample.runtime") != nullptr);
    REQUIRE(stats.attached == 1);
    manager.DeactivateAll();
    session.Close();
}

void TestPluginManifest() {
    const auto root = MakeTempRoot();
    const auto plugin_dir = root / "plugins" / "sample";
    WriteFile(plugin_dir / "mornox.plugin.json", R"({
      "id": "sample.plugin",
      "name": "Sample",
      "version": "0.1.0",
      "publisher": "Mornox",
      "runtime": {"kind": "core", "entry": "builtin:sample"},
      "activationEvents": ["onWorkspaceOpened"],
      "contributes": {
        "commands": [{"id": "sample.run", "title": "Sample: Run"}],
        "languageServices": [{"id": "sample.cpp", "title": "Sample C++"}],
        "agentTools": [{"id": "sample.tool", "title": "Sample Tool"}],
        "buildProviders": [{"id": "sample.build", "title": "Sample Build"}],
        "modelProviders": [{"id": "sample.model", "title": "Sample Model"}],
        "debugProviders": [{"id": "sample.debug", "title": "Sample Debug"}]
      }
    })");

    mornox::PluginManager manager;
    const auto manifests = manager.Scan(root / "plugins");
    REQUIRE(manifests.size() == 1);
    REQUIRE(manifests[0].extension.id == "sample.plugin");
    REQUIRE(manifests[0].activation_events.size() == 1);
}

void TestPluginCompatibilityChecks() {
    const auto root = MakeTempRoot();
    WriteFile(root / "plugins" / "future" / "mornox.plugin.json", R"({
      "id": "sample.future",
      "name": "Future Plugin",
      "version": "0.1.0",
      "publisher": "Mornox",
      "runtime": {"kind": "core", "entry": "builtin:sample"},
      "minApiVersion": "999.0.0",
      "capabilities": ["command"],
      "activationEvents": ["onStartup"]
    })");
    WriteFile(root / "plugins" / "unknown" / "mornox.plugin.json", R"({
      "id": "sample.unknown",
      "name": "Unknown Capability Plugin",
      "version": "0.1.0",
      "publisher": "Mornox",
      "runtime": {"kind": "core", "entry": "builtin:sample"},
      "capabilities": ["notARealCapability"],
      "activationEvents": ["onStartup"]
    })");

    mornox::VirtualFileSystem vfs;
    mornox::WorkspaceRuntime session(vfs, mornox::InlineJobDispatcher());
    std::string error;
    REQUIRE(session.Open(root, &error));
    mornox::ConsoleLogger logger;
    mornox::PluginManager manager;
    mornox::CorePluginRegistry registry;
    manager.Scan(root / "plugins");
    const auto messages = manager.ActivateCorePlugins(registry, logger, session.Context());
    REQUIRE(messages.size() == 2);
    REQUIRE(manager.ActivePluginIds().empty());
    const auto lifecycle = manager.PluginLifecycle();
    REQUIRE(lifecycle.size() == 2);
    REQUIRE(std::all_of(lifecycle.begin(), lifecycle.end(), [](const mornox::PluginLifecycleInfo& info) {
        return info.state == mornox::PluginLifecycleState::Failed && !info.error.empty();
    }));
    session.Close();
}

void TestCorePluginActivation() {
    const auto root = MakeTempRoot();
    WriteFile(root / "CMakeLists.txt", "cmake_minimum_required(VERSION 3.20)\nproject(Sample)\n");
    WriteFile(root / "plugins" / "cmake" / "mornox.plugin.json", R"({
      "id": "mornox.cmake",
      "name": "CMake Support",
      "version": "0.1.0",
      "publisher": "Mornox",
      "runtime": {"kind": "core", "entry": "builtin:cmake"}
    })");
    WriteFile(root / "plugins" / "cmake" / "i18n" / "en-US.properties", R"(
command.detect.title=Detect CMake
command.detect.progress=Detecting {}
)");
    WriteFile(root / "plugins" / "cmake" / "i18n" / "zh-CN.properties", R"(
command.detect.title=检测 CMake
command.detect.progress=正在检测 {}
)");

    mornox::VirtualFileSystem vfs;
    mornox::WorkspaceRuntime session(vfs, mornox::InlineJobDispatcher());
    std::string error;
    REQUIRE(session.Open(root, &error));
    mornox::ConsoleLogger logger;
    mornox::PluginManager manager;
    mornox::CorePluginRegistry registry = mornox::CreateDefaultCorePluginRegistry();

    manager.Scan(root / "plugins");
    const auto messages = manager.ActivateCorePlugins(
        registry,
        logger,
        session.Context());

    REQUIRE(messages.size() == 1);
    REQUIRE(manager.ActivePluginIds().size() == 1);
    REQUIRE(session.Context().LocalizerFor("mornox.cmake").Resolve("command.detect.title", {}, "zh-CN") == "检测 CMake");
    REQUIRE(session.Context().LocalizerFor("mornox.cmake").Resolve("command.detect.progress", {mornox::Value("Sample")}, "zh-CN") == "正在检测 Sample");
    REQUIRE(session.Context().Commands().Execute("cmake.detect", mornox::Value::ObjectValue()).has_value());
    REQUIRE(!session.Context().Build().BuildProviderIds().empty());
    REQUIRE(!session.Context().Projects().ModelProviderIds().empty());
    session.RefreshProject();
    REQUIRE(session.Context().RequireProject().Model().HasFacet("cmake"));
    const auto reload_messages = manager.ReloadCorePlugins(
        registry,
        logger,
        session.Context());
    REQUIRE(reload_messages.size() == 1);
    REQUIRE(manager.ActivePluginIds().size() == 1);
    REQUIRE(session.Context().Commands().Execute("cmake.detect", mornox::Value::ObjectValue()).has_value());
    std::string unload_message;
    REQUIRE(!manager.UnloadPlugin("mornox.cmake", &unload_message));
    REQUIRE(manager.ActivePluginIds().size() == 1);
    manager.DeactivateAll();
    REQUIRE(manager.ActivePluginIds().empty());
    REQUIRE(session.Context().LocalizerFor("mornox.cmake").Resolve("command.detect.title", {}, "zh-CN") == "command.detect.title");
    REQUIRE(!session.Context().Commands().Execute("cmake.detect", mornox::Value::ObjectValue()).has_value());
    REQUIRE(session.Context().Build().BuildProviderIds().empty());
    REQUIRE(session.Context().Projects().ModelProviderIds().empty());
    session.Close();
}

void TestExternalPluginUnloadAndReload() {
    const auto root = MakeTempRoot();
    const auto plugin_dir = root / "plugins" / "external";
    WriteFile(plugin_dir / "mornox.plugin.json", R"({
      "id": "sample.external",
      "name": "Sample External",
      "version": "0.1.0",
      "publisher": "Mornox",
      "runtime": {"kind": "process", "entry": "host.py"},
      "capabilities": ["command", "modelProvider", "debugProvider"]
    })");
    WriteFile(plugin_dir / "host.py", R"PY(#!/usr/bin/env python3
import json
import sys

def read_request():
    headers = {}
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        if line in (b"\r\n", b"\n"):
            break
        key, value = line.decode("utf-8").split(":", 1)
        headers[key.lower()] = value.strip()
    length = int(headers.get("content-length", "0"))
    if length == 0:
        return None
    return json.loads(sys.stdin.buffer.read(length).decode("utf-8"))

def send_response(identifier, result):
    body = json.dumps({"jsonrpc": "2.0", "id": identifier, "result": result})
    sys.stdout.write(f"Content-Length: {len(body)}\r\n\r\n{body}")
    sys.stdout.flush()

while True:
    request = read_request()
    if request is None:
        break
    method = request.get("method")
    params = request.get("params", {})
    if method == "plugin.activate":
        send_response(request["id"], {
            "registrations": [
                {"kind": "command", "id": "external.echo", "title": "External Echo"},
                {"kind": "modelProvider", "id": "external.model", "title": "External Model", "metadata": {
                    "models": [{"id": "external-model", "displayName": "External Model", "capabilities": ["chat"]}]
                }},
                {"kind": "debugProvider", "id": "external.debug", "title": "External Debug", "metadata": {
                    "configurationTypes": ["native"]
                }}
            ]
        })
    elif method == "command.execute":
        send_response(request["id"], {"echo": params.get("arguments", {})})
    elif method == "model.models":
        send_response(request["id"], {"models": [
            {"id": "external-model", "providerId": "external.model", "displayName": "External Model", "capabilities": ["chat"]}
        ]})
    elif method == "model.complete":
        send_response(request["id"], {"ok": True, "modelId": params.get("request", {}).get("modelId", ""), "providerId": "external.model", "content": "External response"})
    elif method == "debug.start":
        send_response(request["id"], {"id": params.get("sessionId"), "providerId": "external.debug", "status": "running", "message": "Started"})
    elif method == "debug.stop":
        send_response(request["id"], {"ok": True})
    elif method == "debug.evaluate":
        send_response(request["id"], {"ok": True, "value": params.get("expression", ""), "type": "string"})
    elif method == "debug.stackTrace":
        send_response(request["id"], {"frames": [{"id": 1, "name": "main"}]})
    elif method == "debug.variables":
        send_response(request["id"], {"variables": [{"name": "value", "value": "1", "type": "int"}]})
    elif method == "plugin.deactivate":
        send_response(request["id"], {"ok": True})
        break
    else:
        send_response(request["id"], {"ok": True})
)PY");
    std::filesystem::permissions(plugin_dir / "host.py", std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);

    const auto inactive_dir = root / "plugins" / "inactive";
    WriteFile(inactive_dir / "mornox.plugin.json", R"({
      "id": "sample.inactive",
      "name": "Inactive External",
      "version": "0.1.0",
      "publisher": "Mornox",
      "runtime": {"kind": "process", "entry": "host.py"},
      "activationEvents": ["onWorkspaceContains:missing.activation.file"]
    })");
    WriteFile(inactive_dir / "host.py", "#!/usr/bin/env python3\n");
    std::filesystem::permissions(inactive_dir / "host.py", std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);

    mornox::VirtualFileSystem vfs;
    mornox::WorkspaceRuntime session(vfs, mornox::InlineJobDispatcher());
    std::string error;
    REQUIRE(session.Open(root, &error));
    mornox::ConsoleLogger logger;
    mornox::PluginManager manager;
    manager.Scan(root / "plugins");

    const auto activate_messages = manager.ActivateExternalPlugins(logger, session.Context());
    REQUIRE(activate_messages.size() == 1);
    REQUIRE(manager.ActivePluginIds().size() == 1);
    auto lifecycle = manager.PluginLifecycle();
    REQUIRE(lifecycle.size() == 2);
    const auto active = std::find_if(lifecycle.begin(), lifecycle.end(), [](const mornox::PluginLifecycleInfo& info) {
        return info.id == "sample.external";
    });
    REQUIRE(active != lifecycle.end());
    REQUIRE(active->state == mornox::PluginLifecycleState::Active);
    REQUIRE(active->unloadable);
    REQUIRE(active->registration_count == 3);
    REQUIRE(active->process_running);
    const auto command_result = session.Context().Commands().Execute("external.echo", mornox::Value::ObjectValue({{"value", mornox::Value("ok")}}));
    REQUIRE(command_result.has_value());
    REQUIRE((*command_result)["echo"].StringValue("value").value_or("") == "ok");
    REQUIRE(session.Context().Models().Model("external-model").has_value());
    const auto model_response = session.Context().Models().Complete({
        .model_id = "external-model",
        .messages = {{
            .role = mornox::ModelMessageRole::User,
            .content = "hello",
        }},
    });
    REQUIRE(model_response.ok);
    REQUIRE(model_response.content == "External response");
    const mornox::DebugSession external_debug = session.Context().Debug().Start(session.Context(), {
        .id = "debug.external",
        .name = "External Debug",
        .type = "native",
    });
    REQUIRE(external_debug.status == mornox::DebugSessionStatus::Running);
    REQUIRE(session.Context().Debug().Evaluate(external_debug.id, "value").ok);
    std::string unload_message;
    REQUIRE(manager.UnloadPlugin("sample.external", &unload_message));
    REQUIRE(manager.ActivePluginIds().empty());
    REQUIRE(!session.Context().Commands().Execute("external.echo", mornox::Value::ObjectValue()).has_value());
    REQUIRE(!session.Context().Models().Model("external-model").has_value());
    REQUIRE(session.Context().Debug().ProviderIds().empty());
    lifecycle = manager.PluginLifecycle();
    REQUIRE(lifecycle.size() == 2);
    const auto inactive = std::find_if(lifecycle.begin(), lifecycle.end(), [](const mornox::PluginLifecycleInfo& info) {
        return info.id == "sample.external";
    });
    REQUIRE(inactive != lifecycle.end());
    REQUIRE(inactive->state == mornox::PluginLifecycleState::Inactive);
    const auto reload_messages = manager.ReloadPlugin("sample.external", logger, session.Context());
    REQUIRE(!reload_messages.empty());
    REQUIRE(manager.ActivePluginIds().size() == 1);
    REQUIRE(manager.UnloadPlugin("sample.external", &unload_message));
    session.Close();
}

void TestExternalPluginProcessHealth() {
    const auto root = MakeTempRoot();
    const auto plugin_dir = root / "plugins" / "crash";
    WriteFile(plugin_dir / "mornox.plugin.json", R"({
      "id": "sample.crash",
      "name": "Crashing External",
      "version": "0.1.0",
      "publisher": "Mornox",
      "runtime": {"kind": "process", "entry": "host.py"},
      "capabilities": ["command"]
    })");
    WriteFile(plugin_dir / "host.py", R"PY(#!/usr/bin/env python3
import json
import sys

def read_request():
    headers = {}
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        if line in (b"\r\n", b"\n"):
            break
        key, value = line.decode("utf-8").split(":", 1)
        headers[key.lower()] = value.strip()
    length = int(headers.get("content-length", "0"))
    if length == 0:
        return None
    return json.loads(sys.stdin.buffer.read(length).decode("utf-8"))

request = read_request()
if request is not None:
    body = json.dumps({"jsonrpc": "2.0", "id": request["id"], "result": {
        "registrations": [{"kind": "command", "id": "crash.command", "title": "Crash Command"}]
    }})
    sys.stdout.write(f"Content-Length: {len(body)}\r\n\r\n{body}")
    sys.stdout.flush()
)PY");
    std::filesystem::permissions(plugin_dir / "host.py", std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);

    mornox::VirtualFileSystem vfs;
    mornox::WorkspaceRuntime session(vfs, mornox::InlineJobDispatcher());
    std::string error;
    REQUIRE(session.Open(root, &error));
    mornox::ConsoleLogger logger;
    mornox::PluginManager manager;
    manager.Scan(root / "plugins");

    const auto activate_messages = manager.ActivateExternalPlugins(logger, session.Context());
    REQUIRE(activate_messages.size() == 1);
    REQUIRE(manager.ActivePluginIds().size() == 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto lifecycle = manager.PluginLifecycle();
    REQUIRE(manager.ActivePluginIds().empty());
    REQUIRE(!session.Context().Commands().Execute("crash.command", mornox::Value::ObjectValue()).has_value());
    const auto failed = std::find_if(lifecycle.begin(), lifecycle.end(), [](const mornox::PluginLifecycleInfo& info) {
        return info.id == "sample.crash";
    });
    REQUIRE(failed != lifecycle.end());
    REQUIRE(failed->state == mornox::PluginLifecycleState::Failed);
    REQUIRE(!failed->process_running);
    REQUIRE(failed->crash_count >= 1);
    session.Close();
}

void TestPluginProtocol() {
    mornox::PluginRpcRequest request;
    request.id = 7;
    request.method = "Activate";
    request.params_json = mornox::ValueToJsonText(mornox::Value::ObjectValue({{"plugin", mornox::Value("sample")}}));
    const mornox::Result<mornox::Value> request_json = mornox::ValueFromJsonText(mornox::FormatPluginRpcRequestText(request));
    REQUIRE(request_json);
    const mornox::Value& json = request_json.Value();
    REQUIRE(json["id"].AsInt() == 7);
    REQUIRE(json["method"].AsString() == "Activate");

    const auto response = mornox::ParsePluginRpcResponse(mornox::Value::ObjectValue({
        {"jsonrpc", mornox::Value("2.0")},
        {"id", mornox::Value(static_cast<std::int64_t>(7))},
        {"result", mornox::Value::ObjectValue({{"ok", mornox::Value(true)}})},
    }));
    REQUIRE(response.has_value());
    REQUIRE(response->ok);
    const mornox::Result<mornox::Value> result = mornox::ValueFromJsonText(response->result_json);
    REQUIRE(result);
    REQUIRE(result.Value()["ok"].AsBool());

    const auto parsed = mornox::ParsePluginCapabilityRegistration(mornox::Value::ObjectValue({
        {"kind", mornox::Value("agentTool")},
        {"id", mornox::Value("sample.tool")},
        {"title", mornox::Value("Sample Tool")},
        {"metadata", mornox::Value::ObjectValue()},
    }));
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->kind == mornox::PluginCapabilityKind::AgentTool);

    const auto model_registration = mornox::ParsePluginCapabilityRegistration(mornox::Value::ObjectValue({
        {"kind", mornox::Value("modelProvider")},
        {"id", mornox::Value("sample.model")},
    }));
    REQUIRE(model_registration.has_value());
    REQUIRE(model_registration->kind == mornox::PluginCapabilityKind::ModelProvider);
    REQUIRE(mornox::ToString(model_registration->kind) == "modelProvider");

    const auto debug_registration = mornox::ParsePluginCapabilityRegistration(mornox::Value::ObjectValue({
        {"kind", mornox::Value("debugProvider")},
        {"id", mornox::Value("sample.debug")},
    }));
    REQUIRE(debug_registration.has_value());
    REQUIRE(debug_registration->kind == mornox::PluginCapabilityKind::DebugProvider);
    REQUIRE(mornox::ToString(debug_registration->kind) == "debugProvider");
}

void TestRuntimeApproval() {
    mornox::WorkspaceTrustService trust;
    mornox::ApprovalService approvals(trust);
    approvals.SetAutoApprove(false);
    const auto decision = approvals.RequestApproval({
        .actor = {.kind = mornox::ApprovalActorKind::Plugin, .id = "sample.plugin"},
        .access = mornox::AccessKind::WorkspaceWrite,
        .action = "write file",
        .high_risk = true,
    });
    REQUIRE(decision == mornox::ApprovalDecision::Deny);
    REQUIRE(approvals.History().size() == 1);

    approvals.SetAutoApprove(true);
    trust.SetLevel(mornox::WorkspaceTrustLevel::Untrusted);
    const auto blocked = approvals.RequestApproval({
        .actor = {.kind = mornox::ApprovalActorKind::Plugin, .id = "sample.plugin"},
        .access = mornox::AccessKind::ProcessExecute,
        .action = "run command",
        .high_risk = true,
    });
    REQUIRE(blocked == mornox::ApprovalDecision::Deny);
    REQUIRE(!trust.Allows(mornox::AccessKind::ProcessExecute, true));
    trust.SetLevel(mornox::WorkspaceTrustLevel::Trusted);
    const auto allowed = approvals.RequestApproval({
        .actor = {.kind = mornox::ApprovalActorKind::Plugin, .id = "sample.plugin"},
        .access = mornox::AccessKind::ProcessExecute,
        .action = "run command",
        .high_risk = true,
    });
    REQUIRE(allowed == mornox::ApprovalDecision::Allow);
}

void TestNativePluginAbiShape() {
    MornoxHostApi api{};
    api.abi_version = MORNOX_PLUGIN_ABI_VERSION;
    api.struct_size = sizeof(MornoxHostApi);

    MornoxPluginCreateInfo create_info{};
    create_info.abi_version = MORNOX_PLUGIN_ABI_VERSION;
    create_info.struct_size = sizeof(MornoxPluginCreateInfo);

    REQUIRE(api.abi_version == 1u);
    REQUIRE(api.struct_size >= sizeof(std::uint32_t) * 2);
    REQUIRE(create_info.abi_version == api.abi_version);
}

}

TEST_CASE("Plugin component registration lifecycle", "[plugin][project]") {
    mornox::tests::TestPluginComponentRegistrationLifecycle();
}

TEST_CASE("Plugin component provider before project attach", "[plugin][project]") {
    mornox::tests::TestPluginComponentProviderBeforeProjectAttach();
}

TEST_CASE("Plugin manifest", "[plugin]") {
    mornox::tests::TestPluginManifest();
}

TEST_CASE("Plugin compatibility checks", "[plugin]") {
    mornox::tests::TestPluginCompatibilityChecks();
}

TEST_CASE("Core plugin activation", "[plugin]") {
    mornox::tests::TestCorePluginActivation();
}

TEST_CASE("External plugin unload and reload", "[plugin]") {
    mornox::tests::TestExternalPluginUnloadAndReload();
}

TEST_CASE("External plugin process Health", "[plugin]") {
    mornox::tests::TestExternalPluginProcessHealth();
}

TEST_CASE("Plugin protocol", "[plugin]") {
    mornox::tests::TestPluginProtocol();
}

TEST_CASE("Runtime approval", "[plugin][security]") {
    mornox::tests::TestRuntimeApproval();
}

TEST_CASE("Native plugin ABI shape", "[plugin]") {
    mornox::tests::TestNativePluginAbiShape();
}
