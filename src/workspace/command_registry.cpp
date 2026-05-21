#include "workspace/command_registry_impl.h"

#include <utility>

namespace vanta {

RegistrationHandle internal::CommandRegistryImpl::RegisterCommand(const std::string& id, CommandHandler handler) {
    handlers_[id] = std::move(handler);
    return RegistrationHandle([this, id] {
        Unregister(id);
    });
}

std::optional<Value> internal::CommandRegistryImpl::Execute(const std::string& id, const Value& arguments) const {
    auto it = handlers_.find(id);
    if (it == handlers_.end()) {
        return std::nullopt;
    }
    return it->second(arguments);
}

std::vector<std::string> internal::CommandRegistryImpl::List() const {
    std::vector<std::string> result;
    for (const auto& [id, handler] : handlers_) {
        (void)handler;
        result.push_back(id);
    }
    return result;
}

void internal::CommandRegistryImpl::Unregister(const std::string& id) {
    handlers_.erase(id);
}

}
