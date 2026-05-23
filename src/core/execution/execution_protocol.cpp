#include "mornox/execution/execution_protocol.h"

#include <cstdint>
#include <utility>

namespace mornox {

std::string ToString(ExecutionEventKind kind) {
    switch (kind) {
    case ExecutionEventKind::Started:
        return "started";
    case ExecutionEventKind::Stdout:
        return "stdout";
    case ExecutionEventKind::Stderr:
        return "stderr";
    case ExecutionEventKind::Progress:
        return "progress";
    case ExecutionEventKind::Finished:
        return "finished";
    }
    return "started";
}

}
