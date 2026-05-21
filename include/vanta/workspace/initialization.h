#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "vanta/core/event.h"
#include "vanta/core/value.h"

namespace vanta {

enum class WorkspaceInitializationStage {
    WorkspaceOpened,
    ProjectModelResolved,
    ComponentsReady,
    FileIndexReady,
    LanguageServicesReady,
    BuildModelReady,
    AgentContextReady,
};

enum class WorkspaceInitializationStatus {
    Pending,
    Running,
    Completed,
    Failed,
};

struct WorkspaceInitializationStep {
    WorkspaceInitializationStage stage = WorkspaceInitializationStage::WorkspaceOpened;
    WorkspaceInitializationStatus status = WorkspaceInitializationStatus::Pending;
    std::string message;
};

struct WorkspaceInitializationChangeEvent {
    WorkspaceInitializationStep step;
};

class WorkspaceInitializationPipeline {
public:
    void Reset();
    void Start(WorkspaceInitializationStage stage, std::string message = {});
    void Complete(WorkspaceInitializationStage stage, std::string message = {});
    void Fail(WorkspaceInitializationStage stage, std::string message);
    std::optional<WorkspaceInitializationStep> Step(WorkspaceInitializationStage stage) const;
    std::vector<WorkspaceInitializationStep> Steps() const;
    bool Completed(WorkspaceInitializationStage stage) const;
    std::uint64_t OnDidChangeStep(EventBus<WorkspaceInitializationChangeEvent>::Listener listener);
    void RemoveStepListener(std::uint64_t listener_id);

private:
    WorkspaceInitializationStep& Ensure(WorkspaceInitializationStage stage);
    void Publish(const WorkspaceInitializationStep& step);

    std::map<WorkspaceInitializationStage, WorkspaceInitializationStep> steps_;
    EventBus<WorkspaceInitializationChangeEvent> on_did_change_;
};

std::string ToString(WorkspaceInitializationStage stage);
std::string ToString(WorkspaceInitializationStatus status);

}
