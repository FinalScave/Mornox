#include "vanta/plugin/core_plugin.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <system_error>

#include "clice_integration.h"
#include "cmake_build_provider.h"
#include "cmake_project_model.h"
#include "cpp_index.h"
#include "vanta/execution/problem_matcher.h"
#include "core/value_projection.h"
#include "vanta/language/language_service.h"
#include "vanta/plugin/approval_service.h"
#include "vanta/project/project.h"
#include "vanta/project/project_manager.h"
#include "vanta/project/project_template.h"
#include "vanta/core/json_codec.h"
#include "vanta/workspace/settings_service.h"

namespace vanta {
namespace {

Value DiagnosticProjection(const Diagnostic& diagnostic) {
    return Value::ObjectValue({
        {"file", Value(diagnostic.location.file.ToUri().ToString())},
        {"line", Value(static_cast<std::int64_t>(diagnostic.location.line))},
        {"column", Value(static_cast<std::int64_t>(diagnostic.location.column))},
        {"severity", Value(ToString(diagnostic.severity))},
        {"source", Value(diagnostic.source)},
        {"message", Value(diagnostic.message)},
    });
}

Value BuildEnvironmentProjection(const BuildEnvironment& environment) {
    return Value::ObjectValue({
        {"providerId", Value(environment.provider_id)},
        {"detected", Value(environment.detected)},
        {"buildDirectory", Value(environment.build_directory.string())},
    });
}

Value BuildResultProjection(const BuildResult& result) {
    Value::Array diagnostics;
    for (const Diagnostic& diagnostic : result.diagnostics) {
        diagnostics.push_back(DiagnosticProjection(diagnostic));
    }
    return Value::ObjectValue({
        {"exitCode", Value(static_cast<std::int64_t>(result.exit_code))},
        {"output", Value(result.output)},
        {"diagnostics", Value::ArrayValue(std::move(diagnostics))},
        {"events", internal::ExecutionEventsProjection(result.events)},
    });
}

TextPosition PositionFromValue(const Value& input) {
    TextPosition position;
    if (input.Contains("line") && input["line"].IsInt()) {
        position.line = static_cast<int>(input["line"].AsInt());
    }
    if (input.Contains("character") && input["character"].IsInt()) {
        position.character = static_cast<int>(input["character"].AsInt());
    }
    return position;
}

BuildRequest BuildRequestFromValue(const Value& input, BuildRequestKind kind) {
    BuildRequest request;
    request.kind = kind;
    request.target_id = input.StringValue("target").value_or(input.StringValue("targetId").value_or(""));
    if (auto build_directory = input.StringValue("buildDirectory")) {
        request.build_directory_override = *build_directory;
    }
    if (input.Contains("parameters") && input["parameters"].IsObject()) {
        if (auto build_directory = input["parameters"].StringValue("buildDirectory")) {
            request.build_directory_override = *build_directory;
        }
    }
    return request;
}

std::string NormalizePathString(const std::filesystem::path& path) {
    std::error_code error;
    const auto normalized = std::filesystem::weakly_canonical(path, error);
    return (error ? path : normalized).string();
}

std::filesystem::path DefaultBuildDirectory(const WorkspaceInfo& workspace) {
    return workspace.root_path / "build";
}

std::filesystem::path CompileCommandsPath(const std::filesystem::path& workspace_root) {
    const auto root_database = workspace_root / "compile_commands.json";
    if (std::filesystem::exists(root_database)) {
        return root_database;
    }
    const auto build_database = workspace_root / "build" / "compile_commands.json";
    if (std::filesystem::exists(build_database)) {
        return build_database;
    }
    return {};
}

std::filesystem::path InferBuildDirectory(const WorkspaceInfo& workspace, const std::filesystem::path& database_path) {
    if (!database_path.empty()) {
        return database_path.parent_path();
    }
    return DefaultBuildDirectory(workspace);
}

void AddUniqueString(std::vector<std::string>& values, std::set<std::string>& seen, std::string value) {
    if (value.empty() || !seen.insert(value).second) {
        return;
    }
    values.push_back(std::move(value));
}

void AddUniqueFile(std::vector<VirtualFile>& values, std::set<Uri>& seen, VirtualFile file) {
    if (!file.Valid() || !seen.insert(file.ToUri()).second) {
        return;
    }
    values.push_back(std::move(file));
}

std::vector<std::string> StringsFromValue(const Value& value) {
    std::vector<std::string> values;
    if (!value.IsArray()) {
        return values;
    }
    for (const Value& item : value.AsArray()) {
        if (item.IsString()) {
            values.push_back(item.AsString());
        }
    }
    return values;
}

Value StringsProjection(const std::vector<std::string>& values) {
    Value::Array array;
    for (const std::string& value : values) {
        array.push_back(Value(value));
    }
    return Value::ArrayValue(std::move(array));
}

std::string Lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string NormalizedExtension(const VirtualFile& file) {
    std::string extension = Lowercase(file.Extension());
    if (!extension.empty() && extension.front() == '.') {
        extension.erase(extension.begin());
    }
    return extension;
}

bool IsCppFile(const VirtualFile& file) {
    const std::string extension = NormalizedExtension(file);
    return extension == "cpp" || extension == "cxx" || extension == "cc" || extension == "c";
}

bool IsPythonFile(const VirtualFile& file) {
    return NormalizedExtension(file) == "py";
}

std::string DefaultStandard(const VirtualFile& file) {
    return NormalizedExtension(file) == "c" ? "c17" : "c++20";
}

std::string SanitizeId(std::string value) {
    for (char& character : value) {
        const bool allowed = std::isalnum(static_cast<unsigned char>(character)) ||
            character == '-' || character == '_' || character == '.';
        if (!allowed) {
            character = '_';
        }
    }
    return value.empty() ? "file" : value;
}

ValidationResult Missing(const std::string& message) {
    return {
        .ok = false,
        .messages = {message},
    };
}

CMakeProjectGraph BuildCMakeGraph(const Workspace& workspace, const CppCompilationDatabase& database) {
    CMakeProjectGraph graph;
    CMakeTarget target;
    target.name = workspace.Info().name.empty() ? "workspace" : workspace.Info().name;
    target.kind = "compileCommands";

    std::set<Uri> source_uris;
    std::set<Uri> target_source_uris;
    std::set<Uri> include_uris;
    std::set<Uri> target_include_uris;
    std::set<std::string> define_values;
    std::set<std::string> target_define_values;
    std::set<std::string> argument_values;

    for (const CppTranslationUnit& unit : database.translation_units) {
        AddUniqueFile(graph.source_files, source_uris, unit.source_file);
        AddUniqueFile(target.source_files, target_source_uris, unit.source_file);

        for (const VirtualFile& include_directory : unit.include_directories) {
            AddUniqueFile(graph.include_directories, include_uris, include_directory);
            AddUniqueFile(target.include_directories, target_include_uris, include_directory);
        }
        for (const std::string& define : unit.defines) {
            AddUniqueString(graph.defines, define_values, define);
            AddUniqueString(target.defines, target_define_values, define);
        }
        for (const std::string& argument : unit.compile_arguments) {
            AddUniqueString(graph.compile_arguments, argument_values, argument);
            target.compile_arguments.push_back(argument);
        }
    }

    if (!target.source_files.empty() || !target.include_directories.empty() || !target.defines.empty()) {
        graph.targets.push_back(std::move(target));
    }
    return graph;
}

Value AttachmentSummary(const CppCompilationDatabase& database) {
    return Value::ObjectValue({
        {"file", Value(database.file.ToUri().ToString())},
        {"translationUnits", Value(static_cast<std::int64_t>(database.translation_units.size()))},
    });
}

Value AttachmentSummary(const CMakeProjectModel& model) {
    return Value::ObjectValue({
        {"cmakeListsFile", Value(model.cmake_lists_file.ToUri().ToString())},
        {"compileCommandsFile", Value(model.compile_commands_file.ToUri().ToString())},
        {"buildDirectory", Value(model.build_directory.string())},
        {"targets", Value(static_cast<std::int64_t>(model.graph.targets.size()))},
        {"sourceFiles", Value(static_cast<std::int64_t>(model.graph.source_files.size()))},
    });
}

ProjectViewNode CMakeGroupNode(std::string id, std::string label, std::string description = {}) {
    return {
        .id = std::move(id),
        .label = std::move(label),
        .description = std::move(description),
        .kind = std::string(ProjectViewNodeKind::kGroup),
        .icon = "folder",
        .has_children = true,
        .synthetic = true,
    };
}

ProjectViewNode CMakeFileNode(const VirtualFile& file) {
    const bool directory = file.Valid() && file.Stat().kind == VirtualFileKind::Directory;
    return {
        .id = file.ToUri().ToString(),
        .label = file.DisplayName(),
        .kind = directory ? std::string(ProjectViewNodeKind::kDirectory) : std::string(ProjectViewNodeKind::kFile),
        .icon = directory ? "folder" : "file",
        .file = file,
        .has_file = file.Valid(),
        .has_children = false,
    };
}

std::vector<ProjectViewNode> CMakeFileNodes(const std::vector<VirtualFile>& files) {
    std::vector<ProjectViewNode> nodes;
    for (const VirtualFile& file : files) {
        nodes.push_back(CMakeFileNode(file));
    }
    return nodes;
}

std::string CMakeTargetNodeId(const CMakeTarget& target) {
    return "vanta.cmake.target:" + SanitizeId(target.name);
}

const CMakeTarget* FindCMakeTarget(const CMakeProjectModel& model, const std::string& node_id) {
    for (const CMakeTarget& target : model.graph.targets) {
        if (CMakeTargetNodeId(target) == node_id) {
            return &target;
        }
    }
    return nullptr;
}

class CMakeProjectModelProvider final : public ProjectModelProvider {
public:
    std::string Id() const override {
        return "vanta.cmake.projectProvider";
    }

