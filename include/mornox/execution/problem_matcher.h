#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "mornox/core/diagnostic.h"
#include "mornox/workspace/workspace.h"

namespace mornox {

struct ProblemMatch {
    std::string file_path;
    int line = 0;
    int column = 0;
    DiagnosticSeverity severity = DiagnosticSeverity::Note;
    std::string source;
    std::string message;
};

class ProblemMatcher {
public:
    std::vector<ProblemMatch> MatchCompilerOutput(const std::string& output) const;
};

class DiagnosticResolver {
public:
    std::vector<Diagnostic> Resolve(
        const std::vector<ProblemMatch>& matches,
        const Workspace& workspace,
        const std::filesystem::path& build_directory = {}) const;

private:
    VirtualFile ResolveFile(
        const ProblemMatch& match,
        const Workspace& workspace,
        const std::filesystem::path& build_directory) const;
};

}
