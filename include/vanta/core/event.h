#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

namespace vanta {

template <typename Event>
class EventBus {
public:
    using Listener = std::function<void(const Event&)>;

    std::uint64_t Subscribe(Listener listener) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::uint64_t id = next_id_++;
        listeners_[id] = std::move(listener);
        return id;
    }

    void Unsubscribe(std::uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        listeners_.erase(id);
    }

    void Publish(const Event& event) const {
        std::vector<Listener> listeners;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [id, listener] : listeners_) {
                (void)id;
                listeners.push_back(listener);
            }
        }
        for (const Listener& listener : listeners) {
            listener(event);
        }
    }

private:
    mutable std::mutex mutex_;
    mutable std::map<std::uint64_t, Listener> listeners_;
    std::uint64_t next_id_ = 1;
};

}
