#include "test_support.h"

#include "vanta/core/json_codec.h"
#include "vanta/core/localization.h"

namespace vanta::tests {

void TestJsonTextCodec() {
    const vanta::Result<vanta::Value> parsed = vanta::ValueFromJsonText(R"({"name":"Vanta","items":[1,true,null]})");
    REQUIRE(parsed);
    REQUIRE(parsed.Value().IsObject());
    REQUIRE(parsed.Value()["name"].AsString() == "Vanta");
    REQUIRE(parsed.Value()["items"].AsArray().size() == 3);
    REQUIRE(vanta::ValueToJsonText(parsed.Value()).find("Vanta") != std::string::npos);
}

void TestValueJsonCodec() {
    const vanta::Value value = vanta::Value::ObjectValue({
        {"name", vanta::Value("Vanta")},
        {"items", vanta::Value::ArrayValue({vanta::Value(1), vanta::Value(true), vanta::Value(nullptr)})},
    });
    const std::string text = vanta::ValueToJsonText(value);
    REQUIRE(text.find("Vanta") != std::string::npos);

    const vanta::Result<vanta::Value> decoded = vanta::ValueFromJsonText(text);
    REQUIRE(decoded);
    REQUIRE(decoded.Value().StringValue("name").value_or("") == "Vanta");
    REQUIRE(decoded.Value()["items"].AsArray().size() == 3);
    REQUIRE(decoded.Value()["items"].At(1).AsBool());
}

void TestLocalizationRegistry() {
    vanta::LocalizationRegistry registry;
    auto en = vanta::ParseLocalizationProperties("vanta.core", "en-US", R"(
        command.build.title=Build
        job.build.progress=Building {}: {}%
        brace.sample={{}} {}
    )");
    REQUIRE(en);
    auto zh = vanta::ParseLocalizationProperties("vanta.core", "zh-CN", R"(
        command.build.title=构建
        job.build.progress=正在构建 {}：{}%
    )");
    REQUIRE(zh);

    auto en_registration = registry.RegisterCatalog(std::move(en.Value()));
    auto zh_registration = registry.RegisterCatalog(std::move(zh.Value()));
    REQUIRE(en_registration.Registered());
    REQUIRE(zh_registration.Registered());

    const vanta::Localizer i18n = registry.LocalizerForOwner("vanta.core");
    REQUIRE(i18n.Resolve("command.build.title", {}, "zh-CN") == "构建");
    REQUIRE(i18n.Resolve("job.build.progress", {vanta::Value("all"), vanta::Value(42)}, "zh-CN") == "正在构建 all：42%");
    REQUIRE(i18n.Resolve("brace.sample", {vanta::Value("ok")}, "zh-CN") == "{} ok");
    REQUIRE(i18n.Resolve("missing.key", {}, "zh-CN") == "missing.key");
}

}

TEST_CASE("Value", "[core]") {
    vanta::tests::TestJsonTextCodec();
}

TEST_CASE("Value JSON codec", "[core]") {
    vanta::tests::TestValueJsonCodec();
}

TEST_CASE("Localization registry", "[core]") {
    vanta::tests::TestLocalizationRegistry();
}
