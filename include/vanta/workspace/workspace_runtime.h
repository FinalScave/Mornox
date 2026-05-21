#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>

#include "vanta/core/localization.h"
#include "vanta/agent/agent_tool_registry.h"
#include "vanta/agent/agent_context.h"
#include "vanta/agent/agent_operation.h"
#include "vanta/agent/agent_runtime.h"
#include "vanta/agent/model_service.h"
#include "vanta/debug/debug_service.h"
#include "vanta/execution/build_service.h"
#include "vanta/execution/job_service.h"
#include "vanta/workspace/change_set_service.h"
#include "vanta/workspace/capability_service.h"
#include "vanta/language/code_intelligence_service.h"
#include "vanta/workspace/diagnostic_service.h"
#include "vanta/workspace/document_service.h"
#include "vanta/workspace/workspace.h"
#include "vanta/execution/execution_service.h"
#include "vanta/workspace/git_service.h"
#include "vanta/workspace/ide_event.h"
#include "vanta/workspace/command_registry.h"
#include "vanta/language/document_language_sync.h"
#include "vanta/language/language_service.h"
#include "vanta/platform/async.h"
#include "vanta/plugin/approval_service.h"
#include "vanta/plugin/plugin_protocol.h"
#include "vanta/workspace/index_service.h"
#include "vanta/workspace/initialization.h"
#include "vanta/workspace/workspace_context.h"
#include "vanta/project/project.h"
#include "vanta/project/project_manager.h"
#include "vanta/project/project_template.h"
#include "vanta/execution/run_configuration.h"
#include "vanta/workspace/settings_service.h"
#include "vanta/workspace/workspace_trust.h"
#include "vanta/vfs/file_watcher.h"
#include "vanta/vfs/virtual_file_system.h"

namespace vanta {

class ContributionRegistry;
class ProjectStateStore;

class WorkspaceRuntime {
public:
    WorkspaceRuntime(VirtualFileSystem& vfs, AsyncRuntime& async);
    WorkspaceRuntime(const WorkspaceRuntime&) = delete;
    WorkspaceRuntime& operator=(const WorkspaceRuntime&) = delete;
    ~WorkspaceRuntime();

    bool Open(const std::filesystem::path& workspace_path, std::string* error_message = nullptr, bool initialize = true);
    void InitializeWorkspace();
    void Close();
    bool IsOpen() const;

    void RefreshProject();
    void StartDocumentSync();
    void StopDocumentSync();
    bool StartFileWatcher(std::string* error_message = nullptr);
    void StopFileWatcher();
    WorkspaceContext& Context();
    const WorkspaceContext& Context() const;

private:
    friend class WorkspaceContext;

    void BindComponent(std::unique_ptr<Component> component);
    bool UnbindComponent(const std::string& id);
    ProjectManager& Projects();
    const ProjectManager& Projects() const;
    Workspace& WorkspaceValue();
    const Workspace& WorkspaceValue() const;
    Project& ProjectValue();
    const Project& ProjectValue() const;
    DocumentService& Documents();
    const DocumentService& Documents() const;
    BuildService& Build();
    const BuildService& Build() const;
    AgentToolRegistry& AgentTools();
    const AgentToolRegistry& AgentTools() const;
    AgentContextCollector& AgentContext();
    AgentOperationService& AgentOperations();
    const AgentOperationService& AgentOperations() const;
    AgentOperationJournal& AgentOperationJournalValue();
    const AgentOperationJournal& AgentOperationJournalValue() const;
    ModelService& Models();
    const ModelService& Models() const;
    AgentRuntime& AgentRuntimeValue();
    const AgentRuntime& AgentRuntimeValue() const;
    ChangeSetService& Changes();
    DebugService& Debug();
    const DebugService& Debug() const;
    ExecutionService& Execution();
    const ExecutionService& Execution() const;
    GitService& Git();
    const GitService& Git() const;
    RunConfigurationRegistry& RunConfigurations();
    const RunConfigurationRegistry& RunConfigurations() const;
    LanguageRegistry& Languages();
    const LanguageRegistry& Languages() const;
    CodeIntelligenceService& CodeIntelligence();
    DiagnosticService& Diagnostics();
    const DiagnosticService& Diagnostics() const;
    JobService& Jobs();
    const JobService& Jobs() const;
    CommandRegistry& Commands();
    const CommandRegistry& Commands() const;
    IndexService& Indexes();
    const IndexService& Indexes() const;
    CapabilityRegistry& Capabilities();
    const CapabilityRegistry& Capabilities() const;
    WorkspaceInitializationPipeline& Initialization();
    const WorkspaceInitializationPipeline& Initialization() const;
    ProjectTemplateService& ProjectTemplates();
    const ProjectTemplateService& ProjectTemplates() const;
    ScratchFileService& ScratchFiles();
    const ScratchFileService& ScratchFiles() const;
    ApprovalService& Approvals();
    const ApprovalService& Approvals() const;
    WorkspaceTrustService& WorkspaceTrust();
    const WorkspaceTrustService& WorkspaceTrust() const;
    SettingsService& WorkspaceSettings();
    LocalizationRegistry& Localization();
    const LocalizationRegistry& Localization() const;
    PluginStorageService& PluginStorage();
    VirtualFileSystem& FileSystems();
    const VirtualFileSystem& FileSystems() const;
    AsyncRuntime& AsyncValue();

