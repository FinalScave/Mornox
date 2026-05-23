#include "clice_integration.h"

#include <utility>

#include "mornox/language/lsp_language_service.h"

namespace mornox {

bool CliceIntegration::Configure(std::filesystem::path clice_path, std::filesystem::path workspace_root) {
    clice_path_ = std::move(clice_path);
    workspace_root_ = std::move(workspace_root);
    return !clice_path_.empty() && std::filesystem::exists(clice_path_);
}

std::unique_ptr<LanguageService> CliceIntegration::CreateLanguageService() const {
    return std::make_unique<LspLanguageService>(clice_path_, workspace_root_, "cpp");
}

const std::filesystem::path& CliceIntegration::ClicePath() const {
    return clice_path_;
}

const std::filesystem::path& CliceIntegration::WorkspaceRoot() const {
    return workspace_root_;
}

}
