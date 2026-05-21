#pragma once

#include <string>
#include <vector>

#include "vanta/plugin/permissions.h"

namespace vanta {

class WorkspaceTrustService;

enum class ApprovalDecision {
    Allow,
    Deny,
};

struct ApprovalRequest {
    std::string subject;
    Permission permission = Permission::WorkspaceRead;
    std::string action;
    bool high_risk = false;
};

class ApprovalService {
public:
    void SetWorkspaceTrust(const WorkspaceTrustService* trust);
    void SetAutoApprove(bool auto_approve);
    ApprovalDecision RequestApproval(const ApprovalRequest& request);
    const std::vector<ApprovalRequest>& History() const;

private:
    const WorkspaceTrustService* trust_ = nullptr;
    bool auto_approve_ = true;
    std::vector<ApprovalRequest> history_;
};

}