    std::uint64_t OnEvent(IdeEventBus::Listener listener);
    void RemoveEventListener(std::uint64_t listener_id);
    IdeEventBus& EventsValue();
    void Publish(IdeEvent event);

    Workspace workspace_;
    Project project_;
    DocumentService documents_;
    std::unique_ptr<BuildService> build_;
    AgentToolRegistry agent_;
    AgentContextCollector agent_context_;
    AgentOperationService agent_operations_;
    AgentOperationJournal agent_operation_journal_;
    ModelService model_service_;
    AgentRuntime agent_runtime_;
    ChangeSetService changes_;
    DebugService debug_;
    ExecutionService execution_;
    std::unique_ptr<GitService> git_;
    std::unique_ptr<RunConfigurationRegistry> run_configuration_registry_;
    std::unique_ptr<LanguageRegistry> languages_;
    CodeIntelligenceService code_intelligence_;
    DiagnosticService diagnostics_;
    JobService jobs_;
    std::unique_ptr<CommandRegistry> commands_;
    IndexService indexes_;
    CapabilityRegistry capabilities_;
    WorkspaceInitializationPipeline initialization_;
    ProjectTemplateService project_templates_;
    ScratchFileService scratch_files_;
    ApprovalService approvals_;
    WorkspaceTrustService workspace_trust_;
    SettingsService workspace_settings_;
    LocalizationRegistry localization_;
    PluginStorageService plugin_storage_;
    std::unique_ptr<ContributionRegistry> contributions_;

    void AddComponentContribution(ComponentContribution contribution);
    RegistrationHandle RegisterComponentContribution(ComponentContribution contribution);
    bool RemoveComponentContribution(const std::string& id);
    std::vector<ComponentContribution> ComponentContributions() const;
    RegistrationHandle RegisterContribution(PluginRegistration contribution);
    std::vector<PluginRegistration> Contributions() const;
    std::vector<PluginRegistration> Contributions(PluginRegistrationKind kind) const;
    void BindBuiltinComponents();
    void RefreshIndexes(std::string title);
    void UpdateCoreCapabilities();
    void ReconcileComponentContributions();
    void ConnectEventRelays();
    void DisconnectEventRelays();
    void HandleFileChange(const VirtualFileChangeEvent& event);
    void PublishDocumentEvent(const DocumentChangeEvent& event);
    void PublishJobEvent(const JobChangeEvent& event);

    VirtualFileSystem& vfs_;
    AsyncRuntime& async_;
    IdeEventBus events_;
    ProjectManager project_manager_;
    std::unique_ptr<ProjectStateStore> project_state_store_;
    ProjectState project_state_;
    ComponentContributionRegistry component_contributions_;
    std::map<std::string, bool> active_contributed_components_;
    WorkspaceContext context_;
    std::unique_ptr<FileWatcher> file_watcher_;
    std::unique_ptr<DocumentLanguageSynchronizer> document_sync_;
    std::uint64_t document_listener_ = 0;
    std::uint64_t diagnostic_listener_ = 0;
    std::uint64_t job_listener_ = 0;
    std::map<JobId, JobStatus> job_statuses_;
    bool open_ = false;
    bool initialized_ = false;
};

}
