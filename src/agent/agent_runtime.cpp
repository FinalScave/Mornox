#include "vanta/agent/agent_runtime.h"

#include <utility>

#include "core/value_projection.h"
#include "vanta/agent/agent_operation.h"
#include "vanta/core/json_codec.h"
#include "vanta/workspace/workspace_context.h"

namespace vanta {
namespace {

Value AgentPayload(Value value) {
    return value;
}

}

AgentSession AgentRuntime::StartSession(WorkspaceContext& context, AgentSessionRequest request, AgentRuntimeCallback on_event) {
    AgentSession& session = CreateSession(std::move(request));
    Update(session, AgentSessionStatus::CollectingContext, "Collecting context", on_event);

    AgentContextRequest context_request;
    context_request.goal = session.request.goal;
    context_request.focus_file = session.request.focus_file;
    context_request.diagnostics = session.request.diagnostics;
    session.context = context.AgentContext().Collect(context_request, context);
    Update(session, AgentSessionStatus::Planning, "Requesting model plan", on_event, AgentPayload(Value::ObjectValue({
        {"contextItems", Value(static_cast<std::int64_t>(session.context.items.size()))},
    })));

    ModelRequest model_request;
    model_request.model_id = session.request.model_id;
    for (const AgentToolDefinition& tool : context.AgentTools().Tools()) {
        model_request.tools.push_back({
            .id = tool.id,
            .description = tool.description,
            .input_schema = tool.input_schema,
        });
    }
    model_request.messages = {
        {
            .role = ModelMessageRole::System,
            .content = "You are Vanta's coding agent. Return a concise plan and propose workspace edits through IDE operations.",
        },
        {
            .role = ModelMessageRole::User,
            .content = session.request.goal,
        },
    };

    ModelResponse response = context.Models().Complete(model_request, [&](const ModelStreamEvent& event) {
        if (event.kind == ModelStreamEventKind::Delta && !event.text.empty()) {
            Update(session, AgentSessionStatus::Running, event.text, on_event, AgentPayload(Value::ObjectValue({
                {"modelEvent", Value(ToString(event.kind))},
            })));
        }
    });
    session.model_response = response.content;
    if (!response.ok) {
        session.error = response.error.empty() ? "Model request failed" : response.error;
        Update(session, AgentSessionStatus::Failed, session.error, on_event);
        return session;
    }

    session.plan.steps.push_back({
        .id = session.id + ".model",
        .kind = AgentStepKind::Think,
        .title = response.content.empty() ? "Model response" : response.content,
        .payload = response.payload,
    });
    ExecuteToolCalls(context, session, response, on_event);
    if (session.status == AgentSessionStatus::Failed) {
        return session;
    }
    Update(session, AgentSessionStatus::Completed, "Agent session completed", on_event);
    return session;
}

bool AgentRuntime::CancelSession(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return false;
    }
    AgentSession& session = it->second;
    if (session.status == AgentSessionStatus::Completed ||
        session.status == AgentSessionStatus::Failed ||
        session.status == AgentSessionStatus::Cancelled) {
        return false;
    }
    session.status = AgentSessionStatus::Cancelled;
    session.events.push_back({
        .session_id = session.id,
        .status = AgentSessionStatus::Cancelled,
        .message = "Agent session cancelled",
    });
    return true;
}

std::optional<AgentSession> AgentRuntime::Session(const std::string& session_id) const {
    auto it = sessions_.find(session_id);
    return it == sessions_.end() ? std::nullopt : std::optional<AgentSession>(it->second);
}

std::vector<AgentSession> AgentRuntime::Sessions() const {
    std::vector<AgentSession> result;
    for (const auto& [id, session] : sessions_) {
        (void)id;
        result.push_back(session);
    }
    return result;
}

void AgentRuntime::Clear() {
    sessions_.clear();
}

AgentSession& AgentRuntime::CreateSession(AgentSessionRequest request) {
    AgentSession session;
    session.id = "agent-session-" + std::to_string(next_session_id_++);
    session.request = std::move(request);
    auto [it, inserted] = sessions_.emplace(session.id, std::move(session));
    (void)inserted;
    return it->second;
}

void AgentRuntime::Update(AgentSession& session, AgentSessionStatus status, std::string message, AgentRuntimeCallback& on_event, std::optional<Value> payload) {
    session.status = status;
    AgentRuntimeEvent event{
        .session_id = session.id,
        .status = status,
        .message = std::move(message),
        .payload = std::move(payload),
    };
    session.events.push_back(event);
    if (on_event) {
        on_event(event);
    }
}

void AgentRuntime::ExecuteToolCalls(WorkspaceContext& context, AgentSession& session, const ModelResponse& response, AgentRuntimeCallback& on_event) {
    int index = 0;
    for (const ModelToolCall& call : response.tool_calls) {
        ++index;
        Update(session, AgentSessionStatus::Running, "Calling agent tool " + call.tool_id, on_event, AgentPayload(Value::ObjectValue({
            {"toolId", Value(call.tool_id)},
        })));
        AgentOperationRequest operation;
        operation.id = session.id + ".tool." + std::to_string(index);
        operation.kind = AgentOperationKind::CallTool;
        operation.tool_id = call.tool_id;
        operation.input = call.input;
        const AgentOperationResult result = context.AgentOperations().Execute(context, operation, [&](const AgentOperationEvent& event) {
            Update(session, AgentSessionStatus::Running, event.message, on_event, AgentPayload(internal::AgentOperationEventProjection(event)));
        });
        session.plan.steps.push_back({
            .id = operation.id,
            .kind = AgentStepKind::Operation,
            .title = result.ok ? "Tool call completed: " + call.tool_id : "Tool call failed: " + call.tool_id,
            .payload = AgentPayload(internal::AgentOperationResultProjection(result)),
        });
        if (!result.ok) {
            session.error = result.error.empty() ? result.message : result.error;
            Update(session, AgentSessionStatus::Failed, session.error, on_event);
            return;
        }
    }
}

std::string ToString(AgentSessionStatus status) {
    switch (status) {
    case AgentSessionStatus::Pending:
        return "pending";
    case AgentSessionStatus::CollectingContext:
        return "collectingContext";
    case AgentSessionStatus::Planning:
        return "planning";
    case AgentSessionStatus::Running:
        return "running";
    case AgentSessionStatus::WaitingForApproval:
        return "waitingForApproval";
    case AgentSessionStatus::Completed:
        return "completed";
    case AgentSessionStatus::Failed:
        return "failed";
    case AgentSessionStatus::Cancelled:
        return "cancelled";
    }
    return "pending";
}

std::string ToString(AgentStepKind kind) {
    switch (kind) {
    case AgentStepKind::Think:
        return "think";
    case AgentStepKind::ToolCall:
        return "toolCall";
    case AgentStepKind::Operation:
        return "operation";
    case AgentStepKind::Patch:
        return "patch";
    }
    return "think";
}

}
