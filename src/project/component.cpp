#include "vanta/project/component.h"

#include <algorithm>
#include <utility>

#include "vanta/workspace/workspace_context.h"
#include "vanta/workspace/workspace_runtime.h"
#include "vanta/project/project.h"

namespace vanta {
namespace {

bool ContainsString(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

}

bool ComponentMatch::Matches(const ProjectModel& model) const {
    if (all_projects) {
        return true;
    }
    if (!project_types.empty()) {
        const std::string primary_type = PrimaryProjectType(model);
        if (ContainsString(project_types, primary_type) || ContainsString(project_types, ToString(model.origin))) {
            return true;
        }
    }
    for (const std::string& facet : facets) {
        if (model.HasFacet(facet)) {
            return true;
        }
    }
    return false;
}

void ComponentContributionRegistry::Add(ComponentContribution contribution) {
    if (contribution.id.empty() || !contribution.factory) {
        return;
    }
    contributions_[contribution.id] = std::move(contribution);
}

bool ComponentContributionRegistry::Remove(const std::string& id) {
    return contributions_.erase(id) > 0;
}

std::optional<ComponentContribution> ComponentContributionRegistry::Contribution(const std::string& id) const {
    auto it = contributions_.find(id);
    return it == contributions_.end() ? std::nullopt : std::optional<ComponentContribution>(it->second);
}

std::vector<ComponentContribution> ComponentContributionRegistry::List() const {
    std::vector<ComponentContribution> values;
    for (const auto& [id, contribution] : contributions_) {
        (void)id;
        values.push_back(contribution);
    }
    return values;
}

std::vector<ComponentContribution> ComponentContributionRegistry::Matching(const ProjectModel& model) const {
    std::vector<ComponentContribution> values;
    for (const auto& [id, contribution] : contributions_) {
        (void)id;
        if (contribution.match.Matches(model)) {
            values.push_back(contribution);
        }
    }
    return values;
}

void Component::OnAttach(WorkspaceContext& context) {
    (void)context;
}

void Component::RestoreState(const Value& state) {
    (void)state;
}

void Component::OnOpenProject(Project& project) {
    (void)project;
}

void Component::OnProjectChanged(Project& project) {
    (void)project;
}

Value Component::SaveState() const {
    return Value::ObjectValue();
}

void Component::OnCloseProject(Project& project) {
    (void)project;
}

void Component::OnDetach() {}

void ComponentRegistry::Bind(std::unique_ptr<Component> component) {
    if (component == nullptr || component->Id().empty()) {
        return;
    }
    const std::string id = component->Id();
    Unbind(id);
    auto [it, inserted] = components_.emplace(id, Entry{.component = std::move(component)});
    (void)inserted;
    if (context_ != nullptr && !AttachEntry(id, it->second)) {
        return;
    }
    if (has_restored_state_) {
        RestoreEntry(id, it->second, restored_state_);
    }
    if (project_open_ && open_project_ != nullptr) {
        OpenEntry(it->second, *open_project_);
    }
}

bool ComponentRegistry::Unbind(const std::string& id) {
    auto it = components_.find(id);
    if (it == components_.end()) {
        return false;
    }
    if (it->second.project_opened && open_project_ != nullptr) {
        CloseEntry(it->second, *open_project_);
    }
    DetachEntry(id, it->second);
    components_.erase(it);
    return true;
}

Component* ComponentRegistry::Get(const std::string& id) {
    auto it = components_.find(id);
    return it == components_.end() ? nullptr : it->second.component.get();
}

const Component* ComponentRegistry::Get(const std::string& id) const {
    auto it = components_.find(id);
    return it == components_.end() ? nullptr : it->second.component.get();
}

std::vector<std::string> ComponentRegistry::Ids() const {
    std::vector<std::string> result;
    for (const auto& [id, entry] : components_) {
        (void)entry;
        result.push_back(id);
    }
    return result;
}

void ComponentRegistry::RememberState(const ProjectState& state) {
    restored_state_ = state;
    has_restored_state_ = true;
}

void ComponentRegistry::AttachAll(WorkspaceContext& context) {
    context_ = &context;
    for (auto& [id, entry] : components_) {
        if (!entry.attached) {
            AttachEntry(id, entry);
        }
    }
}

void ComponentRegistry::RestoreAll(const ProjectState& state) {
    RememberState(state);
    for (auto& [id, entry] : components_) {
        if (!entry.restored) {
            RestoreEntry(id, entry, restored_state_);
        }
    }
}

void ComponentRegistry::OpenProject(Project& project) {
    if (project_open_) {
        ProjectChanged(project);
        return;
    }
    project_open_ = true;
    open_project_ = &project;
    for (auto& [id, entry] : components_) {
        (void)id;
        OpenEntry(entry, project);
    }
}

void ComponentRegistry::ProjectChanged(Project& project) {
    for (auto& [id, entry] : components_) {
        (void)id;
        ProjectChangedEntry(entry, project);
    }
}

ProjectState ComponentRegistry::SaveAll(const ProjectState& previous) const {
    ProjectState state = previous;
    state.schema_version = 1;
    for (const auto& [id, entry] : components_) {
        try {
            state.component_states[id] = entry.component->SaveState();
        } catch (...) {
        }
    }
    return state;
}

void ComponentRegistry::CloseProject(Project& project) {
    if (!project_open_) {
        return;
    }
    for (auto& [id, entry] : components_) {
        (void)id;
        CloseEntry(entry, project);
    }
    project_open_ = false;
    open_project_ = nullptr;
}

void ComponentRegistry::DetachAll() {
    for (auto& [id, entry] : components_) {
        DetachEntry(id, entry);
    }
    context_ = nullptr;
}

bool ComponentRegistry::AttachEntry(const std::string& id, Entry& entry) {
    if (entry.component == nullptr || entry.attached || context_ == nullptr) {
        return entry.attached;
    }
    try {
        entry.component->OnAttach(*context_);
        entry.attached = true;
    } catch (...) {
        if (context_ != nullptr) {
            context_->RemoveEventSubscriptions(id);
        }
    }
    return entry.attached;
}

void ComponentRegistry::RestoreEntry(const std::string& id, Entry& entry, const ProjectState& state) {
    if (entry.component == nullptr || entry.restored || !entry.attached) {
        return;
    }
    entry.restored = true;
    auto found = state.component_states.find(id);
    if (found == state.component_states.end()) {
        return;
    }
    try {
        entry.component->RestoreState(found->second);
    } catch (...) {
    }
}

void ComponentRegistry::OpenEntry(Entry& entry, Project& project) {
    if (entry.component == nullptr || entry.project_opened || !entry.attached) {
        return;
    }
    try {
        entry.component->OnOpenProject(project);
        entry.project_opened = true;
    } catch (...) {
    }
}

void ComponentRegistry::ProjectChangedEntry(Entry& entry, Project& project) {
    if (entry.component == nullptr || !entry.project_opened) {
        return;
    }
    try {
        entry.component->OnProjectChanged(project);
    } catch (...) {
    }
}

void ComponentRegistry::CloseEntry(Entry& entry, Project& project) {
    if (entry.component == nullptr || !entry.project_opened) {
        return;
    }
    try {
        entry.component->OnCloseProject(project);
    } catch (...) {
    }
    entry.project_opened = false;
}

void ComponentRegistry::DetachEntry(const std::string& id, Entry& entry) {
    if (entry.component != nullptr && entry.attached) {
        try {
            entry.component->OnDetach();
        } catch (...) {
        }
        entry.attached = false;
    }
    if (context_ != nullptr) {
        context_->RemoveEventSubscriptions(id);
    }
}

}
