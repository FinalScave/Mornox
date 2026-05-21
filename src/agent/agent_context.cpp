#include "vanta/agent/agent_context.h"

#include <algorithm>

#include "vanta/agent/agent_tool_registry.h"
#include "vanta/workspace/workspace_context.h"
#include "vanta/project/project.h"
#include "vanta/project/project_manager.h"
#include "vanta/core/json_codec.h"
#include "vanta/workspace/diagnostic_service.h"
#include "vanta/workspace/document_service.h"
#include "vanta/workspace/index_service.h"

namespace vanta {
namespace {

Value AgentPayload(Value value) {
    return value;
}

Value DiagnosticProjection(const Diagnostic& diagnostic) {
    return Value::ObjectValue({
        {"file", Value(diagnostic.location.file.ToUri().ToString())},
        {"line", Value(static_cast<std::int64_t>(diagnostic.location.line))},
        {"column", Value(static_cast<std::int64_t>(diagnostic.location.column))},
        {"severity", Value(ToString(diagnostic.severity))},
        {"source", Value(diagnostic.source)},
        {"message", Value(diagnostic.message)},
    });
}

Value AttachmentInfosProjection(const ProjectModel& model) {
    Value::Array values;
    for (const ProjectAttachmentInfo& info : model.attachment_infos) {
        values.push_back(Value::ObjectValue({
            {"id", Value(info.id)},
            {"kind", Value(info.kind)},
            {"title", Value(info.title)},
            {"summary", info.summary},
            {"projection", info.projection},
        }));
    }
    return Value::ArrayValue(std::move(values));
}

class DocumentContextProvider final : public AgentContextProvider {
public:
    std::string Id() const override {
        return "vanta.context.documents";
    }

    std::vector<AgentContextItem> Collect(const AgentContextRequest& request, WorkspaceContext& context) const override {
        std::vector<AgentContextItem> items;
        if (request.focus_file.Valid()) {
            AddDocument(context, request.focus_file, "Focused document", items);
        }
        for (const VirtualFile& file : context.Documents().OpenDocuments()) {
            if (request.focus_file.Valid() && file.ToUri() == request.focus_file.ToUri()) {
                continue;
            }
            AddDocument(context, file, "Open document", items);
        }
        return items;
    }

private:
    void AddDocument(WorkspaceContext& context, const VirtualFile& file, std::string title, std::vector<AgentContextItem>& items) const {
        const auto snapshot = context.Documents().ReadSnapshot(file);
        if (!snapshot) {
            return;
        }
        items.push_back({
            .provider_id = Id(),
            .kind = AgentContextItemKind::Document,
            .title = std::move(title),
            .file = file,
            .text = snapshot->text,
            .payload = AgentPayload(Value::ObjectValue({
                {"uri", Value(file.ToUri().ToString())},
                {"open", Value(snapshot->open)},
                {"dirty", Value(snapshot->dirty)},
                {"version", Value(static_cast<std::int64_t>(snapshot->version))},
            })),
        });
    }
};

class DiagnosticsContextProvider final : public AgentContextProvider {
public:
    std::string Id() const override {
        return "vanta.context.diagnostics";
    }

    std::vector<AgentContextItem> Collect(const AgentContextRequest& request, WorkspaceContext& context) const override {
        const std::vector<Diagnostic> diagnostics = request.diagnostics.empty() ? context.Diagnostics().AllDiagnostics() : request.diagnostics;
        Value::Array values;
        std::string text;
        for (const Diagnostic& diagnostic : diagnostics) {
            values.push_back(DiagnosticProjection(diagnostic));
            text += context.AgentTools().ExplainDiagnostic(diagnostic);
            text += '\n';
        }
        if (diagnostics.empty()) {
            return {};
        }
        return {{
            .provider_id = Id(),
            .kind = AgentContextItemKind::Diagnostics,
            .title = "Diagnostics",
            .file = {},
            .text = std::move(text),
            .payload = AgentPayload(Value::ArrayValue(std::move(values))),
        }};
    }
};

class ProjectModelContextProvider final : public AgentContextProvider {
public:
    std::string Id() const override {
        return "vanta.context.project";
    }

    std::vector<AgentContextItem> Collect(const AgentContextRequest&, WorkspaceContext& context) const override {
        const ProjectModel& model = context.RequireProject().Model();
        return {{
            .provider_id = Id(),
            .kind = AgentContextItemKind::Project,
            .title = "Project model",
            .file = model.root,
            .text = PrimaryProjectType(model),
            .payload = AgentPayload(Value::ObjectValue({
                {"root", Value(model.root.ToUri().ToString())},
                {"type", Value(PrimaryProjectType(model))},
                {"modules", Value(static_cast<std::int64_t>(model.modules.size()))},
                {"attachments", AttachmentInfosProjection(model)},
            })),
        }};
    }
};

class JobContextProvider final : public AgentContextProvider {
public:
    std::string Id() const override {
        return "vanta.context.jobs";
    }

