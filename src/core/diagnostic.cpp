#include "mornox/core/diagnostic.h"

namespace mornox {

std::string ToString(DiagnosticSeverity severity) {
    switch (severity) {
    case DiagnosticSeverity::Note:
        return "note";
    case DiagnosticSeverity::Warning:
        return "warning";
    case DiagnosticSeverity::Error:
        return "error";
    }
    return "note";
}

}
