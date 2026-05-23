#pragma once

#include <string>

#include "mornox/core/diagnostic.h"
#include "mornox/core/event.h"
#include "mornox/execution/job_service.h"
#include "mornox/vfs/file_watcher.h"

namespace mornox {

enum class IdeEventKind {
    WorkspaceOpened,
    WorkspaceClosed,
    FileChanged,
    FileCreated,
    FileDeleted,
    ProjectChanged,
    DocumentOpened,
    DocumentChanged,
    DocumentSaved,
    DocumentClosed,
    DiagnosticsChanged,
    IndexChanged,
    JobChanged,
    JobStarted,
    JobCompleted,
    ChangeSetProposed,
    ChangeSetApplied,
};

struct IdeEvent {
    IdeEventKind kind = IdeEventKind::WorkspaceOpened;
    VirtualFile file;
    std::string source;
    std::string message;
    JobId job_id = 0;
};

using IdeEventBus = EventBus<IdeEvent>;

std::string ToString(IdeEventKind kind);
IdeEventKind IdeEventKindFromFileChange(VirtualFileChangeKind kind);

}
