#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "vanta/core/registration.h"
#include "vanta/plugin/plugin_protocol.h"

namespace vanta {

class ContributionRegistry {
public:
    RegistrationHandle RegisterContribution(PluginRegistration contribution);
    void Remove(const std::string& id);
    void RemovePlugin(const std::string& plugin_id);
    void Clear();

    std::optional<PluginRegistration> Contribution(const std::string& id) const;
    std::vector<PluginRegistration> List() const;
    std::vector<PluginRegistration> List(PluginRegistrationKind kind) const;
    std::vector<PluginRegistration> ListByPlugin(const std::string& plugin_id) const;

private:
    std::map<std::string, PluginRegistration> contributions_;
};

std::optional<PluginRegistrationKind> ContributionKindFromString(const std::string& value);

}
