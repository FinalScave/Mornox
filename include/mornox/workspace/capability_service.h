#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "mornox/core/event.h"
#include "mornox/core/value.h"

namespace mornox {

enum class CapabilityStatus {
    Initializing,
    Available,
    Degraded,
    Unavailable,
};

struct Capability {
    std::string id;
    std::string title;
    std::string provider_id;
    CapabilityStatus status = CapabilityStatus::Unavailable;
    std::string message;
    std::vector<std::pair<std::string, std::string>> details;
};

struct CapabilityChangeEvent {
    Capability capability;
};

class CapabilityRegistry {
public:
    static constexpr const char* kServiceId = "mornox.capabilities";

    void Set(Capability capability);
    bool Remove(const std::string& id);
    std::optional<Capability> Get(const std::string& id) const;
    std::vector<Capability> Capabilities() const;
    bool Available(const std::string& id) const;
    void Clear();
    std::uint64_t OnDidChangeCapability(EventBus<CapabilityChangeEvent>::Listener listener);
    void RemoveCapabilityListener(std::uint64_t listener_id);

private:
    void Publish(const Capability& capability);

    std::map<std::string, Capability> capabilities_;
    EventBus<CapabilityChangeEvent> on_did_change_;
};

std::string ToString(CapabilityStatus status);

}
