#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "mornox/core/registration.h"
#include "mornox/core/value.h"

namespace mornox {

using AgentToolHandler = std::function<Value(const Value&)>;

struct AgentToolDefinition {
    std::string id;
    std::string description;
    Value input_schema = Value::ObjectValue();
    AgentToolHandler handler;
};

class AgentToolRegistry {
public:
    static constexpr const char* kServiceId = "mornox.agent.tools";

    RegistrationHandle RegisterTool(AgentToolDefinition definition);
    void RemoveTool(const std::string& id);
    std::optional<Value> CallTool(const std::string& id, const Value& input) const;
    std::vector<AgentToolDefinition> Tools() const;

private:
    struct RegisteredTool {
        AgentToolDefinition definition;
        std::uint64_t registration_id = 0;
    };

    void RemoveToolRegistration(const std::string& id, std::uint64_t registration_id);

    std::map<std::string, RegisteredTool> tools_;
    std::uint64_t next_registration_id_ = 1;
};

}
