#pragma once

#include <filesystem>
#include <memory>

#include "vanta/language/language_service.h"

namespace vanta {

class CliceIntegration {
public:
    bool Configure(std::filesystem::path clice_path, std::filesystem::path workspace_root);
    std::unique_ptr<LanguageService> CreateLanguageService() const;
    const std::filesystem::path& ClicePath() const;
    const std::filesystem::path& WorkspaceRoot() const;

private:
    std::filesystem::path clice_path_;
    std::filesystem::path workspace_root_;
};

}
