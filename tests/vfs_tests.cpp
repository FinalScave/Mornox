#include "test_support.h"

#include <atomic>

#include "mornox/vfs/file_watcher.h"

namespace mornox::tests {

void TestVirtualFileSystem() {
    const auto root = MakeTempRoot();
    mornox::VirtualFileSystem vfs;
    const mornox::VirtualFile file = vfs.LocalFile(root / "src" / "main.cpp");
    mornox::VirtualFile copied = file;
    REQUIRE(copied.ToUri() == file.ToUri());
    const auto protected_provider = vfs.RegisterProvider("file", std::make_unique<mornox::LocalFileSystemProvider>());
    REQUIRE(!protected_provider.Registered());
    vfs.RemoveProvider("file");

    std::string error;
    REQUIRE(file.WriteText("int main() { return 0; }\n", &error));
    REQUIRE(file.Exists());
    REQUIRE(file.ReadText()->find("return 0") != std::string::npos);
    REQUIRE(file.LocalPath().has_value());

    const auto parent = file.Parent();
    REQUIRE(parent.has_value());
    REQUIRE(parent->Stat().kind == mornox::VirtualFileKind::Directory);
    REQUIRE(!parent->ListChildren().empty());
}

#if defined(_WIN32)
void TestWindowsFileWatcher() {
    const auto root = MakeTempRoot();
    mornox::VirtualFileSystem vfs;
    auto watcher = mornox::CreatePlatformFileWatcher(vfs);
    std::atomic_bool observed = false;
    std::string error;

    REQUIRE(watcher->Start(vfs.LocalFile(root), [&](const mornox::VirtualFileChangeEvent& event) {
        if (event.file.DisplayName() == "watched.txt") {
            observed = true;
        }
    }, &error));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    WriteFile(root / "watched.txt", "hello\n");
    for (int attempt = 0; attempt < 50 && !observed.load(); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    watcher->Stop();
    REQUIRE(observed.load());
}
#endif

}

TEST_CASE("Virtual file system", "[vfs]") {
    mornox::tests::TestVirtualFileSystem();
}

#if defined(_WIN32)
TEST_CASE("Windows file watcher", "[vfs][watcher]") {
    mornox::tests::TestWindowsFileWatcher();
}
#endif
