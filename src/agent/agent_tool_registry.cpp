#include "vanta/agent/agent_tool_registry.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace vanta {

RegistrationHandle AgentToolRegistry::RegisterTool(AgentToolDefinition definition) {
    const std::string id = definition.id;
    tools_.push_back(std::move(definition));
    return RegistrationHandle([this, id] {
        RemoveTool(id);
    });
}

void AgentToolRegistry::RemoveTool(const std::string& id) {
    tools_.erase(
        std::remove_if(tools_.begin(), tools_.end(), [&](const AgentToolDefinition& tool) {
            return tool.id == id;
        }),
        tools_.end());
}

std::optional<Value> AgentToolRegistry::CallTool(const std::string& id, const Value& input) const {
    for (const AgentToolDefinition& tool : tools_) {
        if (tool.id == id) {
            return tool.handler(input);
        }
    }
    return std::nullopt;
}

std::vector<AgentToolDefinition> AgentToolRegistry::Tools() const {
    return tools_;
}

std::optional<std::string> AgentToolRegistry::ReadCode(const VirtualFile& file) const {
    return file.ReadText();
}

std::string AgentToolRegistry::ExplainDiagnostic(const Diagnostic& diagnostic) const {
    std::ostringstream stream;
    stream << diagnostic.location.file.ToUri().ToString() << ':' << diagnostic.location.line << ':' << diagnostic.location.column;
    stream << " reports " << ToString(diagnostic.severity) << " from " << diagnostic.source << ". ";
    stream << diagnostic.message;
    return stream.str();
}

}
