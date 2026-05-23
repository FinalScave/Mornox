#pragma once

#include <memory>

#include "mornox/plugin/core_plugin.h"

namespace mornox::builtin {

std::unique_ptr<CoreExtension> CreateLanguagesCoreExtension();
std::unique_ptr<CoreExtension> CreateCppCoreExtension();
std::unique_ptr<CoreExtension> CreatePythonCoreExtension();
std::unique_ptr<CoreExtension> CreateCMakeCoreExtension();
std::unique_ptr<CoreExtension> CreateGitCoreExtension();
std::unique_ptr<CoreExtension> CreateCliceCoreExtension(CorePluginDependencies dependencies);

}