    void Contribute(WorkspaceContext& context, ProjectModelBuilder& builder) const override {
        Workspace& workspace = context.CurrentWorkspace();
        const auto cmake_lists_path = workspace.Info().root_path / "CMakeLists.txt";
        const bool has_cmake_lists = std::filesystem::exists(cmake_lists_path);
        const auto database_path = CompileCommandsPath(workspace.Info().root_path);
        const bool has_compile_commands = !database_path.empty();
        if (!has_cmake_lists && !has_compile_commands) {
            return;
        }

        CMakeProjectModel cmake;
        cmake.detected = true;
        cmake.build_directory = InferBuildDirectory(workspace.Info(), database_path);
        if (has_cmake_lists) {
            cmake.cmake_lists_file = workspace.File(cmake_lists_path);
            ProjectFacet facet{
                .id = "cmake",
                .type = "cmake",
                .title = "CMake",
            };
            builder.AddFacet(facet);
            builder.AddFacetToPrimaryModule(std::move(facet));
        }

        if (has_compile_commands) {
            CppCompilationDatabase database = LoadCppCompilationDatabase(workspace, database_path);
            cmake.compile_commands_file = database.file;
            cmake.graph = BuildCMakeGraph(workspace, database);
            ProjectFacet facet{
                .id = "cpp",
                .type = "cpp",
                .title = "C++",
            };
            builder.AddFacet(facet);
            builder.AddFacetToPrimaryModule(std::move(facet));
            builder.SetAttachment({
                .id = CppCompilationDatabase::kAttachmentId,
                .kind = CppCompilationDatabase::kAttachmentKind,
                .title = "C++ Compilation Database",
                .summary = AttachmentSummary(database),
                .projection = internal::CppCompilationDatabaseProjection(database),
            }, std::move(database));
        }

        builder.SetAttachment({
            .id = CMakeProjectModel::kAttachmentId,
            .kind = CMakeProjectModel::kAttachmentKind,
            .title = "CMake Project",
            .summary = AttachmentSummary(cmake),
            .projection = internal::CMakeProjectModelProjection(cmake),
        }, std::move(cmake));
    }
};

class CMakeProjectViewProvider final : public ProjectViewProvider {
public:
    std::string Id() const override {
        return "vanta.cmake.projectViewProvider";
    }

