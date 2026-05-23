#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "mornox/vfs/uri.h"

namespace mornox {

class VirtualFileSystem;

enum class VirtualFileKind {
    Unknown,
    File,
    Directory,
};

struct FileStat {
    VirtualFileKind kind = VirtualFileKind::Unknown;
    std::uint64_t size = 0;
};

class VirtualFile {
public:
    VirtualFile() = default;
    VirtualFile(Uri uri, const VirtualFileSystem* vfs);

    bool Valid() const noexcept;
    const Uri& UriValue() const noexcept;
    Uri ToUri() const;
    std::string DisplayName() const;
    std::string Extension() const;

    bool Exists() const;
    FileStat Stat() const;
    std::optional<VirtualFile> Parent() const;
    std::vector<VirtualFile> ListChildren() const;
    std::optional<std::string> ReadText() const;
    bool WriteText(const std::string& text, std::string* error_message = nullptr) const;
    std::optional<std::filesystem::path> LocalPath() const;

    bool operator==(const VirtualFile& other) const noexcept;
    bool operator!=(const VirtualFile& other) const noexcept;
    bool operator<(const VirtualFile& other) const noexcept;

private:
    Uri uri_;
    const VirtualFileSystem* vfs_ = nullptr;
};

}
