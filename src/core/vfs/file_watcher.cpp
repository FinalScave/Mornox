#include "mornox/vfs/file_watcher.h"

#include <array>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <unistd.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace mornox {
namespace {

#if defined(__APPLE__)

class FseventsFileWatcher final : public FileWatcher {
public:
    explicit FseventsFileWatcher(const VirtualFileSystem& vfs) : vfs_(vfs) {}

    ~FseventsFileWatcher() override {
        Stop();
    }

    bool Start(const VirtualFile& root, FileWatchCallback callback, std::string* error_message) override {
        auto local_path = root.LocalPath();
        if (!local_path) {
            if (error_message != nullptr) {
                *error_message = "File watcher requires a local root";
            }
            return false;
        }
        if (running_.load()) {
            return true;
        }

        callback_ = std::move(callback);
        root_path_ = *local_path;

        if (!CreateStream(error_message)) {
            return false;
        }
        running_ = true;
        if (!FSEventStreamStart(stream_)) {
            running_ = false;
            ReleaseStream();
            if (error_message != nullptr) {
                *error_message = "Failed to start FSEvents stream";
            }
            return false;
        }
        return true;
    }

    void Stop() override {
        if (!running_.load()) {
            return;
        }
        running_ = false;
        ReleaseStream();
    }

    bool Running() const override {
        return running_.load();
    }

private:
    static void WatchCallback(
        ConstFSEventStreamRef,
        void* client_callback_info,
        std::size_t num_events,
        void* event_paths,
        const FSEventStreamEventFlags event_flags[],
        const FSEventStreamEventId[]) {
        auto* watcher = static_cast<FseventsFileWatcher*>(client_callback_info);
        if (watcher == nullptr || !watcher->running_.load()) {
            return;
        }

        auto** paths = static_cast<char**>(event_paths);
        for (std::size_t i = 0; i < num_events; ++i) {
            watcher->Publish(paths[i], event_flags[i]);
        }
    }

    bool CreateStream(std::string* error_message) {
        const std::string root = root_path_.string();
        CFStringRef root_string = CFStringCreateWithCString(nullptr, root.c_str(), kCFStringEncodingUTF8);
        if (root_string == nullptr) {
            if (error_message != nullptr) {
                *error_message = "Failed to create file watcher path";
            }
            return false;
        }
        CFArrayRef paths = CFArrayCreate(nullptr, reinterpret_cast<const void**>(&root_string), 1, &kCFTypeArrayCallBacks);
        CFRelease(root_string);
        if (paths == nullptr) {
            if (error_message != nullptr) {
                *error_message = "Failed to create file watcher path list";
            }
            return false;
        }

        FSEventStreamContext context{};
        context.info = this;
        stream_ = FSEventStreamCreate(
            nullptr,
            &FseventsFileWatcher::WatchCallback,
            &context,
            paths,
            kFSEventStreamEventIdSinceNow,
            0.25,
            kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer);
        CFRelease(paths);

        if (stream_ == nullptr) {
            if (error_message != nullptr) {
                *error_message = "Failed to create FSEvents stream";
            }
            return false;
        }

        queue_ = dispatch_queue_create("dev.mornox.file-watcher", DISPATCH_QUEUE_SERIAL);
        FSEventStreamSetDispatchQueue(stream_, queue_);
        return true;
    }

    void ReleaseStream() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stream_ != nullptr) {
            FSEventStreamStop(stream_);
            FSEventStreamInvalidate(stream_);
            FSEventStreamRelease(stream_);
            stream_ = nullptr;
        }
        if (queue_ != nullptr) {
            dispatch_release(queue_);
            queue_ = nullptr;
        }
    }

    void Publish(const char* path, FSEventStreamEventFlags flags) const {
        if (callback_ == nullptr || path == nullptr) {
            return;
        }

        VirtualFileChangeKind kind = VirtualFileChangeKind::Modified;
        if ((flags & kFSEventStreamEventFlagItemCreated) != 0) {
            kind = VirtualFileChangeKind::Created;
        } else if ((flags & kFSEventStreamEventFlagItemRemoved) != 0) {
            kind = VirtualFileChangeKind::Deleted;
        }
        callback_({
            .file = vfs_.LocalFile(std::filesystem::path(path)),
            .kind = kind,
        });
    }

    const VirtualFileSystem& vfs_;
    FileWatchCallback callback_;
    std::filesystem::path root_path_;
    mutable std::mutex mutex_;
    FSEventStreamRef stream_ = nullptr;
    dispatch_queue_t queue_ = nullptr;
    std::atomic_bool running_ = false;
};

