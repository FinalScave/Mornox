#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "vanta/workspace/workspace.h"
#include "vanta/workspace/index_service.h"
#include "vanta/core/value.h"

namespace vanta {

struct CppCompileCommand {
    std::filesystem::path directory;
    std::filesystem::path file;
    std::string command;
    std::vector<std::string> arguments;
};

struct CppTranslationUnit {
    VirtualFile source_file;
    CppCompileCommand command;
    std::vector<VirtualFile> include_directories;
    std::vector<std::string> defines;
    std::vector<std::string> compile_arguments;
};

struct CppCompilationDatabase {
    static constexpr const char* kAttachmentId = "vanta.cpp.compilationDatabase";
    static constexpr const char* kAttachmentKind = "cpp.compilationDatabase";

    VirtualFile file;
    std::vector<CppCompileCommand> commands;
    std::vector<CppTranslationUnit> translation_units;

    bool Available() const;
};

std::vector<std::string> CppCompileArguments(const CppCompileCommand& command);
std::vector<CppCompileCommand> LoadCppCompileCommands(const std::filesystem::path& path);
CppCompilationDatabase LoadCppCompilationDatabase(const Workspace& workspace, const std::filesystem::path& path);
std::unique_ptr<IndexProvider> CreateCppCompilationDatabaseIndexProvider();

}