    std::vector<ProjectView> Views(WorkspaceContext& context) const override {
        if (context.CurrentProject() == nullptr) {
            return {};
        }
        const auto* model = context.CurrentProject()->Model().Attachment<CMakeProjectModel>(CMakeProjectModel::kAttachmentId);
        if (model == nullptr || !model->detected) {
            return {};
        }
        return {{
            .id = "vanta.cmake",
            .title = "CMake",
            .icon = "cmake",
            .priority = 20,
        }};
    }

    std::vector<ProjectViewNode> TopLevelNodes(WorkspaceContext& context, const ProjectView&) override {
        const auto* model = context.RequireProject().Model().Attachment<CMakeProjectModel>(CMakeProjectModel::kAttachmentId);
        if (model == nullptr) {
            return {};
        }

        std::vector<ProjectViewNode> nodes;
        if (!model->graph.targets.empty()) {
            nodes.push_back(CMakeGroupNode("vanta.cmake.targets", "Targets"));
        }
        if (!model->graph.source_files.empty()) {
            nodes.push_back(CMakeGroupNode("vanta.cmake.sources", "Source Files"));
        }
        if (!model->graph.include_directories.empty()) {
            nodes.push_back(CMakeGroupNode("vanta.cmake.includes", "Include Directories"));
        }
        if (!model->graph.generated_files.empty()) {
            nodes.push_back(CMakeGroupNode("vanta.cmake.generated", "Generated Files"));
        }
        return nodes;
    }

    std::vector<ProjectViewNode> Children(WorkspaceContext& context, const ProjectView&, const ProjectViewNode& parent) override {
        const auto* model = context.RequireProject().Model().Attachment<CMakeProjectModel>(CMakeProjectModel::kAttachmentId);
        if (model == nullptr) {
            return {};
        }
        if (parent.id == "vanta.cmake.sources") {
            return CMakeFileNodes(model->graph.source_files);
        }
        if (parent.id == "vanta.cmake.includes") {
            return CMakeFileNodes(model->graph.include_directories);
        }
        if (parent.id == "vanta.cmake.generated") {
            return CMakeFileNodes(model->graph.generated_files);
        }
        if (parent.id == "vanta.cmake.targets") {
            std::vector<ProjectViewNode> nodes;
            for (const CMakeTarget& target : model->graph.targets) {
                nodes.push_back({
                    .id = CMakeTargetNodeId(target),
                    .label = target.name,
                    .description = target.kind,
                    .kind = "vanta.cmake.target",
                    .icon = "target",
                    .has_children = !target.source_files.empty() || !target.include_directories.empty() || !target.defines.empty(),
                    .synthetic = true,
                });
            }
            return nodes;
        }

        for (const CMakeTarget& target : model->graph.targets) {
            const std::string target_id = CMakeTargetNodeId(target);
            if (parent.id == target_id + ".sources") {
                return CMakeFileNodes(target.source_files);
            }
            if (parent.id == target_id + ".includes") {
                return CMakeFileNodes(target.include_directories);
            }
            if (parent.id == target_id + ".defines") {
                std::vector<ProjectViewNode> nodes;
                for (const std::string& define : target.defines) {
                    nodes.push_back({
                        .id = target_id + ".define:" + SanitizeId(define),
                        .label = define,
                        .kind = "vanta.cmake.define",
                        .icon = "symbol",
                        .synthetic = true,
                    });
                }
                return nodes;
            }
        }

        const CMakeTarget* target = FindCMakeTarget(*model, parent.id);
        if (target == nullptr) {
            return {};
        }
        std::vector<ProjectViewNode> nodes;
        if (!target->source_files.empty()) {
            ProjectViewNode group = CMakeGroupNode(parent.id + ".sources", "Source Files");
            group.has_children = true;
            nodes.push_back(std::move(group));
        }
        if (!target->include_directories.empty()) {
            ProjectViewNode group = CMakeGroupNode(parent.id + ".includes", "Include Directories");
            group.has_children = true;
            nodes.push_back(std::move(group));
        }
        if (!target->defines.empty()) {
            ProjectViewNode group = CMakeGroupNode(parent.id + ".defines", "Defines");
            group.has_children = true;
            nodes.push_back(std::move(group));
        }
        return nodes;
    }
};

class LanguagesCoreExtension final : public CoreExtension {
public:
    void Activate(ExtensionContext& context) override {
        WorkspaceContext& workspace = context.Context();
        for (Language language : DefaultLanguages()) {
            context.Track(workspace.Languages().RegisterLanguage(std::move(language)));
        }
        context.Log().Info("Activated languages core plugin");
    }
};

class CppSingleFileRunConfigurationType final : public RunConfigurationType {
public:
    std::string Id() const override {
        return "cpp.singleFile";
    }