class DispatchDirectoryFileWatcher final : public FileWatcher {
public:
    explicit DispatchDirectoryFileWatcher(const VirtualFileSystem& vfs) : vfs_(vfs) {}

    ~DispatchDirectoryFileWatcher() override {
        Stop();
    }

    bool Start(const VirtualFile& root, FileWatchCallback callback, std::string* error_message) override {
        auto local_path = root.LocalPath();
        if (!local_path) {
            if (error_message != nullptr) {
                *error_message = "Directory watcher requires a local root";
            }
            return false;
        }
        if (running_.load()) {
            return true;
        }

        callback_ = std::move(callback);
        root_path_ = *local_path;
        queue_ = dispatch_queue_create("dev.mornox.directory-watcher", DISPATCH_QUEUE_SERIAL);
        running_ = true;
        RegisterTree(root_path_);

        if (sources_.empty()) {
            running_ = false;
            ReleaseSources();
            if (error_message != nullptr) {
                *error_message = "No directories could be watched";
            }
            return false;
        }
        return true;
    }

    void Stop() override {
        if (!running_.load() && sources_.empty()) {
            return;
        }
        running_ = false;
        ReleaseSources();
    }

    bool Running() const override {
        return running_.load();
    }

private:
    struct WatchContext {
        DispatchDirectoryFileWatcher* watcher = nullptr;
        dispatch_source_t source = nullptr;
        int fd = -1;
        std::filesystem::path path;
    };

    static void HandleEvent(void* value) {
        auto* context = static_cast<WatchContext*>(value);
        if (context == nullptr || context->watcher == nullptr || !context->watcher->running_.load()) {
            return;
        }

        const unsigned long flags = dispatch_source_get_data(context->source);
        VirtualFileChangeKind kind = VirtualFileChangeKind::Modified;
        if ((flags & DISPATCH_VNODE_DELETE) != 0 || (flags & DISPATCH_VNODE_RENAME) != 0) {
            kind = VirtualFileChangeKind::Deleted;
        }
        context->watcher->Publish(context->path, kind);
    }

    static void HandleCancel(void* value) {
        auto* context = static_cast<WatchContext*>(value);
        if (context == nullptr) {
            return;
        }
        if (context->fd >= 0) {
            close(context->fd);
            context->fd = -1;
        }
        delete context;
    }

    void RegisterTree(const std::filesystem::path& root) {
        RegisterDirectory(root);
        std::error_code error;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, error)) {
            if (error) {
                break;
            }
            std::error_code status_error;
            if (entry.is_directory(status_error) && !status_error) {
                RegisterDirectory(entry.path());
            }
        }
    }

    void RegisterDirectory(const std::filesystem::path& path) {
        const int fd = open(path.c_str(), O_EVTONLY);
        if (fd < 0) {
            return;
        }

        const unsigned long mask = DISPATCH_VNODE_WRITE | DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME |
                                   DISPATCH_VNODE_EXTEND | DISPATCH_VNODE_ATTRIB | DISPATCH_VNODE_LINK;
        dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, static_cast<uintptr_t>(fd), mask, queue_);
        if (source == nullptr) {
            close(fd);
            return;
        }

        auto* context = new WatchContext{this, source, fd, path};
        dispatch_set_context(source, context);
        dispatch_source_set_event_handler_f(source, &DispatchDirectoryFileWatcher::HandleEvent);
        dispatch_source_set_cancel_handler_f(source, &DispatchDirectoryFileWatcher::HandleCancel);
        sources_.push_back(source);
        dispatch_resume(source);
    }

    void ReleaseSources() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (dispatch_source_t source : sources_) {
            dispatch_source_cancel(source);
            dispatch_release(source);
        }
        sources_.clear();
        if (queue_ != nullptr) {
            dispatch_release(queue_);
            queue_ = nullptr;
        }
    }

    void Publish(const std::filesystem::path& path, VirtualFileChangeKind kind) const {
        if (callback_ == nullptr) {
            return;
        }
        callback_({
            .file = vfs_.LocalFile(path),
            .kind = kind,
        });
    }

    const VirtualFileSystem& vfs_;
    FileWatchCallback callback_;
    std::filesystem::path root_path_;
    mutable std::mutex mutex_;
    dispatch_queue_t queue_ = nullptr;
    std::vector<dispatch_source_t> sources_;
    std::atomic_bool running_ = false;
};

