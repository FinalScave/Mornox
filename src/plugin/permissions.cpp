#include "vanta/plugin/permissions.h"

#include <map>

namespace vanta {
namespace {

const std::map<std::string, Permission>& permission_map() {
    static const std::map<std::string, Permission> permissions = {
        {"workspace.read", Permission::WorkspaceRead},
        {"workspace.write", Permission::WorkspaceWrite},
        {"process.execute", Permission::ProcessExecute},
        {"network.access", Permission::NetworkAccess},
        {"git.read", Permission::GitRead},
        {"git.write", Permission::GitWrite},
    };
    return permissions;
}

}

PermissionSet PermissionSet::FromStrings(const std::vector<std::string>& permissions) {
    PermissionSet result;
    for (const std::string& permission : permissions) {
        auto it = permission_map().find(permission);
        if (it != permission_map().end()) {
            result.Add(it->second);
        }
    }
    return result;
}

void PermissionSet::Add(Permission permission) {
    permissions_.insert(permission);
}

bool PermissionSet::Contains(Permission permission) const {
    return permissions_.contains(permission);
}

std::string ToString(Permission permission) {
    for (const auto& [name, value] : permission_map()) {
        if (value == permission) {
            return name;
        }
    }
    return "";
}

}
