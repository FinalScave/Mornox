#pragma once

#include <set>
#include <string>
#include <vector>

namespace vanta {

enum class Permission {
    WorkspaceRead,
    WorkspaceWrite,
    ProcessExecute,
    NetworkAccess,
    GitRead,
    GitWrite,
};

class PermissionSet {
public:
    static PermissionSet FromStrings(const std::vector<std::string>& permissions);

    void Add(Permission permission);
    bool Contains(Permission permission) const;

private:
    std::set<Permission> permissions_;
};

std::string ToString(Permission permission);

}
