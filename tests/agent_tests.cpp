#include "test_support.h"

namespace mornox::tests {

void TestAgentRuntimeAndModelService() {
    const auto root = MakeTempRoot();
    WriteFile(root / "main.cpp", "int main() { return 0; }\n");

    mornox::VirtualFileSystem vfs;
    mornox::WorkspaceRuntime session(vfs, mornox::InlineJobDispatcher());
    std::string error;
    REQUIRE(session.Open(root, &error));
    session.Context().Models().RegisterProvider(std::make_unique<FakeModelProvider>());

    int events = 0;
    mornox::AgentSessionRequest request;
    request.goal = "Improve main";
    request.model_id = "test-model";
    request.focus_file = session.Context().CurrentWorkspace().File("main.cpp");
    const mornox::AgentSession result = session.Context().Agents().StartSession(session.Context(), request, [&](const mornox::AgentRuntimeEvent&) {
        ++events;
    });

    REQUIRE(result.status == mornox::AgentSessionStatus::Completed);
    REQUIRE(result.model_response == "Plan generated");
    REQUIRE(events >= 3);
    REQUIRE(session.Context().Models().Model("test-model").has_value());
    REQUIRE(session.Context().Agents().Session(result.id).has_value());
    session.Close();
}

void TestAgentRuntimeUsesOperationProtocol() {
    const auto root = MakeTempRoot();
    WriteFile(root / "main.cpp", "int main() { return 0; }\n");

    mornox::VirtualFileSystem vfs;
    mornox::WorkspaceRuntime session(vfs, mornox::InlineJobDispatcher());
    std::string error;
    REQUIRE(session.Open(root, &error));
    session.Context().Models().RegisterProvider(std::make_unique<ToolCallingModelProvider>());

    mornox::AgentSessionRequest request;
    request.goal = "Use a tool";
    request.model_id = "tool-model";
    const mornox::AgentSession result = session.Context().Agents().StartSession(session.Context(), request);

    REQUIRE(result.status == mornox::AgentSessionStatus::Completed);
    REQUIRE(result.operation_ids.size() == 1);
    const auto records = session.Context().AgentOperations().Records();
    REQUIRE(records.size() == 1);
    REQUIRE(records.front().kind == mornox::AgentOperationKind::ReadFile);
    REQUIRE(records.front().ok);
    session.Close();
}

void TestAgentRuntimeStartsSessionsAsynchronously() {
    const auto root = MakeTempRoot();
    WriteFile(root / "main.cpp", "int main() { return 0; }\n");

    mornox::AsyncRuntime async_runtime(1);
    mornox::VirtualFileSystem vfs;
    mornox::WorkspaceRuntime session(vfs, mornox::AsyncJobDispatcher(async_runtime));
    std::string error;
    REQUIRE(session.Open(root, &error));
    session.Context().Models().RegisterProvider(std::make_unique<FakeModelProvider>());

    int events = 0;
    mornox::AgentSessionRequest request;
    request.goal = "Improve main";
    request.model_id = "test-model";
    const mornox::AgentSession started = session.Context().Agents().StartSession(session.Context(), request, [&](const mornox::AgentRuntimeEvent&) {
        ++events;
    });

    REQUIRE(!started.id.empty());
    REQUIRE(started.job_id != 0);
    session.Context().Jobs().Wait(started.job_id);
    const auto completed = session.Context().Agents().Session(started.id);
    REQUIRE(completed.has_value());
    REQUIRE(completed->status == mornox::AgentSessionStatus::Completed);
    REQUIRE(completed->model_response == "Plan generated");
    REQUIRE(events >= 3);
    session.Close();
}

void TestAgentRuntimeCollaborationState() {
    const auto root = MakeTempRoot();
    WriteFile(root / "main.cpp", "int main() { return 0; }\n");

    mornox::VirtualFileSystem vfs;
    mornox::WorkspaceRuntime session(vfs, mornox::InlineJobDispatcher());
    std::string error;
    REQUIRE(session.Open(root, &error));
    session.Context().Models().RegisterProvider(std::make_unique<FakeModelProvider>());

    mornox::AgentSessionRequest request;
    request.work_kind = mornox::AgentWorkKind::Review;
    request.goal = "Review: Check main";
    request.model_id = "test-model";
    request.participants = {{
        .id = "reviewer",
        .display_name = "Reviewer",
        .model_id = "test-model",
        .role = "review",
    }};
    request.ownership_scopes = {{
        .kind = mornox::OwnershipScopeKind::File,
        .file = session.Context().CurrentWorkspace().File("main.cpp"),
        .description = "Review main.cpp",
    }};

    const mornox::AgentSession started = session.Context().Agents().StartSession(session.Context(), request);
    REQUIRE(started.status == mornox::AgentSessionStatus::Completed);
    REQUIRE(started.request.work_kind == mornox::AgentWorkKind::Review);
    REQUIRE(started.request.goal == "Review: Check main");
    REQUIRE(started.request.participants.size() == 1);
    REQUIRE(started.request.ownership_scopes.size() == 1);

    const std::string finding_id = session.Context().Agents().AddFinding({
        .session_id = started.id,
        .category = mornox::AgentWorkKind::Review,
        .severity = mornox::AgentFindingSeverity::Warning,
        .title = "Return value",
        .message = "The return value should be checked.",
        .file = session.Context().CurrentWorkspace().File("main.cpp"),
    });
    REQUIRE(!finding_id.empty());

    const std::string proposal_id = session.Context().Agents().AddProposal({
        .session_id = started.id,
        .title = "Update return value",
        .summary = "Change the return value.",
        .finding_ids = {finding_id},
    });
    REQUIRE(!proposal_id.empty());

    const auto snapshot = session.Context().Agents().Session(started.id);
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->finding_ids.size() == 1);
    REQUIRE(snapshot->proposal_ids.size() == 1);
    REQUIRE(session.Context().Agents().FindingsForSession(started.id).size() == 1);
    REQUIRE(session.Context().Agents().ProposalsForSession(started.id).size() == 1);
    REQUIRE(mornox::ToString(mornox::AgentWorkKind::Security) == "security");

