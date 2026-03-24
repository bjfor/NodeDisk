#ifndef SYNC_TYPES_H
#define SYNC_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace sync {

enum class FileStatus : std::uint8_t {
    kPending = 0,
    kReady = 1,
    kDeleted = 2,
    kCorrupted = 3,
};

enum class ReplicaStatus : std::uint8_t {
    kUnknown = 0,
    kAvailable = 1,
    kMissing = 2,
    kCorrupted = 3,
};

enum class SyncTaskState : std::uint8_t {
    kQueued = 0,
    kRunning = 1,
    kVerifying = 2,
    kCommitted = 3,
    kFailed = 4,
};

enum class JobDomain : std::uint8_t {
    kBackup = 0,
    kTransfer = 1,
    kRestore = 2,
};

enum class JobState : std::uint8_t {
    kQueued = 0,
    kRunning = 1,
    kCompleted = 2,
    kFailed = 3,
};

struct NodeInfo {
    std::string node_id;
    std::string device_name;
    std::string zt_ip;
    std::uint64_t last_seen_epoch = 0;
};

struct ChunkRecord {
    std::string file_id;
    std::uint32_t chunk_index = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::string chunk_hash;
};

struct FileRecord {
    std::string file_id;
    std::string relative_path;
    std::string absolute_path;
    std::uint64_t size = 0;
    std::uint64_t modified_time_epoch = 0;
    std::uint32_t mode = 0;
    std::string root_hash;
    FileStatus status = FileStatus::kPending;
    std::vector<ChunkRecord> chunks;
};

struct ReplicaRecord {
    std::string chunk_hash;
    std::string node_id;
    std::string storage_path;
    std::uint64_t verified_at_epoch = 0;
    ReplicaStatus status = ReplicaStatus::kUnknown;
};

struct SyncTask {
    std::string task_id;
    std::string source_node;
    std::string target_node;
    std::string file_id;
    std::string relative_path;
    std::string expected_root_hash;
    std::string temp_path;
    std::uint32_t total_chunks = 0;
    std::uint32_t completed_chunks = 0;
    SyncTaskState state = SyncTaskState::kQueued;
    std::uint32_t retry_count = 0;
    std::string error_message;
};

struct SyncTaskChunk {
    std::string task_id;
    std::string file_id;
    std::uint32_t chunk_index = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::string chunk_hash;
};

struct StoredObjectRecord {
    std::string file_id;
    std::string backend_name;
    std::string storage_key;
    std::string access_url;
    std::string source_path;
    std::uint64_t stored_at_epoch = 0;
};

struct BackupRecord {
    std::string record_id;
    std::string job_id;
    std::string node_id;
    std::string file_id;
    std::string relative_path;
    std::string source_path;
    std::string backend_name;
    std::string storage_key;
    std::uint64_t backed_up_at_epoch = 0;
};

struct BackupPolicy {
    std::string policy_id;
    std::string node_id;
    std::string source_path;
    bool recursive = true;
    bool enabled = true;
    std::string schedule_expression;
    std::uint64_t created_at_epoch = 0;
};

struct TransferPolicy {
    std::string policy_id;
    std::uint64_t retention_seconds = 7 * 24 * 3600;
    bool auto_cleanup = true;
    bool private_library_only = true;
};

struct BackupJob {
    std::string job_id;
    std::string node_id;
    std::string source_path;
    std::string destination_label;
    std::uint64_t created_at_epoch = 0;
};

struct SharedLibraryEntry {
    std::string entry_id;
    std::string owner_node_id;
    std::string source_path;
    std::string file_id;
    std::string display_name;
    std::string note;
    std::uint64_t created_at_epoch = 0;
    std::uint64_t expires_at_epoch = 0;
    bool delivered = false;
    std::vector<std::string> recipient_nodes;
};

struct RestoreRequest {
    std::string request_id;
    std::string file_id;
    std::string source_node_id;
    std::string target_node_id;
    std::string destination_path;
    std::uint64_t created_at_epoch = 0;
};

struct ScheduledTaskRecord {
    std::string task_id;
    JobDomain domain = JobDomain::kBackup;
    std::string related_id;
    std::string source_node;
    std::string target_node;
    JobState state = JobState::kQueued;
    std::uint64_t created_at_epoch = 0;
    std::string detail;
};

struct AuthIdentity {
    std::string user_id;
    std::string username;
    std::string device_id;
};

struct AuthSession {
    std::string session_id;
    std::string user_id;
    std::string device_id;
    std::string access_token;
    std::uint64_t expires_at_epoch = 0;
};

struct OverlayPeer {
    std::string node_id;
    std::string virtual_ip;
    bool online = false;
    std::uint64_t last_seen_epoch = 0;
};

struct AuditEvent {
    std::string event_id;
    std::string category;
    std::string detail;
    std::uint64_t created_at_epoch = 0;
};

}  // namespace sync

#endif
