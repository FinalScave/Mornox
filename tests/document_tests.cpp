#include "test_support.h"

#include "language/language_registry_impl.h"

namespace vanta::tests {

void TestDocumentService() {
    const auto root = MakeTempRoot();
    WriteFile(root / "main.cpp", "int main() {\n  return 0;\n}\n");

    vanta::Workspace workspace;
    vanta::VirtualFileSystem vfs;
    workspace.BindFileSystem(vfs);
    std::string error;
    REQUIRE(workspace.Open(root, &error));

    vanta::DocumentService documents;
    const vanta::VirtualFile main_file = workspace.File("main.cpp");
    vanta::TextDocument* document = documents.OpenDocument(main_file, &error);
    REQUIRE(document != nullptr);
    REQUIRE(document->version == 1);

    vanta::TextEdit edit;
    edit.range.start = {.line = 1, .character = 9};
    edit.range.end = {.line = 1, .character = 10};
    edit.replacement_text = "1";
    REQUIRE(documents.ApplyEdit(main_file, edit, document->version, &error));
    REQUIRE(documents.Document(main_file)->dirty);
    REQUIRE(documents.Document(main_file)->version == 2);
    REQUIRE(documents.SaveDocument(main_file, &error));
    REQUIRE(workspace.ReadTextFile("main.cpp")->find("return 1") != std::string::npos);
}

void TestDocumentOverlayRead() {
    const auto root = MakeTempRoot();
    WriteFile(root / "main.cpp", "int main() { return 0; }\n");

    vanta::Workspace workspace;
    vanta::VirtualFileSystem vfs;
    workspace.BindFileSystem(vfs);
    std::string error;
    REQUIRE(workspace.Open(root, &error));

    vanta::DocumentService documents;
    const vanta::VirtualFile main_file = workspace.File("main.cpp");
    vanta::TextDocument* document = documents.OpenDocument(main_file, &error);
    REQUIRE(document != nullptr);
    REQUIRE(documents.SetText(main_file, "int main() { return 2; }\n", document->version, &error));

    const auto snapshot = documents.ReadSnapshot(main_file);
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->open);
    REQUIRE(snapshot->dirty);
    REQUIRE(snapshot->text.find("return 2") != std::string::npos);
    REQUIRE(main_file.ReadText()->find("return 0") != std::string::npos);
}

void TestDocumentLanguageSynchronizer() {
    const auto root = MakeTempRoot();
    WriteFile(root / "main.cpp", "int main() { return 0; }\n");

    vanta::Workspace workspace;
    vanta::VirtualFileSystem vfs;
    workspace.BindFileSystem(vfs);
    std::string error;
    REQUIRE(workspace.Open(root, &error));

    vanta::DocumentService documents;
    vanta::internal::LanguageRegistryImpl languages;
    auto service = std::make_unique<FakeLanguageService>();
    FakeLanguageService* raw_service = service.get();
    languages.RegisterLanguage(FakeCppLanguage(raw_service));

    vanta::DocumentLanguageSynchronizer sync(documents, languages);
    sync.Start();

    const vanta::VirtualFile main_file = workspace.File("main.cpp");
    vanta::TextDocument* document = documents.OpenDocument(main_file, &error);
    REQUIRE(document != nullptr);
    REQUIRE(raw_service->opened == 1);
    REQUIRE(documents.SetText(main_file, "int main() { return 1; }\n", document->version, &error));
    REQUIRE(raw_service->changed == 1);
    REQUIRE(documents.SaveDocument(main_file, &error));
    REQUIRE(raw_service->saved == 1);
    REQUIRE(documents.CloseDocument(main_file));
    REQUIRE(raw_service->closed == 1);
}

}

TEST_CASE("Document service", "[documents]") {
    vanta::tests::TestDocumentService();
}

TEST_CASE("Document overlay read", "[documents]") {
    vanta::tests::TestDocumentOverlayRead();
}

TEST_CASE("Document language synchronizer", "[documents][language]") {
    vanta::tests::TestDocumentLanguageSynchronizer();
}
