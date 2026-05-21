#pragma once

#include <any>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "vanta/core/event.h"
#include "vanta/core/registration.h"
#include "vanta/core/value.h"
#include "vanta/workspace/workspace.h"
#include "vanta/vfs/virtual_file.h"

namespace vanta {

class WorkspaceContext;
class ProjectModelBuilder;

enum class ProjectOrigin {
    kWorkspace,
    kSingleFile,
    kScratch,
};

struct ProjectFacet {
    std::string id;
    std::string type;
    std::string title;
};

struct ProjectModule {
    std::string id;
    std::string name;
    VirtualFile content_root;
    std::vector<VirtualFile> source_roots;
    std::vector<VirtualFile> excluded_roots;
    std::vector<ProjectFacet> facets;
};

struct ProjectAttachmentInfo {
    std::string id;
    std::string kind;
    std::string title;
    Value summary;
    Value projection;
};

struct ProjectModel {
    ProjectOrigin origin = ProjectOrigin::kWorkspace;
    VirtualFile root;
    std::vector<ProjectModule> modules;
    std::vector<ProjectFacet> facets;
    std::vector<ProjectAttachmentInfo> attachment_infos;
    std::map<std::string, std::any> attachments;

    bool HasFacet(const std::string& type) const;
    bool HasAttachment(const std::string& id) const;
    std::optional<ProjectAttachmentInfo> AttachmentInfo(const std::string& id) const;

    template <class T>
    void SetAttachment(ProjectAttachmentInfo info, T attachment) {
        if (info.id.empty()) {
            return;
        }
        if (info.summary.IsNull()) {
            info.summary = Value::ObjectValue();
        }
        if (info.projection.IsNull()) {
            info.projection = info.summary;
        }
        attachments[info.id] = std::move(attachment);
        for (ProjectAttachmentInfo& existing : attachment_infos) {
            if (existing.id == info.id) {
                existing = std::move(info);
                return;
            }
        }
        attachment_infos.push_back(std::move(info));
    }

    template <class T>
    const T* Attachment(const std::string& id) const {
        auto found = attachments.find(id);
        if (found == attachments.end()) {
            return nullptr;
        }
        return std::any_cast<T>(&found->second);
    }
};

struct SingleFileModel {
    static constexpr const char* kAttachmentId = "vanta.singleFile";
    static constexpr const char* kAttachmentKind = "singleFile";

    VirtualFile file;
    std::string language_id;
    std::filesystem::path working_directory;
};

class ProjectModelBuilder {
public:
    ProjectModelBuilder(ProjectOrigin origin, VirtualFile root);

    ProjectOrigin Origin() const;
    const VirtualFile& Root() const;
    bool Empty() const;

    void AddModule(ProjectModule module);
    void AddFacet(ProjectFacet facet);
    void AddFacetToPrimaryModule(ProjectFacet facet);

    template <class T>
    void SetAttachment(ProjectAttachmentInfo info, T attachment) {
        model_.SetAttachment(std::move(info), std::move(attachment));
    }

    const ProjectModel& Preview() const;
    ProjectModel Build();

private:
    ProjectModel model_;
};

class ProjectModelProvider {
public:
    virtual ~ProjectModelProvider() = default;

    virtual std::string Id() const = 0;
    virtual void Contribute(WorkspaceContext& context, ProjectModelBuilder& builder) const = 0;
};

struct ProjectView {
    std::string id;
    std::string title;
    std::string icon;
    int priority = 0;
};

namespace ProjectViewNodeKind {
inline constexpr std::string_view kGroup = "vanta.group";
inline constexpr std::string_view kFile = "vanta.file";
inline constexpr std::string_view kDirectory = "vanta.directory";
inline constexpr std::string_view kModule = "vanta.module";
}

struct ProjectViewNode {
    std::string id;
    std::string label;
    std::string description;
    std::string kind;
    std::string icon;
    VirtualFile file;
    bool has_file = false;
    bool has_children = false;
    bool synthetic = false;
};

struct ProjectViewChangeEvent {
    std::string provider_id;
    std::string view_id;
    std::string node_id;
};

class ProjectViewProvider {
public:
    virtual ~ProjectViewProvider() = default;

    virtual std::string Id() const = 0;
    virtual std::vector<ProjectView> Views(WorkspaceContext& context) const = 0;
    virtual std::vector<ProjectViewNode> TopLevelNodes(WorkspaceContext& context, const ProjectView& view) = 0;
    virtual std::vector<ProjectViewNode> Children(WorkspaceContext& context, const ProjectView& view, const ProjectViewNode& parent) = 0;
};

class ProjectManager {
public:
    ProjectManager();

    RegistrationHandle RegisterModelProvider(std::unique_ptr<ProjectModelProvider> provider);
    void RemoveModelProvider(const std::string& provider_id);
    std::vector<std::string> ModelProviderIds() const;
    RegistrationHandle RegisterViewProvider(std::unique_ptr<ProjectViewProvider> provider);
    void RemoveViewProvider(const std::string& provider_id);
    std::vector<std::string> ViewProviderIds() const;
    void SetSingleFile(VirtualFile file, std::string language_id = {});
    void ClearSingleFile();

    const ProjectModel& Refresh(WorkspaceContext& context);
    const ProjectModel& Current() const;
    bool HasProject() const;
    std::vector<ProjectView> Views(WorkspaceContext& context) const;
    std::vector<ProjectViewNode> TopLevelNodes(WorkspaceContext& context, const std::string& view_id);
    std::vector<ProjectViewNode> Children(WorkspaceContext& context, const std::string& view_id, const ProjectViewNode& parent);
    std::uint64_t OnDidChangeViews(EventBus<ProjectViewChangeEvent>::Listener listener);
    void RemoveViewListener(std::uint64_t listener_id);
    void InvalidateViews(ProjectViewChangeEvent event = {});

private:
    ProjectModel model_;
    std::map<std::string, std::unique_ptr<ProjectModelProvider>> model_providers_;
    std::map<std::string, std::unique_ptr<ProjectViewProvider>> view_providers_;
    EventBus<ProjectViewChangeEvent> view_events_;
    std::optional<SingleFileModel> single_file_;
};

std::string ToString(ProjectOrigin origin);
std::string PrimaryProjectType(const ProjectModel& model);

}