    std::string Title() const override {
        return "C++ Single File";
    }

    struct Payload final : public RunConfigurationPayload {
        std::filesystem::path file;
        std::string standard = "c++20";
        std::vector<std::string> arguments;
        std::filesystem::path working_directory;

        std::unique_ptr<RunConfigurationPayload> Clone() const override {
            return std::make_unique<Payload>(*this);
        }
    };

    std::unique_ptr<RunConfigurationPayload> DefaultPayload(WorkspaceContext& context) const override {
        auto payload = std::make_unique<Payload>();
        payload->working_directory = context.CurrentWorkspace().Info().root_path;
        return payload;
    }

    std::unique_ptr<RunConfigurationPayload> DeserializePayload(const Value& value) const override {
        auto payload = std::make_unique<Payload>();
        if (!value.IsObject()) {
            return payload;
        }
        if (auto file = value.StringValue("file")) {
            payload->file = *file;
        }
        payload->standard = value.StringValue("standard").value_or("c++20");
        if (value.Contains("arguments")) {
            payload->arguments = StringsFromValue(value["arguments"]);
        }
        if (auto working_directory = value.StringValue("workingDirectory")) {
            payload->working_directory = *working_directory;
        }
        return payload;
    }

    Value SerializePayload(const RunConfigurationPayload& payload_value) const override {
        const auto* payload = dynamic_cast<const Payload*>(&payload_value);
        if (payload == nullptr) {
            return Value::ObjectValue();
        }
        return Value::ObjectValue({
            {"file", Value(payload->file.string())},
            {"standard", Value(payload->standard)},
            {"arguments", StringsProjection(payload->arguments)},
            {"workingDirectory", Value(payload->working_directory.string())},
        });
    }

    std::vector<ConfigurationField> Fields() const override {
        return {
            {.id = "file", .title = "File", .type = "file", .default_value = Value(""), .required = true},
            {.id = "standard", .title = "Standard", .type = "string", .default_value = Value("c++20")},
            {.id = "arguments", .title = "Arguments", .type = "stringArray", .default_value = Value::ArrayValue()},
            {.id = "workingDirectory", .title = "Working Directory", .type = "path", .default_value = Value("")},
        };
    }

    ValidationResult Validate(WorkspaceContext& context, const RunConfiguration& configuration) const override {
        const Payload* payload = dynamic_cast<const Payload*>(configuration.payload.get());
        if (payload == nullptr) {
            return Missing("Run configuration payload is invalid");
        }
        const VirtualFile file = context.CurrentWorkspace().File(payload->file);
        if (!file.Valid() || !file.Exists()) {
            return Missing("File does not exist");
        }
        if (!IsCppFile(file)) {
            return Missing("File is not a C or C++ source file");
        }
        return {};
    }

