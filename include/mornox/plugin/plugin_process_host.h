#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

#include "mornox/platform/process.h"
#include "mornox/plugin/plugin_manager.h"
#include "mornox/plugin/plugin_protocol.h"

namespace mornox {

struct PluginProcessHealth {
    bool running = false;
    bool responsive = true;
    int failed_requests = 0;
    int crash_count = 0;
    std::optional<int> exit_code;
    std::string last_error;
};

class PluginProcessHost {
public:
    bool Start(const PluginManifest& manifest, const std::filesystem::path& workspace_root, std::string* error_message = nullptr);
    bool Running() const;
    void Stop();
    std::optional<PluginRpcResponse> Activate(const PluginManifest& manifest, const std::filesystem::path& workspace_root);
    std::optional<PluginRpcResponse> Deactivate(const std::string& plugin_id);
    std::optional<PluginRpcResponse> SendRequest(std::string method, std::string params_json = "{}");
    PluginProcessHealth Health();

private:
    void RecordFailure(std::string error);
    void RecordSuccess();

    int next_request_id_ = 1;
    ChildProcess process_;
    std::mutex rpc_mutex_;
    PluginProcessHealth health_;
};

}