    session.Close();
}

void TestAgentContextAndOperationService() {
    const auto root = MakeTempRoot();
    WriteFile(root / "main.cpp", "int main() { return 0; }\n");

    mornox::VirtualFileSystem vfs;
    mornox::WorkspaceRuntime session(vfs, mornox::InlineJobDispatcher());
    std::string error;
    REQUIRE(session.Open(root, &error));
    const mornox::VirtualFile main_file = session.Context().CurrentWorkspace().File("main.cpp");
    REQUIRE(session.Context().Documents().OpenDocument(main_file, &error) != nullptr);

    mornox::Diagnostic diagnostic;
    diagnostic.location.file = main_file;
    diagnostic.source = "test";
    diagnostic.message = "sample";
    session.Context().Diagnostics().Publish("test", {diagnostic});

    mornox::AgentContextRequest context_request;
    context_request.goal = "explain";
    context_request.focus_file = main_file;
    const mornox::AgentContext context = session.Context().AgentContext().Collect(context_request, session.Context());
    REQUIRE(!context.items.empty());

    const auto snapshot = session.Context().Documents().ReadSnapshot(main_file);
    mornox::AgentOperationRequest operation;
    operation.kind = mornox::AgentOperationKind::ProposeFileReplacement;
    operation.file = main_file;
    operation.source = "agent";
    operation.title = "Change return value";
    operation.replacement_text = "int main() { return 2; }\n";
    operation.expected_document_version = snapshot ? snapshot->version : 0;
    const mornox::AgentOperationResult result = session.Context().AgentOperations().Execute(session.Context(), operation);
    REQUIRE(result.ok);
    REQUIRE(!result.change_set_id.empty());
    REQUIRE(session.Context().Changes().Approve(result.change_set_id).ok);
    REQUIRE(session.Context().Changes().ApplyApproved(session.Context().CurrentWorkspace(), session.Context().Documents(), result.change_set_id, {.save_after_apply = true}).ok);
    REQUIRE(session.Context().CurrentWorkspace().ReadTextFile("main.cpp")->find("return 2") != std::string::npos);
    session.Close();
}