    RunResult Run(RunExecutionContext& context, const RunConfiguration& configuration) const override {
        const Payload* payload = dynamic_cast<const Payload*>(configuration.payload.get());
        if (payload == nullptr) {
            return {.exit_code = -1, .output = "Run configuration payload is invalid\n", .job_id = context.job_id};
        }
        const VirtualFile file = context.workspace.CurrentWorkspace().File(payload->file);
        const auto local_path = file.LocalPath();
        if (!local_path) {
            return {.exit_code = -1, .output = "File is not backed by a local path\n", .job_id = context.job_id};
        }

        std::filesystem::path working_directory = payload->working_directory;
        if (working_directory.empty()) {
            working_directory = local_path->parent_path();
        }

        std::error_code error;
        const std::filesystem::path run_directory = context.workspace.CurrentWorkspace().Info().root_path / ".vanta" / "run";
        std::filesystem::create_directories(run_directory, error);
        if (error) {
            return {.exit_code = -1, .output = "Could not create run directory\n", .job_id = context.job_id};
        }

        std::filesystem::path executable_path = run_directory / SanitizeId(local_path->stem().string());
#ifdef _WIN32
        executable_path += ".exe";
#endif

        std::vector<std::string> compile_arguments;
        compile_arguments.push_back("-std=" + (payload->standard.empty() ? DefaultStandard(file) : payload->standard));
        compile_arguments.push_back(local_path->string());
        compile_arguments.push_back("-o");
        compile_arguments.push_back(executable_path.string());
        const ExecutionResult compile = context.workspace.Execution().Execute(context.workspace, {
            .executable = NormalizedExtension(file) == "c" ? "cc" : "c++",
            .arguments = compile_arguments,
            .working_directory = working_directory,
            .job_id = context.job_id,
        }, context.target);

        RunResult result;
        result.exit_code = compile.exit_code;
        result.output = compile.output;
        result.job_id = context.job_id;
        const ProblemMatcher matcher;
        const DiagnosticResolver resolver;
        result.diagnostics = resolver.Resolve(
            matcher.MatchCompilerOutput(result.output),
            context.workspace.CurrentWorkspace(),
            working_directory);
        if (compile.exit_code != 0) {
            return result;
        }

        const ExecutionResult run = context.workspace.Execution().Execute(context.workspace, {
            .executable = executable_path.string(),
            .arguments = payload->arguments,
            .working_directory = working_directory,
            .job_id = context.job_id,
        }, context.target);
        result.exit_code = run.exit_code;
        result.output += run.output;
        return result;
    }
};

class CppSingleFileRunConfigurationProducer final : public RunConfigurationProducer {
public:
    std::string Id() const override {
        return "vanta.cpp.singleFileRunProducer";
    }

    std::vector<RunConfiguration> Produce(WorkspaceContext& context, const VirtualFile& focus_file) const override {
        VirtualFile file = focus_file;
        if (!file.Valid()) {
            if (const auto* single_file = context.RequireProject().Model().Attachment<SingleFileModel>(SingleFileModel::kAttachmentId)) {
                file = single_file->file;
            }
        }
        if (!file.Valid() || !IsCppFile(file)) {
            return {};
        }
        const auto local_path = file.LocalPath();
        if (!local_path) {
            return {};
        }
        auto payload = std::make_unique<CppSingleFileRunConfigurationType::Payload>();
        payload->file = *local_path;
        payload->standard = DefaultStandard(file);
        payload->working_directory = local_path->parent_path();

        RunConfiguration configuration;
        configuration.id = "temp.cpp." + SanitizeId(file.ToUri().ToString());
        configuration.name = "Run " + file.DisplayName();
        configuration.type_id = "cpp.singleFile";
        configuration.target_id = "local.default";
        configuration.payload = std::move(payload);
        configuration.temporary = true;
        return {std::move(configuration)};
    }
};

class PythonScriptRunConfigurationType final : public RunConfigurationType {
public:
    std::string Id() const override {
        return "python.script";
    }

    std::string Title() const override {
        return "Python Script";
    }

    struct Payload final : public RunConfigurationPayload {
        std::filesystem::path file;
        std::vector<std::string> arguments;
        std::filesystem::path working_directory;

        std::unique_ptr<RunConfigurationPayload> Clone() const override {
            return std::make_unique<Payload>(*this);
        }
    };

    std::unique_ptr<RunConfigurationPayload> DefaultPayload(WorkspaceContext& context) const override {
        auto payload = std::make_unique<Payload>();
        payload->working_directory = context.CurrentWorkspace().Info().root_path;
        return payload;
    }

    std::unique_ptr<RunConfigurationPayload> DeserializePayload(const Value& value) const override {
        auto payload = std::make_unique<Payload>();
        if (!value.IsObject()) {
            return payload;
        }
        if (auto file = value.StringValue("file")) {
            payload->file = *file;
        }
        if (value.Contains("arguments")) {
            payload->arguments = StringsFromValue(value["arguments"]);
        }
        if (auto working_directory = value.StringValue("workingDirectory")) {
            payload->working_directory = *working_directory;
        }
        return payload;
    }

    Value SerializePayload(const RunConfigurationPayload& payload_value) const override {
        const auto* payload = dynamic_cast<const Payload*>(&payload_value);
        if (payload == nullptr) {
            return Value::ObjectValue();
        }
        return Value::ObjectValue({
            {"file", Value(payload->file.string())},
            {"arguments", StringsProjection(payload->arguments)},
            {"workingDirectory", Value(payload->working_directory.string())},
        });
    }

    std::vector<ConfigurationField> Fields() const override {
        return {
            {.id = "file", .title = "File", .type = "file", .default_value = Value(""), .required = true},
            {.id = "arguments", .title = "Arguments", .type = "stringArray", .default_value = Value::ArrayValue()},
            {.id = "workingDirectory", .title = "Working Directory", .type = "path", .default_value = Value("")},
        };
    }

    ValidationResult Validate(WorkspaceContext& context, const RunConfiguration& configuration) const override {
        const Payload* payload = dynamic_cast<const Payload*>(configuration.payload.get());
        if (payload == nullptr) {
            return Missing("Run configuration payload is invalid");
        }
        const VirtualFile file = context.CurrentWorkspace().File(payload->file);
        if (!file.Valid() || !file.Exists()) {
            return Missing("File does not exist");
        }
        if (!IsPythonFile(file)) {
            return Missing("File is not a Python script");
        }
        return {};
    }

