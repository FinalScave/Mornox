#pragma once

#include <string>

#include "vanta/project/component.h"
#include "vanta/project/project_manager.h"

namespace vanta {

class Project {
public:
    const ProjectModel& Model() const;
    void SetModel(ProjectModel model);

    ComponentRegistry& Components();
    const ComponentRegistry& Components() const;
    Component* GetComponent(const std::string& id);
    const Component* GetComponent(const std::string& id) const;

    template <class T>
    T* GetComponent(const std::string& id) {
        return components_.Get<T>(id);
    }

    template <class T>
    const T* GetComponent(const std::string& id) const {
        return components_.Get<T>(id);
    }

private:
    ProjectModel model_;
    ComponentRegistry components_;
};

}
