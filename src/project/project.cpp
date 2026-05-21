#include "vanta/project/project.h"

namespace vanta {

const ProjectModel& Project::Model() const {
    return model_;
}

void Project::SetModel(ProjectModel model) {
    model_ = std::move(model);
}

ComponentRegistry& Project::Components() {
    return components_;
}

const ComponentRegistry& Project::Components() const {
    return components_;
}

Component* Project::GetComponent(const std::string& id) {
    return components_.Get(id);
}

const Component* Project::GetComponent(const std::string& id) const {
    return components_.Get(id);
}

}
