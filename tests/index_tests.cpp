#include "test_support.h"

namespace vanta::tests {

void TestIndexServiceSearch() {
    const auto root = MakeTempRoot();
    WriteFile(root / "src" / "main.cpp", "int main() {\n  return 0;\n}\n");
    WriteFile(root / "include" / "main.hpp", "int answer();\n");

    vanta::VirtualFileSystem vfs;
    vanta::AsyncRuntime async_runtime(1);
    vanta::WorkspaceRuntime session(vfs, async_runtime);
    std::string error;
    REQUIRE(session.Open(root, &error));

    const auto snapshot = session.Context().Indexes().Snapshot("vanta.index.search");
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->item_count > 0);
    const auto file_result = session.Context().Indexes().Query(session.Context(), {
        .kind = vanta::IndexQueryKind::Files,
        .query = "main",
    });
    REQUIRE(file_result.ok);
    REQUIRE(!file_result.hits.empty());
    const auto text_result = session.Context().Indexes().Query(session.Context(), {
        .kind = vanta::IndexQueryKind::Text,
        .query = "return 0",
    });
    REQUIRE(text_result.ok);
    REQUIRE(text_result.hits.size() == 1);
    REQUIRE(text_result.hits[0].file.ToUri() == session.Context().CurrentWorkspace().File("src/main.cpp").ToUri());
    session.Close();
}

void TestCppCompilationDatabaseIndexProvider() {
    const auto root = MakeTempRoot();
    WriteFile(root / "src" / "main.cpp", "int main() { return 0; }\n");
    std::filesystem::create_directories(root / "include");
    WriteFile(root / "build" / "compile_commands.json", std::string(R"([
      {
        "directory": ")" + root.string() + R"(",
        "file": ")" + (root / "src" / "main.cpp").string() + R"(",
        "arguments": ["c++", "-Iinclude", "-DVANTA_TEST=1", "-c", "src/main.cpp"]
      }
    ])"));

    vanta::VirtualFileSystem vfs;
    vanta::AsyncRuntime async_runtime(1);
    vanta::WorkspaceRuntime session(vfs, async_runtime);
    std::string error;
    REQUIRE(session.Open(root, &error));
    session.Context().Indexes().RegisterProvider(vanta::CreateCppCompilationDatabaseIndexProvider());
    const vanta::JobId job_id = session.Context().Indexes().Refresh(session.Context(), "Refresh C++ index");
    session.Context().Jobs().Wait(job_id);

    const auto snapshot = session.Context().Indexes().Snapshot("vanta.index.cpp.compilationDatabase");
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->status == vanta::IndexStatus::Ready);
    REQUIRE(snapshot->item_count == 1);

    const auto includes = session.Context().Indexes().Query(session.Context(), {
        .kind = vanta::IndexQueryKind::Includes,
        .query = "main.cpp",
        .provider_id = "vanta.index.cpp.compilationDatabase",
    });
    REQUIRE(includes.ok);
    REQUIRE(includes.hits.size() == 1);
    REQUIRE(includes.hits[0].file.ToUri() == session.Context().CurrentWorkspace().File("include").ToUri());

    session.Close();
}

}

TEST_CASE("Index service search", "[index]") {
    vanta::tests::TestIndexServiceSearch();
}

TEST_CASE("C++ compilation database index provider", "[index][cpp]") {
    vanta::tests::TestCppCompilationDatabaseIndexProvider();
}