    RunResult Run(RunExecutionContext& context, const RunConfiguration& configuration) const override {
        const Payload* payload = dynamic_cast<const Payload*>(configuration.payload.get());
        if (payload == nullptr) {
            return {.exit_code = -1, .output = "Run configuration payload is invalid\n", .job_id = context.job_id};
        }
        const VirtualFile file = context.workspace.CurrentWorkspace().File(payload->file);
        const auto local_path = file.LocalPath();
        if (!local_path) {
            return {.exit_code = -1, .output = "File is not backed by a local path\n", .job_id = context.job_id};
        }
        std::filesystem::path working_directory = payload->working_directory;
        if (working_directory.empty()) {
            working_directory = local_path->parent_path();
        }

        std::vector<std::string> arguments;
        arguments.push_back(local_path->string());
        for (const std::string& argument : payload->arguments) {
            arguments.push_back(argument);
        }
        const ExecutionResult execution = context.workspace.Execution().Execute(context.workspace, {
            .executable = "python3",
            .arguments = arguments,
            .working_directory = working_directory,
            .job_id = context.job_id,
        }, context.target);
        return {
            .exit_code = execution.exit_code,
            .output = execution.output,
            .job_id = context.job_id,
        };
    }
};

class PythonScriptRunConfigurationProducer final : public RunConfigurationProducer {
public:
    std::string Id() const override {
        return "vanta.python.scriptRunProducer";
    }

    std::vector<RunConfiguration> Produce(WorkspaceContext& context, const VirtualFile& focus_file) const override {
        VirtualFile file = focus_file;
        if (!file.Valid()) {
            if (const auto* single_file = context.RequireProject().Model().Attachment<SingleFileModel>(SingleFileModel::kAttachmentId)) {
                file = single_file->file;
            }
        }
        if (!file.Valid() || !IsPythonFile(file)) {
            return {};
        }
        const auto local_path = file.LocalPath();
        if (!local_path) {
            return {};
        }
        auto payload = std::make_unique<PythonScriptRunConfigurationType::Payload>();
        payload->file = *local_path;
        payload->working_directory = local_path->parent_path();

        RunConfiguration configuration;
        configuration.id = "temp.python." + SanitizeId(file.ToUri().ToString());
        configuration.name = "Run " + file.DisplayName();
        configuration.type_id = "python.script";
        configuration.target_id = "local.default";
        configuration.payload = std::move(payload);
        configuration.temporary = true;
        return {std::move(configuration)};
    }
};

class CppCoreExtension final : public CoreExtension {
public:
    void Activate(ExtensionContext& context) override {
        WorkspaceContext& workspace = context.Context();
        context.Track(workspace.RunConfigurations().RegisterType(std::make_unique<CppSingleFileRunConfigurationType>()));
        context.Track(workspace.RunConfigurations().RegisterProducer(std::make_unique<CppSingleFileRunConfigurationProducer>()));
        context.Track(workspace.Indexes().RegisterProvider(CreateCppCompilationDatabaseIndexProvider()));
        context.Track(workspace.ProjectTemplates().RegisterCategory({.id = "cpp", .title = "C++", .sort_order = 10}));
        context.Track(workspace.ProjectTemplates().RegisterTemplate({
            .id = "cpp.console.cmake",
            .category_id = "cpp",
            .title = "C++ Console",
            .description = "A minimal CMake console project.",
            .files = {
                {.relative_path = "CMakeLists.txt", .contents = "cmake_minimum_required(VERSION 3.20)\nproject(VantaApp LANGUAGES CXX)\n\nset(CMAKE_CXX_STANDARD 20)\nadd_executable(vanta_app src/main.cpp)\n"},
                {.relative_path = "src/main.cpp", .contents = "#include <iostream>\n\nint main() {\n    std::cout << \"Hello from Vanta\" << std::endl;\n    return 0;\n}\n"},
            },
            .language_id = "cpp",
        }));
        context.Log().Info("Activated C++ core plugin");
    }
};

class PythonCoreExtension final : public CoreExtension {
public:
    void Activate(ExtensionContext& context) override {
        WorkspaceContext& workspace = context.Context();
        context.Track(workspace.RunConfigurations().RegisterType(std::make_unique<PythonScriptRunConfigurationType>()));
        context.Track(workspace.RunConfigurations().RegisterProducer(std::make_unique<PythonScriptRunConfigurationProducer>()));
        context.Track(workspace.ProjectTemplates().RegisterCategory({.id = "python", .title = "Python", .sort_order = 20}));
        context.Track(workspace.ProjectTemplates().RegisterTemplate({
            .id = "python.script",
            .category_id = "python",
            .title = "Python Script",
            .description = "A single Python entry point.",
            .files = {
                {.relative_path = "main.py", .contents = "def main():\n    print(\"Hello from Vanta\")\n\nif __name__ == \"__main__\":\n    main()\n"},
            },
            .language_id = "python",
        }));
        context.Log().Info("Activated Python core plugin");
    }
};

class CMakeCoreExtension final : public CoreExtension {
public:
    void Activate(ExtensionContext& context) override {
        WorkspaceContext& workspace = context.Context();
        build_ = &workspace.Build();
        workspace_context_ = &context.Context();

        context.Track(workspace.Build().RegisterProvider(std::make_unique<CMakeBuildProvider>()));
        context.Track(workspace.Projects().RegisterModelProvider(std::make_unique<CMakeProjectModelProvider>()));
        context.Track(workspace.Projects().RegisterViewProvider(std::make_unique<CMakeProjectViewProvider>()));
        workspace.Settings().RegisterNode({.id = "build.cmake", .parent_id = "build", .owner_id = "vanta.cmake", .title = "CMake", .order = 10});
        workspace.Settings().RegisterSetting({
            .id = "cmake.build_directory",
            .owner_id = "vanta.cmake",
            .node_id = "build.cmake",
            .title = "Build Directory",
            .description = "Default CMake build directory.",
            .type = SettingValueType::Path,
            .default_value = SettingValue::PathValue("build"),
            .supported_scopes = {SettingScopeKind::Workspace, SettingScopeKind::Project},
            .resolution_order = {SettingScopeKind::Project, SettingScopeKind::Workspace},
            .tags = {"cmake", "build", "directory"},
            .aliases = {"cmake build dir"},
            .order = 10,
        });

        context.Track(workspace.Commands().RegisterCommand("cmake.detect", [this](const Value&) {
                return BuildEnvironmentProjection(build_->Detect(*workspace_context_, workspace_context_->RequireProject().Model()));
        }));

        context.Track(workspace.Commands().RegisterCommand("cmake.build", [this](const Value& input) {
            return BuildResultProjection(build_->Run(*workspace_context_, BuildRequestFromValue(input, BuildRequestKind::Build)));
        }));

        context.Track(workspace.Commands().RegisterCommand("cmake.test", [this](const Value& input) {
            return BuildResultProjection(build_->Run(*workspace_context_, BuildRequestFromValue(input, BuildRequestKind::Test)));
        }));

        context.Track(workspace.AgentTools().RegisterTool({
            .id = "cmake.findOwningTarget",
            .description = "Find compile command information for a source file in the current CMake workspace.",
            .input_schema = Value::ObjectValue({
                {"type", Value("object")},
                {"required", Value::ArrayValue({Value("file")})},
            }),
            .handler = [this](const Value& input) {
                const std::string file = input.StringValue("file").value_or("");
                const std::string target_path = NormalizePathString(file);
                const auto* database = workspace_context_->RequireProject().Model().Attachment<CppCompilationDatabase>(CppCompilationDatabase::kAttachmentId);
                if (database == nullptr) {
                    return Value::ObjectValue({
                        {"found", Value(false)},
                        {"file", Value(file)},
                    });
                }
                for (const CppCompileCommand& command : database->commands) {
                    if (NormalizePathString(command.file) == target_path) {
                        return Value::ObjectValue({
                            {"found", Value(true)},
                            {"file", Value(command.file.string())},
                            {"directory", Value(command.directory.string())},
                            {"command", Value(command.command)},
                        });
                    }
                }
                return Value::ObjectValue({
                    {"found", Value(false)},
                    {"file", Value(file)},
                });
            },
        }));

        context.Log().Info("Activated CMake core plugin");
    }

