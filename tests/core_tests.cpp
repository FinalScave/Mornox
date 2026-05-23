#include "test_support.h"

#include "mornox/core/json_codec.h"
#include "mornox/core/localization.h"

namespace mornox::tests {

void TestJsonTextCodec() {
    const mornox::Result<mornox::Value> parsed = mornox::ValueFromJsonText(R"({"name":"Mornox","items":[1,true,null]})");
    REQUIRE(parsed);
    REQUIRE(parsed.Value().IsObject());
    REQUIRE(parsed.Value()["name"].AsString() == "Mornox");
    REQUIRE(parsed.Value()["items"].AsArray().size() == 3);
    REQUIRE(mornox::ValueToJsonText(parsed.Value()).find("Mornox") != std::string::npos);
}

void TestValueJsonCodec() {
    const mornox::Value value = mornox::Value::ObjectValue({
        {"name", mornox::Value("Mornox")},
        {"items", mornox::Value::ArrayValue({mornox::Value(1), mornox::Value(true), mornox::Value(nullptr)})},
    });
    const std::string text = mornox::ValueToJsonText(value);
    REQUIRE(text.find("Mornox") != std::string::npos);

    const mornox::Result<mornox::Value> decoded = mornox::ValueFromJsonText(text);
    REQUIRE(decoded);
    REQUIRE(decoded.Value().StringValue("name").value_or("") == "Mornox");
    REQUIRE(decoded.Value()["items"].AsArray().size() == 3);
    REQUIRE(decoded.Value()["items"].At(1).AsBool());
}

void TestLocalizationRegistry() {
    mornox::LocalizationRegistry registry;
    auto en = mornox::ParseLocalizationProperties("mornox.core", "en-US", R"(
        command.build.title=Build
        job.build.progress=Building {}: {}%
        brace.sample={{}} {}
    )");
    REQUIRE(en);
    auto zh = mornox::ParseLocalizationProperties("mornox.core", "zh-CN", R"(
        command.build.title=构建
        job.build.progress=正在构建 {}：{}%
    )");
    REQUIRE(zh);

    auto en_registration = registry.RegisterCatalog(std::move(en.Value()));
    auto zh_registration = registry.RegisterCatalog(std::move(zh.Value()));
    REQUIRE(en_registration.Registered());
    REQUIRE(zh_registration.Registered());

    const mornox::Localizer i18n = registry.LocalizerForOwner("mornox.core");
    REQUIRE(i18n.Resolve("command.build.title", {}, "zh-CN") == "构建");
    REQUIRE(i18n.Resolve("job.build.progress", {mornox::Value("all"), mornox::Value(42)}, "zh-CN") == "正在构建 all：42%");
    REQUIRE(i18n.Resolve("brace.sample", {mornox::Value("ok")}, "zh-CN") == "{} ok");
    REQUIRE(i18n.Resolve("missing.key", {}, "zh-CN") == "missing.key");
}

}

TEST_CASE("Value", "[core]") {
    mornox::tests::TestJsonTextCodec();
}

TEST_CASE("Value JSON codec", "[core]") {
    mornox::tests::TestValueJsonCodec();
}

TEST_CASE("Localization registry", "[core]") {
    mornox::tests::TestLocalizationRegistry();
}