    std::vector<AgentContextItem> Collect(const AgentContextRequest&, WorkspaceContext& context) const override {
        Value::Array values;
        std::string text;
        for (const JobRecord& job : context.Jobs().Jobs()) {
            values.push_back(Value::ObjectValue({
                {"id", Value(static_cast<std::int64_t>(job.id))},
                {"kind", Value(ToString(job.kind))},
                {"status", Value(ToString(job.status))},
                {"title", Value(job.title)},
            }));
            text += job.title + " [" + ToString(job.status) + "]\n";
        }
        if (values.empty()) {
            return {};
        }
        return {{
            .provider_id = Id(),
            .kind = AgentContextItemKind::Job,
            .title = "Recent jobs",
            .file = {},
            .text = std::move(text),
            .payload = AgentPayload(Value::ArrayValue(std::move(values))),
        }};
    }
};

class SearchIndexContextProvider final : public AgentContextProvider {
public:
    std::string Id() const override {
        return "vanta.context.searchIndex";
    }

    std::vector<AgentContextItem> Collect(const AgentContextRequest&, WorkspaceContext& context) const override {
        std::size_t entries = 0;
        if (auto snapshot = context.Indexes().Snapshot("vanta.index.search")) {
            entries = snapshot->item_count;
        }
        return {{
            .provider_id = Id(),
            .kind = AgentContextItemKind::SearchIndex,
            .title = "Workspace file index",
            .file = context.CurrentWorkspace().RootFile(),
            .text = std::to_string(entries) + " indexed entries",
            .payload = AgentPayload(Value::ObjectValue({
                {"entries", Value(static_cast<std::int64_t>(entries))},
            })),
        }};
    }
};

class GitDiffContextProvider final : public AgentContextProvider {
public:
    explicit GitDiffContextProvider(const GitService& git) : git_(git) {}

    std::string Id() const override {
        return "vanta.context.gitDiff";
    }

    std::vector<AgentContextItem> Collect(const AgentContextRequest&, WorkspaceContext& context) const override {
        const GitDiff diff = git_.Diff();
        if (diff.exit_code != 0 || diff.text.empty()) {
            return {};
        }
        return {{
            .provider_id = Id(),
            .kind = AgentContextItemKind::GitDiff,
            .title = "Git diff",
            .file = context.CurrentWorkspace().RootFile(),
            .text = diff.text,
            .payload = AgentPayload(Value::ObjectValue({
                {"exitCode", Value(static_cast<std::int64_t>(diff.exit_code))},
            })),
        }};
    }

private:
    const GitService& git_;
};

}

RegistrationHandle AgentContextCollector::RegisterProvider(std::unique_ptr<AgentContextProvider> provider) {
    if (provider == nullptr || provider->Id().empty()) {
        return {};
    }
    const std::string id = provider->Id();
    providers_[id] = std::move(provider);
    return RegistrationHandle([this, id] {
        RemoveProvider(id);
    });
}

void AgentContextCollector::RemoveProvider(const std::string& provider_id) {
    providers_.erase(provider_id);
}

void AgentContextCollector::ClearProviders() {
    providers_.clear();
}

std::vector<std::string> AgentContextCollector::ProviderIds() const {
    std::vector<std::string> ids;
    for (const auto& [id, provider] : providers_) {
        (void)provider;
        ids.push_back(id);
    }
    return ids;
}

AgentContext AgentContextCollector::Collect(const AgentContextRequest& request, WorkspaceContext& workspace) const {
    AgentContext context;
    for (const auto& [id, provider] : providers_) {
        (void)id;
        try {
            std::vector<AgentContextItem> items = provider->Collect(request, workspace);
            for (AgentContextItem& item : items) {
                if (context.items.size() >= request.max_items) {
                    return context;
                }
                context.items.push_back(std::move(item));
            }
        } catch (...) {
        }
    }
    return context;
}

void RegisterDefaultAgentContextProviders(AgentContextCollector& service) {
    service.RegisterProvider(std::make_unique<DocumentContextProvider>());
    service.RegisterProvider(std::make_unique<DiagnosticsContextProvider>());
    service.RegisterProvider(std::make_unique<ProjectModelContextProvider>());
    service.RegisterProvider(std::make_unique<JobContextProvider>());
    service.RegisterProvider(std::make_unique<SearchIndexContextProvider>());
}

std::unique_ptr<AgentContextProvider> CreateGitDiffAgentContextProvider(const GitService& git) {
    return std::make_unique<GitDiffContextProvider>(git);
}

std::string ToString(AgentContextItemKind kind) {
    switch (kind) {
    case AgentContextItemKind::Text:
        return "text";
    case AgentContextItemKind::Document:
        return "document";
    case AgentContextItemKind::Diagnostics:
        return "diagnostics";
    case AgentContextItemKind::Project:
        return "project";
    case AgentContextItemKind::Job:
        return "job";
    case AgentContextItemKind::SearchIndex:
        return "searchIndex";
    case AgentContextItemKind::GitDiff:
        return "gitDiff";
    }
    return "text";
}

}