    void Deactivate() override {
        build_ = nullptr;
        workspace_context_ = nullptr;
    }

private:
    BuildService* build_ = nullptr;
    WorkspaceContext* workspace_context_ = nullptr;
};

class GitCoreExtension final : public CoreExtension {
public:
    void Activate(ExtensionContext& context) override {
        WorkspaceContext& workspace = context.Context();
        git_ = &workspace.Git();

        context.Track(workspace.Commands().RegisterCommand("git.diff", [this](const Value&) {
            const GitDiff diff = git_->Diff();
            return Value::ObjectValue({
                {"exitCode", Value(static_cast<std::int64_t>(diff.exit_code))},
                {"text", Value(diff.text)},
            });
        }));

        context.Track(workspace.AgentTools().RegisterTool({
            .id = "git.diff",
            .description = "Return the current workspace Git diff.",
            .input_schema = Value::ObjectValue({{"type", Value("object")}}),
            .handler = [this](const Value&) {
                const GitDiff diff = git_->Diff();
                return Value::ObjectValue({
                    {"exitCode", Value(static_cast<std::int64_t>(diff.exit_code))},
                    {"text", Value(diff.text)},
                });
            },
        }));

        context.Log().Info("Activated Git core plugin");
    }

    void Deactivate() override {
        git_ = nullptr;
    }

private:
    GitService* git_ = nullptr;
};

class CliceCoreExtension final : public CoreExtension {
public:
    explicit CliceCoreExtension(CorePluginDependencies dependencies) : dependencies_(std::move(dependencies)) {}

