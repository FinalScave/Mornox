#include "test_support.h"

namespace vanta::tests {

void TestDiagnosticService() {
    vanta::DiagnosticService diagnostics;
    bool changed = false;
    diagnostics.OnDidChangeDiagnostics([&](const vanta::DiagnosticChangeEvent& event) {
        changed = event.source == "build";
    });

    vanta::Diagnostic diagnostic;
    vanta::VirtualFileSystem vfs;
    const vanta::VirtualFile main_file = vfs.LocalFile("main.cpp");
    diagnostic.location.file = main_file;
    diagnostic.location.line = 4;
    diagnostic.location.column = 2;
    diagnostic.severity = vanta::DiagnosticSeverity::Error;
    diagnostic.message = "expected expression";
    diagnostics.Publish("build", {diagnostic});

    REQUIRE(changed);
    REQUIRE(diagnostics.AllDiagnostics().size() == 1);
    REQUIRE(diagnostics.DiagnosticsForFile(main_file).size() == 1);

    diagnostics.Clear("build");
    REQUIRE(diagnostics.AllDiagnostics().empty());
}

void TestProblemMatcherResolvesWorkspaceFiles() {
    const auto root = MakeTempRoot();
    WriteFile(root / "src" / "main.cpp", "int main() {\n  return ;\n}\n");

    vanta::Workspace workspace;
    vanta::VirtualFileSystem vfs;
    workspace.BindFileSystem(vfs);
    std::string error;
    REQUIRE(workspace.Open(root, &error));

    const vanta::ProblemMatcher matcher;
    const auto matches = matcher.MatchCompilerOutput("src/main.cpp:2:10: error: expected expression\n");
    REQUIRE(matches.size() == 1);

    const vanta::DiagnosticResolver resolver;
    const auto diagnostics = resolver.Resolve(matches, workspace, root / "build");
    REQUIRE(diagnostics.size() == 1);
    REQUIRE(diagnostics[0].location.file.ToUri() == workspace.File("src/main.cpp").ToUri());
    REQUIRE(diagnostics[0].location.line == 2);
    REQUIRE(diagnostics[0].severity == vanta::DiagnosticSeverity::Error);
}

}

TEST_CASE("Diagnostic service", "[diagnostics]") {
    vanta::tests::TestDiagnosticService();
}

TEST_CASE("Problem matcher resolves workspace files", "[diagnostics]") {
    vanta::tests::TestProblemMatcherResolvesWorkspaceFiles();
}
