#pragma once

#include <functional>
#include <memory>
#include <string>

#include "mornox/vfs/virtual_file.h"
#include "mornox/vfs/virtual_file_system.h"

namespace mornox {

enum class VirtualFileChangeKind {
    Created,
    Modified,
    Deleted,
};

struct VirtualFileChangeEvent {
    VirtualFile file;
    VirtualFileChangeKind kind = VirtualFileChangeKind::Modified;
};

using FileWatchCallback = std::function<void(const VirtualFileChangeEvent&)>;

class FileWatcher {
public:
    virtual ~FileWatcher() = default;

    virtual bool Start(const VirtualFile& root, FileWatchCallback callback, std::string* error_message = nullptr) = 0;
    virtual void Stop() = 0;
    virtual bool Running() const = 0;
};

std::unique_ptr<FileWatcher> CreatePlatformFileWatcher(const VirtualFileSystem& vfs);
std::string ToString(VirtualFileChangeKind kind);

}
