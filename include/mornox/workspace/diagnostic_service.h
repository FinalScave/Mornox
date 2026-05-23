#pragma once

#include <map>
#include <string>
#include <vector>

#include "mornox/core/diagnostic.h"
#include "mornox/core/event.h"

namespace mornox {

struct DiagnosticChangeEvent {
    std::string source;
    std::vector<Diagnostic> diagnostics;
};

class DiagnosticService {
public:
    static constexpr const char* kServiceId = "mornox.diagnostics";

    void Publish(std::string source, std::vector<Diagnostic> diagnostics);
    void Clear(const std::string& source);
    std::vector<Diagnostic> DiagnosticsForFile(const VirtualFile& file) const;
    std::vector<Diagnostic> AllDiagnostics() const;

    std::uint64_t OnDidChangeDiagnostics(EventBus<DiagnosticChangeEvent>::Listener listener);
    void RemoveDiagnosticsListener(std::uint64_t listener_id);

private:
    std::map<std::string, std::vector<Diagnostic>> diagnostics_by_source_;
    EventBus<DiagnosticChangeEvent> on_did_change_;
};

}