void TestAgentOperationService() {
    const auto root = MakeTempRoot();
    WriteFile(root / "main.cpp", "int main() { return 0; }\n");

    mornox::VirtualFileSystem vfs;
    mornox::WorkspaceRuntime session(vfs, mornox::InlineJobDispatcher());
    std::string error;
    REQUIRE(session.Open(root, &error));
    const mornox::VirtualFile main_file = session.Context().CurrentWorkspace().File("main.cpp");

    int events = 0;
    const auto record_event = [&](const mornox::AgentOperationEvent& event) {
        REQUIRE(!mornox::ToString(event.status).empty());
        ++events;
    };

    mornox::AgentOperationRequest read_request;
    read_request.id = "read-main";
    read_request.kind = mornox::AgentOperationKind::ReadFile;
    read_request.file = main_file;
    mornox::AgentOperationResult read = session.Context().AgentOperations().Execute(session.Context(), read_request, record_event);
    REQUIRE(read.ok);
    REQUIRE(read.text.find("return 0") != std::string::npos);

    mornox::AgentOperationRequest search_request;
    search_request.id = "search-main";
    search_request.kind = mornox::AgentOperationKind::SearchText;
    search_request.query = "return 0";
    mornox::AgentOperationResult search = session.Context().AgentOperations().Execute(session.Context(), search_request, record_event);
    REQUIRE(search.ok);
    REQUIRE(search.search_hits.size() == 1);

    mornox::AgentOperationRequest change_request;
    change_request.id = "change-main";
    change_request.kind = mornox::AgentOperationKind::ProposeFileReplacement;
    change_request.file = main_file;
    change_request.title = "Change return value";
    change_request.replacement_text = "int main() { return 3; }\n";
    mornox::AgentOperationResult change = session.Context().AgentOperations().Execute(session.Context(), change_request, record_event);
    REQUIRE(change.ok);
    REQUIRE(!change.change_set_id.empty());
    REQUIRE(session.Context().Changes().Get(change.change_set_id).has_value());
    auto change_record = session.Context().AgentOperations().Record("change-main");
    REQUIRE(change_record.has_value());
    REQUIRE(change_record->ok);
    REQUIRE(change_record->change_set_id == change.change_set_id);

    session.Context().AgentTools().RegisterTool({
        .id = "test.echo",
        .description = "Echo input",
        .handler = [](const mornox::Value& input) {
            return input;
        },
    });
    mornox::AgentOperationRequest tool_request;
    tool_request.id = "tool-echo";
    tool_request.kind = mornox::AgentOperationKind::CallTool;
    tool_request.tool_id = "test.echo";
    tool_request.input = mornox::Value(mornox::Value::ObjectValue({{"value", mornox::Value("ok")}}));
    mornox::AgentOperationResult tool = session.Context().AgentOperations().Execute(session.Context(), tool_request, record_event);
    REQUIRE(tool.ok);
    REQUIRE(tool.payload.has_value());
    REQUIRE(tool.payload->StringValue("value").value_or("") == "ok");
    REQUIRE(session.Context().AgentOperations().Records().size() >= 4);
    const auto jobs = session.Context().Jobs().Jobs();
    REQUIRE(std::any_of(jobs.begin(), jobs.end(), [](const mornox::JobRecord& job) {
        return job.kind == mornox::JobKind::Agent && job.status == mornox::JobStatus::Succeeded;
    }));

    session.Context().Approvals().SetAutoApprove(false);
    mornox::AgentOperationRequest denied_request;
    denied_request.id = "denied-change";
    denied_request.kind = mornox::AgentOperationKind::ProposeFileReplacement;
    denied_request.file = main_file;
    denied_request.replacement_text = "int main() { return 4; }\n";
    const mornox::AgentOperationResult denied = session.Context().AgentOperations().Execute(session.Context(), denied_request, record_event);
    REQUIRE(!denied.ok);
    REQUIRE(denied.error.find("denied") != std::string::npos);
    REQUIRE(!session.Context().Approvals().History().empty());
    const auto denied_record = session.Context().AgentOperations().Record("denied-change");
    REQUIRE(denied_record.has_value());
    REQUIRE(!denied_record->ok);
    REQUIRE(events >= 8);
    session.Close();
}

void TestAgentToolRegistryReplacesToolsSafely() {
    mornox::AgentToolRegistry registry;
    mornox::RegistrationHandle first = registry.RegisterTool({
        .id = "test.echo",
        .description = "First echo",
        .handler = [](const mornox::Value&) {
            return mornox::Value("first");
        },
    });
    mornox::RegistrationHandle second = registry.RegisterTool({
        .id = "test.echo",
        .description = "Second echo",
        .handler = [](const mornox::Value&) {
            return mornox::Value("second");
        },
    });

    REQUIRE(registry.Tools().size() == 1);
    REQUIRE(registry.CallTool("test.echo", mornox::Value::ObjectValue())->AsString() == "second");
    first.Unregister();
    REQUIRE(registry.CallTool("test.echo", mornox::Value::ObjectValue())->AsString() == "second");
    second.Unregister();
    REQUIRE(!registry.CallTool("test.echo", mornox::Value::ObjectValue()).has_value());
}

}

TEST_CASE("Agent runtime and model service", "[agent]") {
    mornox::tests::TestAgentRuntimeAndModelService();
}

TEST_CASE("Agent runtime uses operation protocol", "[agent]") {
    mornox::tests::TestAgentRuntimeUsesOperationProtocol();
}

TEST_CASE("Agent runtime starts sessions asynchronously", "[agent]") {
    mornox::tests::TestAgentRuntimeStartsSessionsAsynchronously();
}

TEST_CASE("Agent runtime collaboration state", "[agent]") {
    mornox::tests::TestAgentRuntimeCollaborationState();
}

TEST_CASE("Agent context and operation service", "[agent]") {
    mornox::tests::TestAgentContextAndOperationService();
}

TEST_CASE("Agent operation service", "[agent]") {
    mornox::tests::TestAgentOperationService();
}

TEST_CASE("Agent tool registry replaces tools safely", "[agent]") {
    mornox::tests::TestAgentToolRegistryReplacesToolsSafely();
}
