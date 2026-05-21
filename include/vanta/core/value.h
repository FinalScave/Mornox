#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace vanta {

class Value {
public:
    enum class Kind {
        Null,
        Bool,
        Int,
        Double,
        String,
        Array,
        Object,
    };

    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value>;

    Value();
    Value(std::nullptr_t);
    Value(bool value);
    Value(int value);
    Value(std::int64_t value);
    Value(double value);
    Value(const char* value);
    Value(std::string value);
    Value(Array value);
    Value(Object value);

    static Value ObjectValue(Object value = {});
    static Value ArrayValue(Array value = {});

    Kind GetKind() const;
    bool IsNull() const;
    bool IsBool() const;
    bool IsInt() const;
    bool IsDouble() const;
    bool IsNumber() const;
    bool IsString() const;
    bool IsArray() const;
    bool IsObject() const;

    bool AsBool() const;
    std::int64_t AsInt() const;
    double AsDouble() const;
    const std::string& AsString() const;
    const Array& AsArray() const;
    const Object& AsObject() const;
    Array& AsArray();
    Object& AsObject();

    const Value& At(const std::string& key) const;
    const Value& At(std::size_t index) const;
    const Value& operator[](const std::string& key) const;
    Value& operator[](const std::string& key);
    bool Contains(const std::string& key) const;

    std::optional<std::string> StringValue(const std::string& key) const;
    std::optional<std::int64_t> IntValue(const std::string& key) const;
    std::optional<double> DoubleValue(const std::string& key) const;
    std::optional<bool> BoolValue(const std::string& key) const;

private:
    using Storage = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, Array, Object>;

    explicit Value(Storage value);

    Storage value_;
};

}
