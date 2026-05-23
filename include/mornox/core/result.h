#pragma once

#include <optional>
#include <string>
#include <utility>

namespace mornox {

struct Error {
    std::string code;
    std::string message;
};

template <typename T>
class Result {
public:
    static Result Success(T value) {
        Result result;
        result.value_ = std::move(value);
        return result;
    }

    static Result Failure(std::string code, std::string message) {
        Result result;
        result.error_ = Error{std::move(code), std::move(message)};
        return result;
    }

    bool Ok() const {
        return value_.has_value();
    }

    explicit operator bool() const {
        return Ok();
    }

    const T& Value() const {
        return *value_;
    }

    T& Value() {
        return *value_;
    }

    const Error& ErrorValue() const {
        return *error_;
    }

private:
    std::optional<T> value_;
    std::optional<Error> error_;
};

template <>
class Result<void> {
public:
    static Result Success() {
        return {};
    }

    static Result Failure(std::string code, std::string message) {
        Result result;
        result.error_ = Error{std::move(code), std::move(message)};
        return result;
    }

    bool Ok() const {
        return !error_.has_value();
    }

    explicit operator bool() const {
        return Ok();
    }

    const Error& ErrorValue() const {
        return *error_;
    }

private:
    std::optional<Error> error_;
};

}
