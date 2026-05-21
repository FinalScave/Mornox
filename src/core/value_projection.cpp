#include "core/value_projection.h"

#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>

#include "vanta/core/diagnostic.h"

namespace vanta::internal {
namespace {

Value StringsProjection(const std::vector<std::string>& values) {
    Value::Array result;
    for (const std::string& value : values) {
        result.push_back(Value(value));
    }
    return Value::ArrayValue(std::move(result));
}

Value VirtualFilesProjection(const std::vector<VirtualFile>& files) {
    Value::Array values;
    for (const VirtualFile& file : files) {
        values.push_back(Value(file.ToUri().ToString()));
    }
    return Value::ArrayValue(std::move(values));
}

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

Value DiagnosticsProjection(const std::vector<Diagnostic>& diagnostics) {
    Value::Array values;
    for (const Diagnostic& diagnostic : diagnostics) {
        values.push_back(DiagnosticProjection(diagnostic));
    }
    return Value::ArrayValue(std::move(values));
}

Value IndexHitProjection(const IndexHit& hit) {
    return Value::ObjectValue({
        {"file", Value(hit.file.ToUri().ToString())},
        {"startLine", Value(static_cast<std::int64_t>(hit.range.start.line))},
        {"startCharacter", Value(static_cast<std::int64_t>(hit.range.start.character))},
        {"endLine", Value(static_cast<std::int64_t>(hit.range.end.line))},
        {"endCharacter", Value(static_cast<std::int64_t>(hit.range.end.character))},
        {"title", Value(hit.title)},
        {"preview", Value(hit.preview)},
        {"providerId", Value(hit.provider_id)},
        {"score", Value(static_cast<std::int64_t>(hit.score))},
    });
}

Value BuildResultProjection(const BuildResult& result) {
    return Value::ObjectValue({
        {"exitCode", Value(static_cast<std::int64_t>(result.exit_code))},
        {"output", Value(result.output)},
        {"diagnostics", DiagnosticsProjection(result.diagnostics)},
        {"events", ExecutionEventsProjection(result.events)},
    });
}

Value AgentContextItemProjection(const AgentContextItem& item) {
    return Value::ObjectValue({
        {"providerId", Value(item.provider_id)},
        {"kind", Value(ToString(item.kind))},
        {"title", Value(item.title)},
        {"file", Value(item.file.ToUri().ToString())},
        {"text", Value(item.text)},
        {"data", item.payload.value_or(Value::ObjectValue())},
    });
}

Value CppCompileCommandProjection(const CppCompileCommand& command) {
    Value::Array arguments;
    for (const std::string& argument : CppCompileArguments(command)) {
        arguments.push_back(Value(argument));
    }
    return Value::ObjectValue({
        {"directory", Value(command.directory.string())},
        {"file", Value(command.file.string())},
        {"command", Value(command.command)},
        {"arguments", Value::ArrayValue(std::move(arguments))},
    });
}

Value CppTranslationUnitProjection(const CppTranslationUnit& unit) {
    Value::Array include_directories;
    for (const VirtualFile& directory : unit.include_directories) {
        include_directories.push_back(Value(directory.ToUri().ToString()));
    }

    return Value::ObjectValue({
        {"sourceFile", Value(unit.source_file.ToUri().ToString())},
        {"includeDirectories", Value::ArrayValue(std::move(include_directories))},
        {"defines", StringsProjection(unit.defines)},
        {"compileArguments", StringsProjection(unit.compile_arguments)},
    });
}

Value LanguageTraceProjection(const LanguageRequestTrace& trace) {
    return Value::ObjectValue({
        {"id", Value(static_cast<std::int64_t>(trace.id))},
        {"method", Value(trace.method)},
        {"rawRequest", Value(trace.raw_request)},
        {"rawResponse", Value(trace.raw_response)},
    });
}

Value CompletionItemsProjection(const std::vector<CompletionItem>& items) {
    Value::Array values;
    for (const CompletionItem& item : items) {
        values.push_back(Value::ObjectValue({
            {"label", Value(item.label)},
            {"insertText", Value(item.insert_text)},
            {"detail", Value(item.detail)},
            {"documentation", Value(item.documentation)},
        }));
    }
    return Value::ArrayValue(std::move(values));
}

Value LocationsProjection(const std::vector<Location>& locations) {
    Value::Array values;
    for (const Location& location : locations) {
        values.push_back(Value::ObjectValue({
            {"file", Value(location.file.ToUri().ToString())},
            {"range", Value::ObjectValue({
                {"start", Value::ObjectValue({
                    {"line", Value(static_cast<std::int64_t>(location.range.start.line))},
                    {"character", Value(static_cast<std::int64_t>(location.range.start.character))},
                })},
                {"end", Value::ObjectValue({
                    {"line", Value(static_cast<std::int64_t>(location.range.end.line))},
                    {"character", Value(static_cast<std::int64_t>(location.range.end.character))},
                })},
            })},
        }));
    }
    return Value::ArrayValue(std::move(values));
}

Value TokenDataProjection(const std::vector<std::int64_t>& data) {
    Value::Array values;
    for (std::int64_t value : data) {
        values.push_back(Value(value));
    }
    return Value::ArrayValue(std::move(values));
}

Value LanguagePayloadProjection(const CodeIntelligencePayload& payload) {
    return std::visit([](const auto& value) -> Value {
        using Payload = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Payload, std::monostate>) {
            return Value(nullptr);
        } else if constexpr (std::is_same_v<Payload, CompletionList>) {
            return LanguageCompletionProjection(value);
        } else if constexpr (std::is_same_v<Payload, HoverResult>) {
            return LanguageHoverProjection(value);
        } else if constexpr (std::is_same_v<Payload, LocationResult>) {
            return LanguageLocationProjection(value);
        } else {
            return LanguageSemanticTokensProjection(value);
        }
    }, payload);
}

}

