#ifndef NETDISK_STORAGE_REPLICA_MANAGER_H
#define NETDISK_STORAGE_REPLICA_MANAGER_H

#include <vector>

#include "netdisk/common/status.h"
#include "netdisk/core/types.h"

namespace netdisk {

class ReplicaManager {
public:
    Status RecordReplica(const sync::ReplicaRecord &record);
    Status UpdateReplicaState(const std::string &chunk_hash,
                              const std::string &node_id,
                              sync::ReplicaStatus status);
    std::vector<sync::ReplicaRecord> ListReplicasForChunk(const std::string &chunk_hash) const;

private:
    std::vector<sync::ReplicaRecord> replicas_;
};

}  // namespace netdisk

#endif
