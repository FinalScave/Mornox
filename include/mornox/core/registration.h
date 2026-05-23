#pragma once

#include <functional>

namespace mornox {

class RegistrationHandle {
public:
    RegistrationHandle() = default;
    explicit RegistrationHandle(std::function<void()> unregister);
    RegistrationHandle(const RegistrationHandle&) = delete;
    RegistrationHandle& operator=(const RegistrationHandle&) = delete;
    RegistrationHandle(RegistrationHandle&& other) noexcept;
    RegistrationHandle& operator=(RegistrationHandle&& other) noexcept;
    ~RegistrationHandle();

    void Unregister() noexcept;
    bool Registered() const noexcept;

private:
    std::function<void()> unregister_callback_;
    bool registered_ = false;
};

}