Value AgentContextProjection(const AgentContext& context) {
    Value::Array items;
    for (const AgentContextItem& item : context.items) {
        items.push_back(AgentContextItemProjection(item));
    }
    return Value::ObjectValue({
        {"items", Value::ArrayValue(std::move(items))},
    });
}

Value AgentOperationEventProjection(const AgentOperationEvent& event) {
    return Value::ObjectValue({
        {"operationId", Value(event.operation_id)},
        {"kind", Value(ToString(event.kind))},
        {"status", Value(ToString(event.status))},
        {"message", Value(event.message)},
        {"data", event.payload.value_or(Value::ObjectValue())},
    });
}

Value AgentOperationResultProjection(const AgentOperationResult& result) {
    Value::Array hits;
    for (const IndexHit& hit : result.search_hits) {
        hits.push_back(IndexHitProjection(hit));
    }
    return Value::ObjectValue({
        {"ok", Value(result.ok)},
        {"kind", Value(ToString(result.kind))},
        {"error", Value(result.error)},
        {"message", Value(result.message)},
        {"text", Value(result.text)},
        {"searchHits", Value::ArrayValue(std::move(hits))},
        {"changeSetId", Value(result.change_set_id)},
        {"buildResult", BuildResultProjection(result.build_result)},
        {"data", result.payload.value_or(Value::ObjectValue())},
    });
}

Value CMakeProjectGraphProjection(const CMakeProjectGraph& graph) {
    Value::Array targets;
    for (const CMakeTarget& target : graph.targets) {
        targets.push_back(Value::ObjectValue({
            {"name", Value(target.name)},
            {"kind", Value(target.kind)},
            {"sourceFiles", VirtualFilesProjection(target.source_files)},
            {"includeDirectories", VirtualFilesProjection(target.include_directories)},
            {"defines", StringsProjection(target.defines)},
            {"compileArguments", StringsProjection(target.compile_arguments)},
        }));
    }

    return Value::ObjectValue({
        {"targets", Value::ArrayValue(std::move(targets))},
        {"sourceFiles", VirtualFilesProjection(graph.source_files)},
        {"includeDirectories", VirtualFilesProjection(graph.include_directories)},
        {"generatedFiles", VirtualFilesProjection(graph.generated_files)},
        {"defines", StringsProjection(graph.defines)},
        {"compileArguments", StringsProjection(graph.compile_arguments)},
    });
}

Value CMakeProjectModelProjection(const CMakeProjectModel& model) {
    return Value::ObjectValue({
        {"detected", Value(model.detected)},
        {"cmakeListsFile", Value(model.cmake_lists_file.ToUri().ToString())},
        {"compileCommandsFile", Value(model.compile_commands_file.ToUri().ToString())},
        {"buildDirectory", Value(model.build_directory.string())},
        {"graph", CMakeProjectGraphProjection(model.graph)},
    });
}

