#pragma once

#include <functional>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "vanta/plugin/extension_context.h"

namespace vanta {

class CoreExtension {
public:
    virtual ~CoreExtension() = default;

    virtual void Activate(ExtensionContext& context) = 0;
    virtual void Deactivate();
};

using CoreExtensionFactory = std::function<std::unique_ptr<CoreExtension>()>;

class CorePluginRegistry {
public:
    void Add(std::string entry, CoreExtensionFactory factory);
    std::unique_ptr<CoreExtension> Create(const std::string& entry) const;
    std::vector<std::string> Entries() const;

private:
    std::map<std::string, CoreExtensionFactory> factories_;
};

struct ClicePluginConfig {
    std::filesystem::path server_path;
};

struct CorePluginDependencies {
    ClicePluginConfig clice;
};

CorePluginRegistry CreateDefaultCorePluginRegistry(CorePluginDependencies dependencies = {});

}
