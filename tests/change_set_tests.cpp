#include "test_support.h"

namespace vanta::tests {

void TestChangeSetDiff() {
    vanta::VirtualFileSystem vfs;
    const std::string diff = vanta::CreateUnifiedDiff(vfs.LocalFile("main.cpp"), "int main() { return 0; }\n", "int main() { return 1; }\n");
    REQUIRE(diff.find("-int main() { return 0; }") != std::string::npos);
    REQUIRE(diff.find("+int main() { return 1; }") != std::string::npos);
}

void TestChangeSetService() {
    const auto root = MakeTempRoot();
    WriteFile(root / "main.cpp", "int main() { return 0; }\n");

    vanta::Workspace workspace;
    vanta::VirtualFileSystem vfs;
    workspace.BindFileSystem(vfs);
    std::string error;
    REQUIRE(workspace.Open(root, &error));

    vanta::DocumentService documents;
    const vanta::VirtualFile main_file = workspace.File("main.cpp");
    documents.OpenDocument(main_file, &error);

    vanta::ChangeSetService changes;
    const auto change_set = changes.CreateFileReplacement(
        main_file,
        "test",
        "Change return value",
        "int main() { return 0; }\n",
        "int main() { return 1; }\n",
        documents.Document(main_file)->version);
    REQUIRE(changes.Approve(change_set.id).ok);
    REQUIRE(changes.ApplyApproved(workspace, documents, change_set.id, {.save_after_apply = true}).ok);
    REQUIRE(workspace.ReadTextFile("main.cpp")->find("return 1") != std::string::npos);
    auto applied = changes.Get(change_set.id);
    REQUIRE(applied.has_value());
    REQUIRE(applied->status == vanta::ChangeSetStatus::Applied);
    REQUIRE(!applied->undo_token.empty());
    REQUIRE(!applied->inverse_edit.operations.empty());
    REQUIRE(changes.UndoApplied(workspace, documents, change_set.id, {.save_after_apply = true}).ok);
    REQUIRE(workspace.ReadTextFile("main.cpp")->find("return 0") != std::string::npos);
    auto undone = changes.Get(change_set.id);
    REQUIRE(undone.has_value());
    REQUIRE(undone->status == vanta::ChangeSetStatus::Undone);
}

void TestStructuredWorkspaceEdits() {
    const auto root = MakeTempRoot();
    WriteFile(root / "old.cpp", "int old_value = 1;\n");
    WriteFile(root / "delete.cpp", "int delete_me = 1;\n");

    vanta::Workspace workspace;
    vanta::VirtualFileSystem vfs;
    workspace.BindFileSystem(vfs);
    std::string error;
    REQUIRE(workspace.Open(root, &error));

    vanta::DocumentService documents;
    vanta::ChangeSetService changes;
    const vanta::VirtualFile created_file = workspace.File("created.cpp");
    const vanta::ChangeSet create_set = changes.CreateFileCreation(
        created_file,
        "test",
        "Create file",
        "int created = 1;\n");
    REQUIRE(changes.Preflight(workspace, documents, create_set.id).ok);
    REQUIRE(changes.Approve(create_set.id).ok);
    REQUIRE(changes.ApplyApproved(workspace, documents, create_set.id, {.save_after_apply = true}).ok);
    REQUIRE(created_file.Exists());
    REQUIRE(created_file.ReadText()->find("created") != std::string::npos);
    REQUIRE(changes.UndoApplied(workspace, documents, create_set.id).ok);
    REQUIRE(!created_file.Exists());

    const vanta::VirtualFile deleted_file = workspace.File("delete.cpp");
    const vanta::ChangeSet delete_set = changes.CreateFileDeletion(
        deleted_file,
        "test",
        "Delete file",
        "int delete_me = 1;\n");
    REQUIRE(changes.Approve(delete_set.id).ok);
    REQUIRE(changes.ApplyApproved(workspace, documents, delete_set.id).ok);
    REQUIRE(!deleted_file.Exists());
    REQUIRE(changes.UndoApplied(workspace, documents, delete_set.id).ok);
    REQUIRE(deleted_file.Exists());

    const vanta::VirtualFile old_file = workspace.File("old.cpp");
    const vanta::VirtualFile new_file = workspace.File("new.cpp");
    const vanta::ChangeSet rename_set = changes.CreateFileRename(
        old_file,
        new_file,
        "test",
        "Rename file");
    REQUIRE(rename_set.unified_diff.find("rename from") != std::string::npos);
    REQUIRE(changes.Approve(rename_set.id).ok);
    REQUIRE(changes.ApplyApproved(workspace, documents, rename_set.id).ok);
    REQUIRE(!old_file.Exists());
    REQUIRE(new_file.Exists());
    REQUIRE(changes.UndoApplied(workspace, documents, rename_set.id).ok);
    REQUIRE(old_file.Exists());
    REQUIRE(!new_file.Exists());

    const vanta::ChangeSet conflict_set = changes.CreateFileCreation(
        old_file,
        "test",
        "Create conflicting file",
        "int conflict = 1;\n");
    const auto preflight = changes.Preflight(workspace, documents, conflict_set.id);
    REQUIRE(!preflight.ok);
    REQUIRE(preflight.conflicts.size() == 1);
    REQUIRE(changes.Approve(conflict_set.id).ok);
    const auto apply_conflict = changes.ApplyApproved(workspace, documents, conflict_set.id);
    REQUIRE(!apply_conflict.ok);
    REQUIRE(apply_conflict.message.find("conflict") != std::string::npos);
}

}

TEST_CASE("Change set diff", "[changes]") {
    vanta::tests::TestChangeSetDiff();
}

TEST_CASE("Change set service", "[changes]") {
    vanta::tests::TestChangeSetService();
}

TEST_CASE("Structured workspace edits", "[changes]") {
    vanta::tests::TestStructuredWorkspaceEdits();
}

