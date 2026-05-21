#pragma once

#include <functional>
#include <string>
#include <vector>

#include "vanta/execution/job_service.h"
#include "vanta/core/value.h"

namespace vanta {

enum class ExecutionEventKind {
    Started,
    Stdout,
    Stderr,
    Progress,
    Finished,
};

struct ExecutionEvent {
    ExecutionEventKind kind = ExecutionEventKind::Started;
    JobId job_id = 0;
    std::string executor_id;
    std::string target_id;
    std::string text;
    double progress = -1.0;
    int exit_code = 0;
};

using ExecutionEventCallback = std::function<void(const ExecutionEvent&)>;

std::string ToString(ExecutionEventKind kind);

}
