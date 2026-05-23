#include "mornox/core/registration.h"

#include <utility>

namespace mornox {

RegistrationHandle::RegistrationHandle(std::function<void()> unregister)
    : unregister_callback_(std::move(unregister)), registered_(true) {}

RegistrationHandle::RegistrationHandle(RegistrationHandle&& other) noexcept {
    *this = std::move(other);
}

RegistrationHandle& RegistrationHandle::operator=(RegistrationHandle&& other) noexcept {
    if (this != &other) {
        Unregister();
        unregister_callback_ = std::move(other.unregister_callback_);
        registered_ = other.registered_;
        other.registered_ = false;
    }
    return *this;
}

RegistrationHandle::~RegistrationHandle() {
    // Registration handles are explicit lifecycle tokens. Call Unregister when
    // a registration should be removed.
}

void RegistrationHandle::Unregister() noexcept {
    if (!registered_) {
        return;
    }
    registered_ = false;
    if (unregister_callback_) {
        unregister_callback_();
    }
}

bool RegistrationHandle::Registered() const noexcept {
    return registered_;
}

}
