#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "vanta/core/value.h"
#include "vanta/workspace/ide_event.h"

namespace vanta {

class AgentToolRegistry;
class ChangeSetService;
class CommandRegistry;
class Component;
class DiagnosticService;
class DocumentService;
class Project;
class ExecutionService;
class Workspace;
class WorkspaceContext;
struct ProjectModel;

struct ComponentMatch {
    bool all_projects = false;
    std::vector<std::string> project_types;
    std::vector<std::string> facets;

    bool Matches(const ProjectModel& model) const;
};

struct ComponentContribution {
    std::string id;
    std::string title;
    std::string plugin_id;
    ComponentMatch match;
    std::function<std::unique_ptr<Component>()> factory;
};

class ComponentContributionRegistry {
public:
    void Add(ComponentContribution contribution);
    bool Remove(const std::string& id);
    std::optional<ComponentContribution> Contribution(const std::string& id) const;
    std::vector<ComponentContribution> List() const;
    std::vector<ComponentContribution> Matching(const ProjectModel& model) const;

private:
    std::map<std::string, ComponentContribution> contributions_;
};

struct ProjectState {
    int schema_version = 1;
    std::map<std::string, Value> component_states;
};

class Component {
public:
    virtual ~Component() = default;

    virtual std::string Id() const = 0;
    virtual void OnAttach(WorkspaceContext& context);
    virtual void RestoreState(const Value& state);
    virtual void OnOpenProject(Project& project);
    virtual void OnProjectChanged(Project& project);
    virtual Value SaveState() const;
    virtual void OnCloseProject(Project& project);
    virtual void OnDetach();
};

class ComponentRegistry {
public:
    void Bind(std::unique_ptr<Component> component);
    bool Unbind(const std::string& id);
    Component* Get(const std::string& id);
    const Component* Get(const std::string& id) const;

    template <class T>
    T* Get(const std::string& id) {
        return dynamic_cast<T*>(Get(id));
    }

    template <class T>
    const T* Get(const std::string& id) const {
        return dynamic_cast<const T*>(Get(id));
    }

    std::vector<std::string> Ids() const;
    void RememberState(const ProjectState& state);
    void AttachAll(WorkspaceContext& context);
    void RestoreAll(const ProjectState& state);
    void OpenProject(Project& project);
    void ProjectChanged(Project& project);
    ProjectState SaveAll(const ProjectState& previous = {}) const;
    void CloseProject(Project& project);
    void DetachAll();

private:
    struct Entry {
        std::unique_ptr<Component> component;
        bool attached = false;
        bool restored = false;
        bool project_opened = false;
    };

    bool AttachEntry(const std::string& id, Entry& entry);
    void RestoreEntry(const std::string& id, Entry& entry, const ProjectState& state);
    void OpenEntry(Entry& entry, Project& project);
    void ProjectChangedEntry(Entry& entry, Project& project);
    void CloseEntry(Entry& entry, Project& project);
    void DetachEntry(const std::string& id, Entry& entry);

    std::map<std::string, Entry> components_;
    WorkspaceContext* context_ = nullptr;
    ProjectState restored_state_;
    bool has_restored_state_ = false;
    Project* open_project_ = nullptr;
    bool project_open_ = false;
};

}