class MacFileWatcher final : public FileWatcher {
public:
    explicit MacFileWatcher(const VirtualFileSystem& vfs)
        : fsevents_(vfs), directories_(vfs) {}

    bool Start(const VirtualFile& root, FileWatchCallback callback, std::string* error_message) override {
        std::string fsevents_error;
        FileWatchCallback fallback_callback = callback;
        if (fsevents_.Start(root, std::move(callback), &fsevents_error)) {
            active_ = &fsevents_;
            return true;
        }

        std::string directory_error;
        if (directories_.Start(root, std::move(fallback_callback), &directory_error)) {
            active_ = &directories_;
            return true;
        }

        if (error_message != nullptr) {
            *error_message = fsevents_error.empty() ? directory_error : fsevents_error + "; " + directory_error;
        }
        return false;
    }

    void Stop() override {
        if (active_ != nullptr) {
            active_->Stop();
            active_ = nullptr;
        }
    }

    bool Running() const override {
        return active_ != nullptr && active_->Running();
    }

private:
    FseventsFileWatcher fsevents_;
    DispatchDirectoryFileWatcher directories_;
    FileWatcher* active_ = nullptr;
};

#elif defined(_WIN32)

class WindowsFileWatcher final : public FileWatcher {
public:
    explicit WindowsFileWatcher(const VirtualFileSystem& vfs) : vfs_(vfs) {}

    ~WindowsFileWatcher() override {
        Stop();
    }

    bool Start(const VirtualFile& root, FileWatchCallback callback, std::string* error_message) override {
        auto local_path = root.LocalPath();
        if (!local_path) {
            if (error_message != nullptr) {
                *error_message = "File watcher requires a local root";
            }
            return false;
        }
        if (running_.load()) {
            return true;
        }

        callback_ = std::move(callback);
        root_path_ = *local_path;
        const std::wstring root_path = root_path_.wstring();
        HANDLE directory = CreateFileW(
            root_path.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr);
        if (directory == INVALID_HANDLE_VALUE) {
            if (error_message != nullptr) {
                *error_message = "Failed to open directory for file watching";
            }
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            directory_ = directory;
        }
        running_ = true;
        worker_ = std::thread([this] {
            WatchLoop();
        });
        return true;
    }

    void Stop() override {
        if (!running_.exchange(false) && !worker_.joinable()) {
            return;
        }

        HANDLE directory = DirectoryHandle();
        if (directory != INVALID_HANDLE_VALUE) {
            CancelIoEx(directory, nullptr);
        }
        if (worker_.joinable()) {
            worker_.join();
        }
        CloseDirectory();
    }

    bool Running() const override {
        return running_.load();
    }

private:
    static constexpr DWORD kBufferSize = 64 * 1024;

