#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "vanta/core/localization.h"
#include "vanta/core/registration.h"
#include "vanta/plugin/plugin_protocol.h"
#include "vanta/workspace/command_registry.h"
#include "vanta/workspace/ide_event.h"

namespace vanta {

class AgentToolRegistry;
class AgentContextCollector;
class AgentOperationService;
class AgentOperationJournal;
class AgentRuntime;
class AsyncRuntime;
class ApprovalService;
class BuildService;
class CapabilityRegistry;
class ChangeSetService;
class CodeIntelligenceService;
class Component;
struct ComponentContribution;
class DebugService;
class DiagnosticService;
class DocumentService;
class ExecutionService;
class GitService;
class IndexService;
class JobService;
class LanguageRegistry;
class ModelService;
class Project;
class ProjectManager;
class ProjectTemplateService;
class RunConfigurationRegistry;
class ScratchFileService;
class SettingsService;
class VirtualFileSystem;
class Workspace;
class WorkspaceInitializationPipeline;
class WorkspaceRuntime;
class WorkspaceTrustService;

class WorkspaceContext {
public:
    explicit WorkspaceContext(WorkspaceRuntime& runtime);

    void SetProject(Project* project);
    Workspace& CurrentWorkspace();
    const Workspace& CurrentWorkspace() const;
    bool WorkspaceOpen() const;
    Project* CurrentProject();
    const Project* CurrentProject() const;
    Project& RequireProject();
    const Project& RequireProject() const;

    CommandRegistry& Commands();
    DocumentService& Documents();
    VirtualFileSystem& FileSystems();
    LanguageRegistry& Languages();
    CodeIntelligenceService& CodeIntelligence();
    DiagnosticService& Diagnostics();
    BuildService& Build();
    ExecutionService& Execution();
    RunConfigurationRegistry& RunConfigurations();
    const RunConfigurationRegistry& RunConfigurations() const;
    JobService& Jobs();
    IndexService& Indexes();
    ProjectManager& Projects();
    AgentToolRegistry& AgentTools();
    AgentContextCollector& AgentContext();
    AgentOperationService& AgentOperations();
    AgentOperationJournal& AgentOperationJournal();
    ModelService& Models();
    AgentRuntime& Agents();
    ChangeSetService& Changes();
    DebugService& Debug();
    GitService& Git();
    CapabilityRegistry& Capabilities();
    WorkspaceInitializationPipeline& Initialization();
    ProjectTemplateService& ProjectTemplates();
    ScratchFileService& ScratchFiles();
    ApprovalService& Approvals();
    WorkspaceTrustService& WorkspaceTrust();
    SettingsService& Settings();
    LocalizationRegistry& Localization();
    const LocalizationRegistry& Localization() const;
    Localizer LocalizerFor(std::string owner_id) const;
    AsyncRuntime& Async();
    IdeEventBus& Events();
    const IdeEventBus& Events() const;
    void Publish(IdeEvent event);
    void RefreshProject();

    RegistrationHandle RegisterContribution(PluginRegistration contribution);
    std::vector<PluginRegistration> Contributions() const;
    std::vector<PluginRegistration> Contributions(PluginRegistrationKind kind) const;
    void BindComponent(std::unique_ptr<Component> component);
    bool UnbindComponent(const std::string& id);
    RegistrationHandle RegisterComponentContribution(ComponentContribution contribution);

    std::uint64_t OnEvent(const Component& owner, IdeEventBus::Listener listener);
    std::uint64_t OnEvent(const Component& owner, IdeEventKind kind, IdeEventBus::Listener listener);
    void RemoveEventSubscriptions(const std::string& component_id);

private:
    WorkspaceRuntime* runtime_ = nullptr;
    Project* project_ = nullptr;
    std::map<std::string, std::vector<std::uint64_t>> event_subscriptions_;
};

}
