#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "vanta/plugin/permissions.h"
#include "vanta/core/event.h"
#include "vanta/core/value.h"

namespace vanta {

enum class WorkspaceTrustLevel {
    Trusted,
    Restricted,
    Untrusted,
};

struct WorkspaceTrustPolicy {
    WorkspaceTrustLevel level = WorkspaceTrustLevel::Trusted;
    bool allow_remembered_approvals = true;
    std::vector<Permission> blocked_permissions;
    std::vector<Permission> high_risk_blocked_permissions;
};

struct WorkspaceTrustChangeEvent {
    WorkspaceTrustLevel level = WorkspaceTrustLevel::Trusted;
    WorkspaceTrustPolicy policy;
};

class WorkspaceTrustService {
public:
    WorkspaceTrustService();

    void SetLevel(WorkspaceTrustLevel level);
    WorkspaceTrustLevel Level() const;
    WorkspaceTrustPolicy Policy() const;
    bool Trusted() const;
    bool Allows(Permission permission, bool high_risk = false) const;
    bool CanRememberApprovals() const;
    std::string DenialReason(Permission permission, bool high_risk = false) const;

    std::uint64_t OnDidChangeTrust(EventBus<WorkspaceTrustChangeEvent>::Listener listener);
    void RemoveTrustListener(std::uint64_t listener_id);

private:
    void ApplyDefaultPolicy();
    void Publish();

    WorkspaceTrustPolicy policy_;
    EventBus<WorkspaceTrustChangeEvent> on_did_change_;
};

std::string ToString(WorkspaceTrustLevel level);
std::optional<WorkspaceTrustLevel> WorkspaceTrustLevelFromString(const std::string& value);

}
