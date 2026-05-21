#include "vanta/core/diagnostic.h"

namespace vanta {

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
