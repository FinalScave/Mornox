#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "vanta/core/registration.h"
#include "vanta/core/diagnostic.h"
#include "vanta/core/value.h"
#include "vanta/vfs/virtual_file.h"

namespace vanta {

using AgentToolHandler = std::function<Value(const Value&)>;

struct AgentToolDefinition {
    std::string id;
    std::string description;
    Value input_schema = Value::ObjectValue();
    AgentToolHandler handler;
};

class AgentToolRegistry {
public:
    RegistrationHandle RegisterTool(AgentToolDefinition definition);
    void RemoveTool(const std::string& id);
    std::optional<Value> CallTool(const std::string& id, const Value& input) const;
    std::vector<AgentToolDefinition> Tools() const;

    std::optional<std::string> ReadCode(const VirtualFile& file) const;
    std::string ExplainDiagnostic(const Diagnostic& diagnostic) const;

private:
    std::vector<AgentToolDefinition> tools_;
};

}
