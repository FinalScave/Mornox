#include "test_support.h"

#include "mornox/language/refactoring_service.h"

namespace mornox::tests {

class FakeRefactoringProvider final : public mornox::RefactoringProvider {
public:
    std::string Id() const override {
        return "test.refactor";
    }

    bool Supports(mornox::RefactoringKind kind) const override {
        return kind == mornox::RefactoringKind::RenameSymbol;
    }

    mornox::RefactoringPrepareResult Prepare(mornox::WorkspaceContext&, const mornox::RefactoringRequest& request) const override {
        return {
            .ok = true,
            .title = request.title.empty() ? "Rename symbol" : request.title,
            .affected_symbols = {{
                .id = "main",
                .name = "main",
                .kind = mornox::SymbolKind::Function,
            }},
        };
    }

    mornox::RefactoringPlan Plan(mornox::WorkspaceContext&, const mornox::RefactoringRequest& request) const override {
        mornox::WorkspaceEdit edit;
        edit.operations.push_back({
            .kind = mornox::WorkspaceEditOperationKind::EditText,
            .file = request.document.file,
            .text_edits = {{
                .range = {
                    .start = {.line = 0, .character = 4},
                    .end = {.line = 0, .character = 8},
                },
                .replacement_text = "entry",
            }},
        });
        return {
            .ok = true,
            .title = request.title.empty() ? "Rename symbol" : request.title,
            .edit = std::move(edit),
        };
    }
};

void TestRefactoringServiceCreatesChangeSet() {
    const auto root = MakeTempRoot();
    WriteFile(root / "main.cpp", "int main() { return 0; }\n");

    mornox::VirtualFileSystem vfs;
    mornox::WorkspaceRuntime session(vfs, mornox::InlineJobDispatcher());
    std::string error;
    REQUIRE(session.Open(root, &error));
    const mornox::VirtualFile main_file = session.Context().CurrentWorkspace().File("main.cpp");
    REQUIRE(session.Context().Documents().OpenDocument(main_file, &error) != nullptr);

    auto registration = session.Context().Refactorings().RegisterProvider(std::make_unique<FakeRefactoringProvider>());
    REQUIRE(registration.Registered());
    REQUIRE(session.Context().Refactorings().ProviderIds().size() == 1);

    mornox::RefactoringRequest request;
    request.kind = mornox::RefactoringKind::RenameSymbol;
    request.document.file = main_file;
    request.document.language_id = "cpp";
    request.position = {.line = 0, .character = 4};
    request.title = "Rename main";
    request.params = mornox::RenameSymbolParams{.new_name = "entry"};

    const mornox::RefactoringPrepareResult prepared = session.Context().Refactorings().Prepare(session.Context(), request);
    REQUIRE(prepared.ok);
    REQUIRE(prepared.affected_symbols.size() == 1);

    const mornox::RefactoringPlan plan = session.Context().Refactorings().Plan(session.Context(), request);
    REQUIRE(plan.ok);
    const std::optional<mornox::ChangeSet> change_set = session.Context().Refactorings().CreateChangeSet(session.Context(), plan, "test.refactor");
    REQUIRE(change_set.has_value());
    REQUIRE(session.Context().Changes().Approve(change_set->id).ok);
    REQUIRE(session.Context().Changes().ApplyApproved(session.Context().CurrentWorkspace(), session.Context().Documents(), change_set->id).ok);

    const auto text = session.Context().Documents().ReadText(main_file);
    REQUIRE(text.has_value());
    REQUIRE(text->find("entry") != std::string::npos);
    registration.Unregister();
    REQUIRE(session.Context().Refactorings().ProviderIds().empty());
    session.Close();
}

}

TEST_CASE("Refactoring service creates change set", "[refactoring]") {
    mornox::tests::TestRefactoringServiceCreatesChangeSet();
}
