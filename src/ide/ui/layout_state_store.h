#pragma once

#include <map>
#include <string>
#include <vector>

#include "mornox/core/value.h"
#include "mornox/project/component.h"
#include "mornox/vfs/uri.h"

namespace mornox {

struct UiState;

struct LayoutState {
    std::vector<Uri> open_tabs;
    Uri active_file;
    bool project_view_visible = true;
    bool problems_visible = true;
    bool build_panel_visible = true;
    bool agent_panel_visible = true;
    bool git_panel_visible = true;
    std::string last_build_target;
    std::map<std::string, Value> plugin_state;
};

class LayoutStateStore final : public Component {
public:
    static constexpr const char* kComponentId = "mornox.ui.layout";

    std::string Id() const override;
    void RestoreState(const Value& state) override;
    Value SaveState() const override;

    void Capture(const UiState& ui);
    void RememberBuildTarget(std::string target);

    const LayoutState& State() const;
    LayoutState& State();

private:
    LayoutState state_;
};

}
