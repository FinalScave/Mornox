#include "test_support.h"
#include "mornox/plugin/plugin_storage.h"

namespace mornox::tests {

void TestSettingsAndResult() {
    const auto root = MakeTempRoot();
    mornox::SettingsService settings;
    mornox::RegisterDefaultSettings(settings);
    const mornox::SettingScope ide_scope{.kind = mornox::SettingScopeKind::Ide};
    const mornox::SettingScope workspace_scope{.kind = mornox::SettingScopeKind::Workspace, .qualifier = root.string()};
    const mornox::SettingScope language_scope{.kind = mornox::SettingScopeKind::Language, .qualifier = "cpp"};
    int setting_events = 0;
    settings.OnDidChangeSetting([&](const mornox::SettingChangeEvent& event) {
        if (event.id == "editor.formatOnSave") {
            ++setting_events;
        }
    });
    REQUIRE(settings.SetValue("editor.fontSize", ide_scope, mornox::SettingValue::IntValue(16)));
    REQUIRE(settings.SetValue("editor.formatOnSave", workspace_scope, mornox::SettingValue::BoolValue(true)));
    REQUIRE(settings.SetValue("editor.formatOnSave", language_scope, mornox::SettingValue::BoolValue(false)));
    const mornox::SettingResolution resolved = settings.Resolve("editor.formatOnSave", {
        .workspace_id = root.string(),
        .language_id = "cpp",
    });
    REQUIRE(!resolved.defaulted);
    REQUIRE(resolved.source.kind == mornox::SettingScopeKind::Language);
    REQUIRE(std::get<bool>(resolved.value.data) == false);
    const auto scopes = settings.ScopesFor("editor.formatOnSave", {
        .workspace_id = root.string(),
        .language_id = "cpp",
    });
    REQUIRE(scopes.size() == 3);
    REQUIRE(std::any_of(scopes.begin(), scopes.end(), [](const mornox::SettingScopeDescriptor& scope) {
        return scope.scope.kind == mornox::SettingScopeKind::Language && scope.effective_source;
    }));
    const auto search_results = settings.Search("model");
    REQUIRE(!search_results.empty());
    REQUIRE(std::any_of(search_results.begin(), search_results.end(), [](const mornox::SettingSearchResult& result) {
        return result.setting_id == "ai.agent.model";
    }));
    REQUIRE(!settings.Children("ai").empty());
    REQUIRE(settings.Save(workspace_scope, root / ".mornox" / "settings.json"));

    mornox::SettingsService loaded;
    mornox::RegisterDefaultSettings(loaded);
    REQUIRE(loaded.Load(workspace_scope, root / ".mornox" / "settings.json"));
    const mornox::SettingResolution loaded_value = loaded.Resolve("editor.formatOnSave", {
        .workspace_id = root.string(),
        .language_id = "python",
    });
    REQUIRE(std::get<bool>(loaded_value.value.data));
    REQUIRE(setting_events == 2);

    mornox::PluginStorageService storage(root / ".mornox" / "plugin-storage");
    REQUIRE(storage.Write("sample.plugin", "state", mornox::Value::ObjectValue({{"ok", mornox::Value(true)}})));
    const auto state = storage.Read("sample.plugin", "state");
    REQUIRE(state);
    REQUIRE(state.Value()["ok"].AsBool());

    const auto error = mornox::Result<int>::Failure("sample", "failed");
    REQUIRE(!error);
    REQUIRE(error.ErrorValue().code == "sample");
}

}

TEST_CASE("Settings and result", "[settings][platform]") {
    mornox::tests::TestSettingsAndResult();
}
