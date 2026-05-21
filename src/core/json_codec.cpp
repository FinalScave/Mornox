#include "vanta/core/json_codec.h"

#include <nlohmann/json.hpp>

#include <exception>
#include <utility>

namespace vanta {
namespace {

nlohmann::json JsonFromValue(const Value& value) {
    switch (value.GetKind()) {
    case Value::Kind::Null:
        return nullptr;
    case Value::Kind::Bool:
        return value.AsBool();
    case Value::Kind::Int:
        return value.AsInt();
    case Value::Kind::Double:
        return value.AsDouble();
    case Value::Kind::String:
        return value.AsString();
    case Value::Kind::Array: {
        nlohmann::json array = nlohmann::json::array();
        for (const Value& item : value.AsArray()) {
            array.push_back(JsonFromValue(item));
        }
        return array;
    }
    case Value::Kind::Object: {
        nlohmann::json object = nlohmann::json::object();
        for (const auto& [key, item] : value.AsObject()) {
            object[key] = JsonFromValue(item);
        }
        return object;
    }
    }
    return nullptr;
}

Value ValueFromNlohmann(const nlohmann::json& json) {
    if (json.is_null()) {
        return Value(nullptr);
    }
    if (json.is_boolean()) {
        return Value(json.get<bool>());
    }
    if (json.is_number_integer()) {
        return Value(static_cast<std::int64_t>(json.get<std::int64_t>()));
    }
    if (json.is_number_unsigned()) {
        return Value(static_cast<std::int64_t>(json.get<std::uint64_t>()));
    }
    if (json.is_number_float()) {
        return Value(json.get<double>());
    }
    if (json.is_string()) {
        return Value(json.get<std::string>());
    }
    if (json.is_array()) {
        Value::Array array;
        for (const nlohmann::json& item : json) {
            array.push_back(ValueFromNlohmann(item));
        }
        return Value::ArrayValue(std::move(array));
    }
    if (json.is_object()) {
        Value::Object object;
        for (auto it = json.begin(); it != json.end(); ++it) {
            object[it.key()] = ValueFromNlohmann(it.value());
        }
        return Value::ObjectValue(std::move(object));
    }
    return Value(nullptr);
}

}

std::string ValueToJsonText(const Value& value) {
    return JsonFromValue(value).dump();
}

Result<Value> ValueFromJsonText(std::string_view text) {
    try {
        return Result<Value>::Success(ValueFromNlohmann(nlohmann::json::parse(text)));
    } catch (const std::exception& error) {
        return Result<Value>::Failure("json.parse", error.what());
    }
}

}