Value CppCompilationDatabaseProjection(const CppCompilationDatabase& database) {
    Value::Array commands;
    for (const CppCompileCommand& command : database.commands) {
        commands.push_back(CppCompileCommandProjection(command));
    }
    Value::Array translation_units;
    for (const CppTranslationUnit& unit : database.translation_units) {
        translation_units.push_back(CppTranslationUnitProjection(unit));
    }
    return Value::ObjectValue({
        {"file", Value(database.file.ToUri().ToString())},
        {"available", Value(database.Available())},
        {"commands", Value::ArrayValue(std::move(commands))},
        {"translationUnits", Value::ArrayValue(std::move(translation_units))},
    });
}

Value ExecutionTargetProjection(const ExecutionTarget& target) {
    return Value::ObjectValue({
        {"id", Value(target.id)},
        {"executorId", Value(target.executor_id)},
        {"name", Value(target.name)},
        {"kind", Value(ToString(target.kind))},
        {"capabilities", StringsProjection(target.capabilities)},
    });
}

Value ExecutionEventProjection(const ExecutionEvent& event) {
    return Value::ObjectValue({
        {"kind", Value(ToString(event.kind))},
        {"jobId", Value(static_cast<std::int64_t>(event.job_id))},
        {"executorId", Value(event.executor_id)},
        {"targetId", Value(event.target_id)},
        {"text", Value(event.text)},
        {"progress", Value(event.progress)},
        {"exitCode", Value(static_cast<std::int64_t>(event.exit_code))},
    });
}

Value ExecutionEventsProjection(const std::vector<ExecutionEvent>& events) {
    Value::Array values;
    for (const ExecutionEvent& event : events) {
        values.push_back(ExecutionEventProjection(event));
    }
    return Value::ArrayValue(std::move(values));
}

Value RunConfigurationProjection(const RunConfiguration& configuration) {
    return Value::ObjectValue({
        {"id", Value(configuration.id)},
        {"name", Value(configuration.name)},
        {"typeId", Value(configuration.type_id)},
        {"targetId", Value(configuration.target_id)},
        {"data", Value::ObjectValue()},
        {"temporary", Value(configuration.temporary)},
    });
}

Value RunResultProjection(const RunResult& result) {
    return Value::ObjectValue({
        {"exitCode", Value(static_cast<std::int64_t>(result.exit_code))},
        {"output", Value(result.output)},
        {"diagnostics", DiagnosticsProjection(result.diagnostics)},
        {"jobId", Value(static_cast<std::int64_t>(result.job_id))},
    });
}

Value LanguageCompletionProjection(const CompletionList& result) {
    return Value::ObjectValue({
        {"ok", Value(result.ok)},
        {"error", Value(result.error)},
        {"incomplete", Value(result.incomplete)},
        {"items", CompletionItemsProjection(result.items)},
        {"trace", LanguageTraceProjection(result.trace)},
    });
}

Value LanguageHoverProjection(const HoverResult& result) {
    return Value::ObjectValue({
        {"ok", Value(result.ok)},
        {"error", Value(result.error)},
        {"contents", Value(result.contents)},
        {"trace", LanguageTraceProjection(result.trace)},
    });
}

Value LanguageLocationProjection(const LocationResult& result) {
    return Value::ObjectValue({
        {"ok", Value(result.ok)},
        {"error", Value(result.error)},
        {"locations", LocationsProjection(result.locations)},
        {"trace", LanguageTraceProjection(result.trace)},
    });
}

Value LanguageSemanticTokensProjection(const SemanticTokens& result) {
    return Value::ObjectValue({
        {"ok", Value(result.ok)},
        {"error", Value(result.error)},
        {"data", TokenDataProjection(result.data)},
        {"trace", LanguageTraceProjection(result.trace)},
    });
}

Value LanguageErrorProjection(const std::string& error) {
    return Value::ObjectValue({
        {"ok", Value(false)},
        {"error", Value(error)},
    });
}

Value CodeIntelligenceResultProjection(const CodeIntelligenceResult& result) {
    return Value::ObjectValue({
        {"kind", Value(ToString(result.kind))},
        {"document", Value(result.document_uri.ToString())},
        {"requestedVersion", Value(static_cast<std::int64_t>(result.requested_version))},
        {"currentVersion", Value(static_cast<std::int64_t>(result.current_version))},
        {"ok", Value(result.ok)},
        {"cancelled", Value(result.cancelled)},
        {"stale", Value(result.stale)},
        {"timedOut", Value(result.timed_out)},
        {"error", Value(result.error)},
        {"result", LanguagePayloadProjection(result.payload)},
    });
}

}
