#include "vanta/workspace/workspace_trust.h"

#include <algorithm>
#include <utility>

namespace vanta {
namespace {

bool ContainsPermission(const std::vector<Permission>& permissions, Permission permission) {
    return std::find(permissions.begin(), permissions.end(), permission) != permissions.end();
}

}

WorkspaceTrustService::WorkspaceTrustService() {
    ApplyDefaultPolicy();
}

void WorkspaceTrustService::SetLevel(WorkspaceTrustLevel level) {
    if (policy_.level == level) {
        return;
    }
    policy_.level = level;
    ApplyDefaultPolicy();
    Publish();
}

WorkspaceTrustLevel WorkspaceTrustService::Level() const {
    return policy_.level;
}

WorkspaceTrustPolicy WorkspaceTrustService::Policy() const {
    return policy_;
}

bool WorkspaceTrustService::Trusted() const {
    return policy_.level == WorkspaceTrustLevel::Trusted;
}

bool WorkspaceTrustService::Allows(Permission permission, bool high_risk) const {
    if (ContainsPermission(policy_.blocked_permissions, permission)) {
        return false;
    }
    return !high_risk || !ContainsPermission(policy_.high_risk_blocked_permissions, permission);
}

bool WorkspaceTrustService::CanRememberApprovals() const {
    return policy_.allow_remembered_approvals;
}

std::string WorkspaceTrustService::DenialReason(Permission permission, bool high_risk) const {
    if (Allows(permission, high_risk)) {
        return {};
    }
    return "Permission is blocked by workspace trust policy: " + ToString(permission);
}

std::uint64_t WorkspaceTrustService::OnDidChangeTrust(EventBus<WorkspaceTrustChangeEvent>::Listener listener) {
    return on_did_change_.Subscribe(std::move(listener));
}

void WorkspaceTrustService::RemoveTrustListener(std::uint64_t listener_id) {
    on_did_change_.Unsubscribe(listener_id);
}

void WorkspaceTrustService::ApplyDefaultPolicy() {
    policy_.blocked_permissions.clear();
    policy_.high_risk_blocked_permissions.clear();
    switch (policy_.level) {
    case WorkspaceTrustLevel::Trusted:
        policy_.allow_remembered_approvals = true;
        break;
    case WorkspaceTrustLevel::Restricted:
        policy_.allow_remembered_approvals = false;
        policy_.blocked_permissions = {Permission::NetworkAccess};
        policy_.high_risk_blocked_permissions = {
            Permission::WorkspaceWrite,
            Permission::ProcessExecute,
            Permission::GitWrite,
        };
        break;
    case WorkspaceTrustLevel::Untrusted:
        policy_.allow_remembered_approvals = false;
        policy_.blocked_permissions = {
            Permission::WorkspaceWrite,
            Permission::ProcessExecute,
            Permission::NetworkAccess,
            Permission::GitWrite,
        };
        break;
    }
}

void WorkspaceTrustService::Publish() {
    on_did_change_.Publish({
        .level = policy_.level,
        .policy = policy_,
    });
}

std::string ToString(WorkspaceTrustLevel level) {
    switch (level) {
    case WorkspaceTrustLevel::Trusted:
        return "trusted";
    case WorkspaceTrustLevel::Restricted:
        return "restricted";
    case WorkspaceTrustLevel::Untrusted:
        return "untrusted";
    }
    return "trusted";
}

std::optional<WorkspaceTrustLevel> WorkspaceTrustLevelFromString(const std::string& value) {
    if (value == "trusted") {
        return WorkspaceTrustLevel::Trusted;
    }
    if (value == "restricted") {
        return WorkspaceTrustLevel::Restricted;
    }
    if (value == "untrusted") {
        return WorkspaceTrustLevel::Untrusted;
    }
    return std::nullopt;
}

}
