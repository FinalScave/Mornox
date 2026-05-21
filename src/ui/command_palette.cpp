#include "ui/command_palette.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

namespace vanta {

namespace {

std::string Lowercase(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool Matches(const CommandPaletteItem& item, const std::string& query) {
    if (query.empty()) {
        return true;
    }
    const std::string needle = Lowercase(query);
    return Lowercase(item.id).find(needle) != std::string::npos ||
           Lowercase(item.title).find(needle) != std::string::npos ||
           Lowercase(item.source).find(needle) != std::string::npos;
}

}

void KeybindingRegistry::Bind(Keybinding binding) {
    if (binding.command_id.empty()) {
        return;
    }
    bindings_[binding.command_id] = std::move(binding);
}

void KeybindingRegistry::Unbind(const std::string& command_id) {
    bindings_.erase(command_id);
}

std::optional<Keybinding> KeybindingRegistry::BindingForCommand(const std::string& command_id) const {
    auto it = bindings_.find(command_id);
    return it == bindings_.end() ? std::nullopt : std::optional<Keybinding>(it->second);
}

std::vector<Keybinding> KeybindingRegistry::List() const {
    std::vector<Keybinding> result;
    for (const auto& [id, binding] : bindings_) {
        (void)id;
        result.push_back(binding);
    }
    return result;
}

std::vector<CommandPaletteItem> CommandPaletteItems(
    const CommandRegistry& commands,
    const std::vector<PluginRegistration>& contributions,
    const KeybindingRegistry& keybindings) {
    std::vector<CommandPaletteItem> result;
    std::set<std::string> seen;

    for (const PluginRegistration& contribution : contributions) {
        if (contribution.kind != PluginRegistrationKind::Command) {
            continue;
        }
        CommandPaletteItem item;
        item.id = contribution.id;
        item.title = contribution.title.empty() ? contribution.id : contribution.title;
        item.source = contribution.plugin_id;
        if (auto binding = keybindings.BindingForCommand(item.id)) {
            item.keybinding = binding->key;
        }
        result.push_back(std::move(item));
        seen.insert(contribution.id);
    }

    for (const std::string& id : commands.List()) {
        if (seen.contains(id)) {
            continue;
        }
        CommandPaletteItem item;
        item.id = id;
        item.title = id;
        item.source = "runtime";
        if (auto binding = keybindings.BindingForCommand(item.id)) {
            item.keybinding = binding->key;
        }
        result.push_back(std::move(item));
    }

    std::sort(result.begin(), result.end(), [](const CommandPaletteItem& left, const CommandPaletteItem& right) {
        return left.title < right.title;
    });
    return result;
}

std::vector<CommandPaletteItem> FilterCommandPaletteItems(const std::vector<CommandPaletteItem>& items, const std::string& query) {
    std::vector<CommandPaletteItem> result;
    for (const CommandPaletteItem& item : items) {
        if (Matches(item, query)) {
            result.push_back(item);
        }
    }
    return result;
}

}
