#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "vanta/agent/agent_context.h"
#include "vanta/agent/model_service.h"
#include "vanta/core/value.h"

namespace vanta {

class WorkspaceContext;

enum class AgentSessionStatus {
    Pending,
    CollectingContext,
    Planning,
    Running,
    WaitingForApproval,
    Completed,
    Failed,
    Cancelled,
};

enum class AgentStepKind {
    Think,
    ToolCall,
    Operation,
    Patch,
};

struct AgentPlanStep {
    std::string id;
    AgentStepKind kind = AgentStepKind::Think;
    std::string title;
    std::optional<Value> payload;
};

struct AgentPlan {
    std::vector<AgentPlanStep> steps;
};

struct AgentSessionRequest {
    std::string goal;
    std::string model_id;
    VirtualFile focus_file;
    std::vector<Diagnostic> diagnostics;
};

struct AgentRuntimeEvent {
    std::string session_id;
    AgentSessionStatus status = AgentSessionStatus::Pending;
    std::string message;
    std::optional<Value> payload;
};

struct AgentSession {
    std::string id;
    AgentSessionRequest request;
    AgentSessionStatus status = AgentSessionStatus::Pending;
    AgentContext context;
    AgentPlan plan;
    std::vector<AgentRuntimeEvent> events;
    std::string model_response;
    std::string change_set_id;
    std::string error;
};

using AgentRuntimeCallback = std::function<void(const AgentRuntimeEvent&)>;

class AgentRuntime {
public:
    AgentSession StartSession(
        WorkspaceContext& context,
        AgentSessionRequest request,
        AgentRuntimeCallback on_event = {});
    bool CancelSession(const std::string& session_id);
    std::optional<AgentSession> Session(const std::string& session_id) const;
    std::vector<AgentSession> Sessions() const;
    void Clear();

private:
    AgentSession& CreateSession(AgentSessionRequest request);
    void Update(AgentSession& session, AgentSessionStatus status, std::string message, AgentRuntimeCallback& on_event, std::optional<Value> payload = std::nullopt);
    void ExecuteToolCalls(WorkspaceContext& context, AgentSession& session, const ModelResponse& response, AgentRuntimeCallback& on_event);

    std::uint64_t next_session_id_ = 1;
    std::map<std::string, AgentSession> sessions_;
};

std::string ToString(AgentSessionStatus status);
std::string ToString(AgentStepKind kind);

}
