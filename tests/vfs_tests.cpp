#include "test_support.h"

namespace vanta::tests {

void TestVirtualFileSystem() {
    const auto root = MakeTempRoot();
    vanta::VirtualFileSystem vfs;
    const vanta::VirtualFile file = vfs.LocalFile(root / "src" / "main.cpp");
    vanta::VirtualFile copied = file;
    REQUIRE(copied.ToUri() == file.ToUri());
    const auto protected_provider = vfs.RegisterProvider("file", std::make_unique<vanta::LocalFileSystemProvider>());
    REQUIRE(!protected_provider.Registered());
    vfs.RemoveProvider("file");

    std::string error;
    REQUIRE(file.WriteText("int main() { return 0; }\n", &error));
    REQUIRE(file.Exists());
    REQUIRE(file.ReadText()->find("return 0") != std::string::npos);
    REQUIRE(file.LocalPath().has_value());

    const auto parent = file.Parent();
    REQUIRE(parent.has_value());
    REQUIRE(parent->Stat().kind == vanta::VirtualFileKind::Directory);
    REQUIRE(!parent->ListChildren().empty());
}

}

TEST_CASE("Virtual file system", "[vfs]") {
    vanta::tests::TestVirtualFileSystem();
}
