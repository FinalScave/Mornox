#include "vanta/plugin/approval_service.h"

#include "vanta/workspace/workspace_trust.h"

namespace vanta {

void ApprovalService::SetWorkspaceTrust(const WorkspaceTrustService* trust) {
    trust_ = trust;
}

void ApprovalService::SetAutoApprove(bool auto_approve) {
    auto_approve_ = auto_approve;
}

ApprovalDecision ApprovalService::RequestApproval(const ApprovalRequest& request) {
    history_.push_back(request);
    if (trust_ != nullptr && !trust_->Allows(request.permission, request.high_risk)) {
        return ApprovalDecision::Deny;
    }
    return auto_approve_ ? ApprovalDecision::Allow : ApprovalDecision::Deny;
}

const std::vector<ApprovalRequest>& ApprovalService::History() const {
    return history_;
}

}
