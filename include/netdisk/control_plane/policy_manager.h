#ifndef NETDISK_CONTROL_PLANE_POLICY_MANAGER_H
#define NETDISK_CONTROL_PLANE_POLICY_MANAGER_H

#include <optional>
#include <vector>

#include "netdisk/common/status.h"
#include "netdisk/core/types.h"
#include "netdisk/metadata/metadata_store.h"

namespace netdisk {

class PolicyManager {
public:
    explicit PolicyManager(sync::MetadataStore &store);

    Status UpsertBackupPolicy(const sync::BackupPolicy &policy);
    Status UpsertTransferPolicy(const sync::TransferPolicy &policy);
    std::optional<sync::BackupPolicy> FindBackupPolicy(const std::string &policy_id) const;
    std::optional<sync::TransferPolicy> FindTransferPolicy(const std::string &policy_id) const;
    std::vector<sync::BackupPolicy> ListBackupPolicies() const;
    std::vector<sync::TransferPolicy> ListTransferPolicies() const;
    std::vector<sync::BackupPolicy> ListBackupPoliciesForNode(const std::string &node_id) const;

private:
    sync::MetadataStore &store_;
};

}  // namespace netdisk

#endif
