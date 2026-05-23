#pragma once

#include "mornox/execution/build_service.h"

namespace mornox {

class CMakeBuildProvider final : public BuildProvider {
public:
    std::string Id() const override;
    BuildEnvironment Detect(WorkspaceContext& context, const ProjectModel& project) const override;
    BuildPlan Plan(WorkspaceContext& context, const BuildRequest& request) const override;
};

}
