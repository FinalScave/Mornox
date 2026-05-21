#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "vanta/core/registration.h"
#include "vanta/core/value.h"

namespace vanta {

using CommandHandler = std::function<Value(const Value&)>;

class CommandRegistry {
public:
    virtual ~CommandRegistry() = default;
    virtual RegistrationHandle RegisterCommand(const std::string& id, CommandHandler handler) = 0;
    virtual std::optional<Value> Execute(const std::string& id, const Value& arguments) const = 0;
    virtual std::vector<std::string> List() const = 0;
};

}
