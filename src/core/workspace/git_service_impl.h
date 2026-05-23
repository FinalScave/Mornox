#pragma once

#include <filesystem>

#include "mornox/workspace/git_service.h"

namespace mornox::internal {

class GitServiceImpl final : public GitService {
public:
    void SetWorkspaceRoot(std::filesystem::path workspace_root);

    GitDiff Diff() const override;
    GitDiff Diff(const std::filesystem::path& workspace_root) const;

private:
    std::filesystem::path workspace_root_;
};

}
