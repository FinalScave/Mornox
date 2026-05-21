#include "vanta/workspace/initialization.h"

#include <cstdint>
#include <utility>

namespace vanta {
namespace {

std::vector<WorkspaceInitializationStage> OrderedStages() {
    return {
        WorkspaceInitializationStage::WorkspaceOpened,
        WorkspaceInitializationStage::ProjectModelResolved,
        WorkspaceInitializationStage::ComponentsReady,
        WorkspaceInitializationStage::FileIndexReady,
        WorkspaceInitializationStage::LanguageServicesReady,
        WorkspaceInitializationStage::BuildModelReady,
        WorkspaceInitializationStage::AgentContextReady,
    };
}

}

void WorkspaceInitializationPipeline::Reset() {
    steps_.clear();
    for (WorkspaceInitializationStage stage : OrderedStages()) {
        Ensure(stage);
    }
}

void WorkspaceInitializationPipeline::Start(WorkspaceInitializationStage stage, std::string message) {
    WorkspaceInitializationStep& value = Ensure(stage);
    value.status = WorkspaceInitializationStatus::Running;
    value.message = std::move(message);
    Publish(value);
}

void WorkspaceInitializationPipeline::Complete(WorkspaceInitializationStage stage, std::string message) {
    WorkspaceInitializationStep& value = Ensure(stage);
    value.status = WorkspaceInitializationStatus::Completed;
    value.message = std::move(message);
    Publish(value);
}

void WorkspaceInitializationPipeline::Fail(WorkspaceInitializationStage stage, std::string message) {
    WorkspaceInitializationStep& value = Ensure(stage);
    value.status = WorkspaceInitializationStatus::Failed;
    value.message = std::move(message);
    Publish(value);
}

std::optional<WorkspaceInitializationStep> WorkspaceInitializationPipeline::Step(WorkspaceInitializationStage stage) const {
    auto it = steps_.find(stage);
    return it == steps_.end() ? std::nullopt : std::optional<WorkspaceInitializationStep>(it->second);
}

std::vector<WorkspaceInitializationStep> WorkspaceInitializationPipeline::Steps() const {
    std::vector<WorkspaceInitializationStep> values;
    for (WorkspaceInitializationStage stage : OrderedStages()) {
        auto it = steps_.find(stage);
        if (it != steps_.end()) {
            values.push_back(it->second);
        }
    }
    return values;
}

bool WorkspaceInitializationPipeline::Completed(WorkspaceInitializationStage stage) const {
    auto value = Step(stage);
    return value && value->status == WorkspaceInitializationStatus::Completed;
}

std::uint64_t WorkspaceInitializationPipeline::OnDidChangeStep(EventBus<WorkspaceInitializationChangeEvent>::Listener listener) {
    return on_did_change_.Subscribe(std::move(listener));
}

void WorkspaceInitializationPipeline::RemoveStepListener(std::uint64_t listener_id) {
    on_did_change_.Unsubscribe(listener_id);
}

WorkspaceInitializationStep& WorkspaceInitializationPipeline::Ensure(WorkspaceInitializationStage stage) {
    WorkspaceInitializationStep& value = steps_[stage];
    value.stage = stage;
    return value;
}

void WorkspaceInitializationPipeline::Publish(const WorkspaceInitializationStep& step) {
    on_did_change_.Publish({.step = step});
}

std::string ToString(WorkspaceInitializationStage stage) {
    switch (stage) {
    case WorkspaceInitializationStage::WorkspaceOpened:
        return "workspaceOpened";
    case WorkspaceInitializationStage::ProjectModelResolved:
        return "projectModelResolved";
    case WorkspaceInitializationStage::ComponentsReady:
        return "componentsReady";
    case WorkspaceInitializationStage::FileIndexReady:
        return "fileIndexReady";
    case WorkspaceInitializationStage::LanguageServicesReady:
        return "languageServicesReady";
    case WorkspaceInitializationStage::BuildModelReady:
        return "buildModelReady";
    case WorkspaceInitializationStage::AgentContextReady:
        return "agentContextReady";
    }
    return "workspaceOpened";
}

std::string ToString(WorkspaceInitializationStatus status) {
    switch (status) {
    case WorkspaceInitializationStatus::Pending:
        return "pending";
    case WorkspaceInitializationStatus::Running:
        return "running";
    case WorkspaceInitializationStatus::Completed:
        return "completed";
    case WorkspaceInitializationStatus::Failed:
        return "failed";
    }
    return "pending";
}

}
