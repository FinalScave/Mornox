#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "vanta/plugin/plugin_protocol.h"
#include "vanta/workspace/command_registry.h"

namespace vanta {

struct CommandPaletteItem {
    std::string id;
    std::string title;
    std::string keybinding;
    std::string source;
    bool enabled = true;
};

struct Keybinding {
    std::string command_id;
    std::string key;
    std::string when;
};

class KeybindingRegistry {
public:
    void Bind(Keybinding binding);
    void Unbind(const std::string& command_id);
    std::optional<Keybinding> BindingForCommand(const std::string& command_id) const;
    std::vector<Keybinding> List() const;

private:
    std::map<std::string, Keybinding> bindings_;
};

std::vector<CommandPaletteItem> CommandPaletteItems(
    const CommandRegistry& commands,
    const std::vector<PluginRegistration>& contributions,
    const KeybindingRegistry& keybindings);
std::vector<CommandPaletteItem> FilterCommandPaletteItems(const std::vector<CommandPaletteItem>& items, const std::string& query);

}
