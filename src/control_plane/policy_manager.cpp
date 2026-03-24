#include "netdisk/control_plane/policy_manager.h"

namespace netdisk {

Status PolicyManager::UpsertBackupPolicy(const sync::BackupPolicy &policy) {
    for (auto &current : backup_policies_) {
        if (current.policy_id == policy.policy_id) {
            current = policy;
            return Status::Ok();
        }
    }
    backup_policies_.push_back(policy);
    return Status::Ok();
}

Status PolicyManager::UpsertTransferPolicy(const sync::TransferPolicy &policy) {
    for (auto &current : transfer_policies_) {
        if (current.policy_id == policy.policy_id) {
            current = policy;
            return Status::Ok();
        }
    }
    transfer_policies_.push_back(policy);
    return Status::Ok();
}

std::optional<sync::BackupPolicy> PolicyManager::FindBackupPolicy(const std::string &policy_id) const {
    for (const auto &policy : backup_policies_) {
        if (policy.policy_id == policy_id) {
            return policy;
        }
    }
    return std::nullopt;
}

std::optional<sync::TransferPolicy> PolicyManager::FindTransferPolicy(const std::string &policy_id) const {
    for (const auto &policy : transfer_policies_) {
        if (policy.policy_id == policy_id) {
            return policy;
        }
    }
    return std::nullopt;
}

std::vector<sync::BackupPolicy> PolicyManager::ListBackupPolicies() const {
    return backup_policies_;
}

std::vector<sync::TransferPolicy> PolicyManager::ListTransferPolicies() const {
    return transfer_policies_;
}

std::vector<sync::BackupPolicy> PolicyManager::ListBackupPoliciesForNode(const std::string &node_id) const {
    std::vector<sync::BackupPolicy> result;
    for (const auto &policy : backup_policies_) {
        if (policy.node_id == node_id) {
            result.push_back(policy);
        }
    }
    return result;
}

}  // namespace netdisk
