#include "netdisk/control_plane/policy_manager.h"

namespace netdisk {

PolicyManager::PolicyManager(sync::MetadataStore &store) : store_(store) {}

Status PolicyManager::UpsertBackupPolicy(const sync::BackupPolicy &policy) {
    store_.UpsertBackupPolicy(policy);
    return Status::Ok();
}

Status PolicyManager::UpsertTransferPolicy(const sync::TransferPolicy &policy) {
    store_.UpsertTransferPolicy(policy);
    return Status::Ok();
}

std::optional<sync::BackupPolicy> PolicyManager::FindBackupPolicy(const std::string &policy_id) const {
    return store_.GetBackupPolicy(policy_id);
}

std::optional<sync::TransferPolicy> PolicyManager::FindTransferPolicy(const std::string &policy_id) const {
    return store_.GetTransferPolicy(policy_id);
}

std::vector<sync::BackupPolicy> PolicyManager::ListBackupPolicies() const {
    return store_.ListBackupPolicies();
}

std::vector<sync::TransferPolicy> PolicyManager::ListTransferPolicies() const {
    return store_.ListTransferPolicies();
}

std::vector<sync::BackupPolicy> PolicyManager::ListBackupPoliciesForNode(const std::string &node_id) const {
    std::vector<sync::BackupPolicy> result;
    for (const auto &policy : store_.ListBackupPolicies()) {
        if (policy.node_id == node_id) {
            result.push_back(policy);
        }
    }
    return result;
}

}  // namespace netdisk