    void Activate(ExtensionContext& context) override {
        WorkspaceContext& workspace = context.Context();
        languages_ = &workspace.Languages();
        approvals_ = &workspace.Approvals();
        subject_ = context.Extension().id;

        context.Track(workspace.Commands().RegisterCommand("clice.start", [this](const Value&) {
            if (approvals_ != nullptr && approvals_->RequestApproval({
                .subject = subject_,
                .permission = Permission::ProcessExecute,
                .action = "start clice language server",
                .high_risk = true,
            }) == ApprovalDecision::Deny) {
                return Value::ObjectValue({
                    {"ok", Value(false)},
                    {"running", Value(false)},
                    {"error", Value("process.execute was denied")},
                });
            }
            LanguageService* service = languages_->ServiceForLanguage("cpp");
            std::string error;
            const bool ok = service != nullptr && service->Start(&error);
            return Value::ObjectValue({
                {"ok", Value(ok)},
                {"running", Value(service != nullptr && service->Running())},
                {"error", Value(service == nullptr ? "C++ language service is not registered" : error)},
            });
        }));

        context.Track(workspace.Commands().RegisterCommand("clice.hover", [this](const Value& input) {
            const std::string file = input.StringValue("file").value_or("");
            return CallDocumentPosition("hover", VirtualFile(Uri::Parse(file), nullptr), PositionFromValue(input));
        }));

        context.Track(workspace.Commands().RegisterCommand("clice.completion", [this](const Value& input) {
            const std::string file = input.StringValue("file").value_or("");
            return CallDocumentPosition("completion", VirtualFile(Uri::Parse(file), nullptr), PositionFromValue(input));
        }));

        context.Track(workspace.Commands().RegisterCommand("clice.semantic_tokens", [this](const Value& input) {
            const std::string file = input.StringValue("file").value_or("");
            const VirtualFile virtual_file(Uri::Parse(file), nullptr);
            LanguageService* service = languages_->ServiceForDocument(virtual_file);
            if (service == nullptr) {
                return internal::LanguageErrorProjection("No language service is registered for document");
            }
            return internal::LanguageSemanticTokensProjection(service->SemanticTokensFull({.file = virtual_file, .language_id = "cpp"}));
        }));

        context.Track(workspace.AgentTools().RegisterTool({
            .id = "clice.findSymbol",
            .description = "Ask clice for the definition location near a source position.",
            .input_schema = Value::ObjectValue({
                {"type", Value("object")},
                {"required", Value::ArrayValue({Value("file"), Value("line"), Value("character")})},
            }),
            .handler = [this](const Value& input) {
                const std::string file = input.StringValue("file").value_or("");
                return CallDocumentPosition("definition", VirtualFile(Uri::Parse(file), nullptr), PositionFromValue(input));
            },
        }));

        clice_.Configure(dependencies_.clice.server_path, context.Workspace().root_path);
        language_service_ = clice_.CreateLanguageService();
        Language language;
        for (Language candidate : DefaultLanguages()) {
            if (candidate.id == "cpp") {
                language = std::move(candidate);
                break;
            }
        }
        if (language.id.empty()) {
            language.id = "cpp";
            language.association.extensions = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"};
        }
        language.service = language_service_.get();
        language.priority = 100;
        context.Track(workspace.Languages().RegisterLanguage(std::move(language)));

        context.Log().Info("Activated clice core plugin");
    }

    void Deactivate() override {
        languages_ = nullptr;
        approvals_ = nullptr;
        subject_.clear();
    }

private:
    Value CallDocumentPosition(const std::string& operation, const VirtualFile& file, TextPosition position) const {
        LanguageService* service = languages_->ServiceForDocument(file);
        if (service == nullptr) {
            return internal::LanguageErrorProjection("No language service is registered for document");
        }

        TextDocumentPosition request;
        request.document.file = file;
        request.document.language_id = "cpp";
        request.position = position;

        if (operation == "completion") {
            return internal::LanguageCompletionProjection(service->Completion(request));
        }
        if (operation == "hover") {
            return internal::LanguageHoverProjection(service->Hover(request));
        }
        return internal::LanguageLocationProjection(service->Definition(request));
    }

    CorePluginDependencies dependencies_;
    CliceIntegration clice_;
    std::unique_ptr<LanguageService> language_service_;
    LanguageRegistry* languages_ = nullptr;
    ApprovalService* approvals_ = nullptr;
    std::string subject_;
};

}

void CoreExtension::Deactivate() {}

void CorePluginRegistry::Add(std::string entry, CoreExtensionFactory factory) {
    factories_[std::move(entry)] = std::move(factory);
}

std::unique_ptr<CoreExtension> CorePluginRegistry::Create(const std::string& entry) const {
    auto it = factories_.find(entry);
    if (it == factories_.end()) {
        return nullptr;
    }
    return it->second();
}

std::vector<std::string> CorePluginRegistry::Entries() const {
    std::vector<std::string> result;
    for (const auto& [entry, factory] : factories_) {
        (void)factory;
        result.push_back(entry);
    }
    return result;
}

CorePluginRegistry CreateDefaultCorePluginRegistry(CorePluginDependencies dependencies) {
    CorePluginRegistry registry;
    registry.Add("builtin:languages", [] {
        return std::make_unique<LanguagesCoreExtension>();
    });
    registry.Add("builtin:cpp", [] {
        return std::make_unique<CppCoreExtension>();
    });
    registry.Add("builtin:python", [] {
        return std::make_unique<PythonCoreExtension>();
    });
    registry.Add("builtin:cmake", [] {
        return std::make_unique<CMakeCoreExtension>();
    });
    registry.Add("builtin:git", [] {
        return std::make_unique<GitCoreExtension>();
    });
    registry.Add("builtin:clice", [dependencies] {
        return std::make_unique<CliceCoreExtension>(dependencies);
    });
    return registry;
}

}
