#pragma once

#include <map>
#include <string>

#include "vanta/workspace/command_registry.h"

namespace vanta::internal {

class CommandRegistryImpl final : public CommandRegistry {
public:
    RegistrationHandle RegisterCommand(const std::string& id, CommandHandler handler) override;
    std::optional<Value> Execute(const std::string& id, const Value& arguments) const override;
    std::vector<std::string> List() const override;

private:
    void Unregister(const std::string& id);

    std::map<std::string, CommandHandler> handlers_;
};

}
