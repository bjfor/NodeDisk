#include "netdisk/storage/replica_manager.h"

namespace netdisk {

Status ReplicaManager::RecordReplica(const sync::ReplicaRecord &record) {
    for (auto &current : replicas_) {
        if (current.chunk_hash == record.chunk_hash && current.node_id == record.node_id) {
            current = record;
            return Status::Ok();
        }
    }
    replicas_.push_back(record);
    return Status::Ok();
}

Status ReplicaManager::UpdateReplicaState(const std::string &chunk_hash,
                                          const std::string &node_id,
                                          sync::ReplicaStatus status) {
    for (auto &record : replicas_) {
        if (record.chunk_hash == chunk_hash && record.node_id == node_id) {
            record.status = status;
            return Status::Ok();
        }
    }
    return Status::Error(StatusCode::kNotFound, "replica not found");
}

std::vector<sync::ReplicaRecord> ReplicaManager::ListReplicasForChunk(const std::string &chunk_hash) const {
    std::vector<sync::ReplicaRecord> result;
    for (const auto &record : replicas_) {
        if (record.chunk_hash == chunk_hash) {
            result.push_back(record);
        }
    }
    return result;
}

}  // namespace netdisk
