#include "plugin/contribution_registry.h"

#include <utility>

namespace vanta {

RegistrationHandle ContributionRegistry::RegisterContribution(PluginRegistration contribution) {
    if (contribution.id.empty()) {
        return {};
    }
    const std::string id = contribution.id;
    contributions_[id] = std::move(contribution);
    return RegistrationHandle([this, id] {
        Remove(id);
    });
}

void ContributionRegistry::Remove(const std::string& id) {
    contributions_.erase(id);
}

void ContributionRegistry::RemovePlugin(const std::string& plugin_id) {
    for (auto it = contributions_.begin(); it != contributions_.end();) {
        if (it->second.plugin_id == plugin_id) {
            it = contributions_.erase(it);
        } else {
            ++it;
        }
    }
}

void ContributionRegistry::Clear() {
    contributions_.clear();
}

std::optional<PluginRegistration> ContributionRegistry::Contribution(const std::string& id) const {
    auto it = contributions_.find(id);
    return it == contributions_.end() ? std::nullopt : std::optional<PluginRegistration>(it->second);
}

std::vector<PluginRegistration> ContributionRegistry::List() const {
    std::vector<PluginRegistration> result;
    for (const auto& [id, contribution] : contributions_) {
        (void)id;
        result.push_back(contribution);
    }
    return result;
}

std::vector<PluginRegistration> ContributionRegistry::List(PluginRegistrationKind kind) const {
    std::vector<PluginRegistration> result;
    for (const auto& [id, contribution] : contributions_) {
        (void)id;
        if (contribution.kind == kind) {
            result.push_back(contribution);
        }
    }
    return result;
}

std::vector<PluginRegistration> ContributionRegistry::ListByPlugin(const std::string& plugin_id) const {
    std::vector<PluginRegistration> result;
    for (const auto& [id, contribution] : contributions_) {
        (void)id;
        if (contribution.plugin_id == plugin_id) {
            result.push_back(contribution);
        }
    }
    return result;
}

std::optional<PluginRegistrationKind> ContributionKindFromString(const std::string& value) {
    if (value == "command" || value == "commands") {
        return PluginRegistrationKind::Command;
    }
    if (value == "view" || value == "views") {
        return PluginRegistrationKind::View;
    }
    if (value == "menu" || value == "menus") {
        return PluginRegistrationKind::Menu;
    }
    if (value == "languageService" || value == "languageServices") {
        return PluginRegistrationKind::LanguageService;
    }
    if (value == "fileSystemProvider" || value == "fileSystemProviders") {
        return PluginRegistrationKind::FileSystemProvider;
    }
    if (value == "agentTool" || value == "agentTools") {
        return PluginRegistrationKind::AgentTool;
    }
    if (value == "agentContextProvider" || value == "agentContextProviders") {
        return PluginRegistrationKind::AgentContextProvider;
    }
    if (value == "runConfiguration" || value == "runConfigurations") {
        return PluginRegistrationKind::RunConfiguration;
    }
    if (value == "diagnosticProvider" || value == "diagnosticProviders") {
        return PluginRegistrationKind::DiagnosticProvider;
    }
    if (value == "component" || value == "components") {
        return PluginRegistrationKind::Component;
    }
    return std::nullopt;
}

}
