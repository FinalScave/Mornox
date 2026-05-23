#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "mornox/vfs/virtual_file.h"
#include "mornox/vfs/virtual_file_system.h"

namespace mornox {

struct WorkspaceInfo {
    std::filesystem::path root_path;
    std::string name;
};

class Workspace {
public:
    void BindFileSystem(const VirtualFileSystem& vfs);
    bool Open(const std::filesystem::path& root_path, std::string* error_message = nullptr);

    const WorkspaceInfo& Info() const;
    bool IsOpen() const;

    std::filesystem::path Resolve(const std::filesystem::path& path) const;
    VirtualFile File(const std::filesystem::path& path) const;
    VirtualFile RootFile() const;
    std::optional<std::string> ReadTextFile(const std::filesystem::path& path) const;
    bool WriteTextFile(const std::filesystem::path& path, const std::string& text, std::string* error_message = nullptr);

private:
    WorkspaceInfo info_;
    const VirtualFileSystem* vfs_ = nullptr;
    bool open_ = false;
};

}
