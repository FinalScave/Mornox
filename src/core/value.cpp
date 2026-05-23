#include "mornox/core/value.h"

#include <stdexcept>
#include <utility>

namespace mornox {
namespace {

const Value& NullValue() {
    static const Value value;
    return value;
}

}

Value::Value()
    : value_(nullptr) {}

Value::Value(std::nullptr_t)
    : value_(nullptr) {}

Value::Value(bool value)
    : value_(value) {}

Value::Value(int value)
    : value_(static_cast<std::int64_t>(value)) {}

Value::Value(std::int64_t value)
    : value_(value) {}

Value::Value(double value)
    : value_(value) {}

Value::Value(const char* value)
    : value_(std::string(value == nullptr ? "" : value)) {}

Value::Value(std::string value)
    : value_(std::move(value)) {}

Value::Value(Array value)
    : value_(std::move(value)) {}

Value::Value(Object value)
    : value_(std::move(value)) {}

Value::Value(Storage value)
    : value_(std::move(value)) {}

Value Value::ObjectValue(Object value) {
    return Value(std::move(value));
}

Value Value::ArrayValue(Array value) {
    return Value(std::move(value));
}

Value::Kind Value::GetKind() const {
    switch (value_.index()) {
    case 0:
        return Kind::Null;
    case 1:
        return Kind::Bool;
    case 2:
        return Kind::Int;
    case 3:
        return Kind::Double;
    case 4:
        return Kind::String;
    case 5:
        return Kind::Array;
    case 6:
        return Kind::Object;
    }
    return Kind::Null;
}

bool Value::IsNull() const {
    return std::holds_alternative<std::nullptr_t>(value_);
}

bool Value::IsBool() const {
    return std::holds_alternative<bool>(value_);
}

bool Value::IsInt() const {
    return std::holds_alternative<std::int64_t>(value_);
}

bool Value::IsDouble() const {
    return std::holds_alternative<double>(value_);
}

bool Value::IsNumber() const {
    return IsInt() || IsDouble();
}

bool Value::IsString() const {
    return std::holds_alternative<std::string>(value_);
}

bool Value::IsArray() const {
    return std::holds_alternative<Array>(value_);
}

bool Value::IsObject() const {
    return std::holds_alternative<Object>(value_);
}

bool Value::AsBool() const {
    return std::get<bool>(value_);
}

std::int64_t Value::AsInt() const {
    return std::get<std::int64_t>(value_);
}

double Value::AsDouble() const {
    return IsInt() ? static_cast<double>(AsInt()) : std::get<double>(value_);
}

const std::string& Value::AsString() const {
    return std::get<std::string>(value_);
}

const Value::Array& Value::AsArray() const {
    return std::get<Array>(value_);
}

const Value::Object& Value::AsObject() const {
    return std::get<Object>(value_);
}

Value::Array& Value::AsArray() {
    return std::get<Array>(value_);
}

Value::Object& Value::AsObject() {
    return std::get<Object>(value_);
}

const Value& Value::At(const std::string& key) const {
    if (!IsObject()) {
        return NullValue();
    }
    const auto& object = AsObject();
    auto it = object.find(key);
    return it == object.end() ? NullValue() : it->second;
}

const Value& Value::At(std::size_t index) const {
    if (!IsArray() || index >= AsArray().size()) {
        return NullValue();
    }
    return AsArray()[index];
}

const Value& Value::operator[](const std::string& key) const {
    return At(key);
}

Value& Value::operator[](const std::string& key) {
    if (!IsObject()) {
        value_ = Object{};
    }
    return std::get<Object>(value_)[key];
}

bool Value::Contains(const std::string& key) const {
    return IsObject() && AsObject().contains(key);
}

std::optional<std::string> Value::StringValue(const std::string& key) const {
    const Value& value = At(key);
    return value.IsString() ? std::optional<std::string>(value.AsString()) : std::nullopt;
}

std::optional<std::int64_t> Value::IntValue(const std::string& key) const {
    const Value& value = At(key);
    return value.IsInt() ? std::optional<std::int64_t>(value.AsInt()) : std::nullopt;
}

std::optional<double> Value::DoubleValue(const std::string& key) const {
    const Value& value = At(key);
    return value.IsNumber() ? std::optional<double>(value.AsDouble()) : std::nullopt;
}

std::optional<bool> Value::BoolValue(const std::string& key) const {
    const Value& value = At(key);
    return value.IsBool() ? std::optional<bool>(value.AsBool()) : std::nullopt;
}

}