    HANDLE DirectoryHandle() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return directory_;
    }

    void CloseDirectory() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (directory_ != INVALID_HANDLE_VALUE) {
            CloseHandle(directory_);
            directory_ = INVALID_HANDLE_VALUE;
        }
    }

    void WatchLoop() {
        HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (event == nullptr) {
            running_ = false;
            return;
        }

        while (running_.load()) {
            std::array<unsigned char, kBufferSize> buffer{};
            OVERLAPPED overlapped{};
            overlapped.hEvent = event;
            ResetEvent(event);

            HANDLE directory = DirectoryHandle();
            if (directory == INVALID_HANDLE_VALUE) {
                break;
            }

            const DWORD filter =
                FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_SIZE |
                FILE_NOTIFY_CHANGE_LAST_WRITE |
                FILE_NOTIFY_CHANGE_CREATION;
            const BOOL started = ReadDirectoryChangesW(
                directory,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                TRUE,
                filter,
                nullptr,
                &overlapped,
                nullptr);
            if (!started) {
                break;
            }

            const DWORD wait_result = WaitForSingleObject(event, INFINITE);
            if (wait_result != WAIT_OBJECT_0) {
                CancelIoEx(directory, &overlapped);
                break;
            }

            DWORD bytes_returned = 0;
            if (!GetOverlappedResult(directory, &overlapped, &bytes_returned, FALSE)) {
                if (!running_.load() || GetLastError() == ERROR_OPERATION_ABORTED) {
                    break;
                }
                continue;
            }
            if (bytes_returned > 0) {
                PublishBuffer(buffer, bytes_returned);
            }
        }

        CloseHandle(event);
        running_ = false;
    }

    void PublishBuffer(const std::array<unsigned char, kBufferSize>& buffer, DWORD bytes_returned) const {
        const unsigned char* current = buffer.data();
        const unsigned char* end = buffer.data() + bytes_returned;
        while (current < end) {
            const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(current);
            const std::wstring relative_path(info->FileName, info->FileNameLength / sizeof(wchar_t));
            Publish(root_path_ / std::filesystem::path(relative_path), KindForAction(info->Action));
            if (info->NextEntryOffset == 0) {
                break;
            }
            current += info->NextEntryOffset;
        }
    }

    static VirtualFileChangeKind KindForAction(DWORD action) {
        switch (action) {
        case FILE_ACTION_ADDED:
        case FILE_ACTION_RENAMED_NEW_NAME:
            return VirtualFileChangeKind::Created;
        case FILE_ACTION_REMOVED:
        case FILE_ACTION_RENAMED_OLD_NAME:
            return VirtualFileChangeKind::Deleted;
        default:
            return VirtualFileChangeKind::Modified;
        }
    }

    void Publish(const std::filesystem::path& path, VirtualFileChangeKind kind) const {
        if (callback_ == nullptr) {
            return;
        }
        callback_({
            .file = vfs_.LocalFile(path),
            .kind = kind,
        });
    }

    const VirtualFileSystem& vfs_;
    FileWatchCallback callback_;
    std::filesystem::path root_path_;
    mutable std::mutex mutex_;
    HANDLE directory_ = INVALID_HANDLE_VALUE;
    std::thread worker_;
    std::atomic_bool running_ = false;
};

#else

class UnsupportedFileWatcher final : public FileWatcher {
public:
    bool Start(const VirtualFile&, FileWatchCallback, std::string* error_message) override {
        if (error_message != nullptr) {
            *error_message = "Platform file watcher is not implemented";
        }
        return false;
    }

    void Stop() override {}
    bool Running() const override { return false; }
};

#endif

}

std::unique_ptr<FileWatcher> CreatePlatformFileWatcher(const VirtualFileSystem& vfs) {
#if defined(__APPLE__)
    return std::make_unique<MacFileWatcher>(vfs);
#elif defined(_WIN32)
    return std::make_unique<WindowsFileWatcher>(vfs);
#else
    (void)vfs;
    return std::make_unique<UnsupportedFileWatcher>();
#endif
}

std::string ToString(VirtualFileChangeKind kind) {
    switch (kind) {
    case VirtualFileChangeKind::Created:
        return "created";
    case VirtualFileChangeKind::Modified:
        return "modified";
    case VirtualFileChangeKind::Deleted:
        return "deleted";
    }
    return "modified";
}

}
