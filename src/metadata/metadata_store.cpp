#include "netdisk/metadata/metadata_store.h"

#include <sqlite3.h>

#include <stdexcept>

namespace sync {

void InMemoryMetadataStore::UpsertNode(const NodeInfo &node) {
    for (auto &current : nodes_) {
        if (current.node_id == node.node_id) {
            current = node;
            return;
        }
    }
    nodes_.push_back(node);
}

void InMemoryMetadataStore::UpsertBackupPolicy(const BackupPolicy &policy) {
    for (auto &current : backup_policies_) {
        if (current.policy_id == policy.policy_id) {
            current = policy;
            return;
        }
    }
    backup_policies_.push_back(policy);
}

void InMemoryMetadataStore::UpsertTransferPolicy(const TransferPolicy &policy) {
    for (auto &current : transfer_policies_) {
        if (current.policy_id == policy.policy_id) {
            current = policy;
            return;
        }
    }
    transfer_policies_.push_back(policy);
}

void InMemoryMetadataStore::UpsertScheduledTask(const ScheduledTaskRecord &task) {
    for (auto &current : scheduled_tasks_) {
        if (current.task_id == task.task_id) {
            current = task;
            return;
        }
    }
    scheduled_tasks_.push_back(task);
}

void InMemoryMetadataStore::UpsertBackupJob(const BackupJob &job) {
    for (auto &current : backup_jobs_) {
        if (current.job_id == job.job_id) {
            current = job;
            return;
        }
    }
    backup_jobs_.push_back(job);
}

void InMemoryMetadataStore::UpsertRestoreRequest(const RestoreRequest &request) {
    for (auto &current : restore_requests_) {
        if (current.request_id == request.request_id) {
            current = request;
            return;
        }
    }
    restore_requests_.push_back(request);
}

void InMemoryMetadataStore::UpsertSharedLibraryEntry(const SharedLibraryEntry &entry) {
    for (auto &current : shared_library_entries_) {
        if (current.entry_id == entry.entry_id) {
            current = entry;
            return;
        }
    }
    shared_library_entries_.push_back(entry);
}

void InMemoryMetadataStore::UpsertFile(const FileRecord &file) {
    for (auto &current : files_) {
        if (current.file_id == file.file_id || current.absolute_path == file.absolute_path) {
            current = file;
            return;
        }
    }
    files_.push_back(file);
}

void InMemoryMetadataStore::UpsertTask(const SyncTask &task) {
    for (auto &current : tasks_) {
        if (current.task_id == task.task_id) {
            current = task;
            return;
        }
    }
    tasks_.push_back(task);
}

void InMemoryMetadataStore::MarkTaskChunkComplete(const SyncTaskChunk &chunk) {
    for (auto &current : task_chunks_) {
        if (current.task_id == chunk.task_id && current.chunk_index == chunk.chunk_index) {
            current = chunk;
            return;
        }
    }
    task_chunks_.push_back(chunk);
}

void InMemoryMetadataStore::ClearTaskChunks(const std::string &task_id) {
    std::vector<SyncTaskChunk> remaining;
    remaining.reserve(task_chunks_.size());
    for (const auto &chunk : task_chunks_) {
        if (chunk.task_id != task_id) {
            remaining.push_back(chunk);
        }
    }
    task_chunks_ = std::move(remaining);
}

void InMemoryMetadataStore::UpsertStoredObject(const StoredObjectRecord &record) {
    for (auto &current : stored_objects_) {
        if (current.file_id == record.file_id) {
            current = record;
            return;
        }
    }
    stored_objects_.push_back(record);
}

void InMemoryMetadataStore::UpsertBackupRecord(const BackupRecord &record) {
    for (auto &current : backup_records_) {
        if (current.record_id == record.record_id) {
            current = record;
            return;
        }
    }
    backup_records_.push_back(record);
}

void InMemoryMetadataStore::UpsertDeviceFile(const DeviceFileRecord &record) {
    for (auto &current : device_files_) {
        if (current.record_id == record.record_id ||
            (current.node_id == record.node_id && current.absolute_path == record.absolute_path)) {
            current = record;
            return;
        }
    }
    device_files_.push_back(record);
}

void InMemoryMetadataStore::AppendAuditEvent(const AuditEvent &event) {
    audit_events_.push_back(event);
}

std::optional<NodeInfo> InMemoryMetadataStore::GetNode(const std::string &node_id) const {
    for (const auto &node : nodes_) {
        if (node.node_id == node_id) {
            return node;
        }
    }
    return std::nullopt;
}

std::optional<BackupPolicy> InMemoryMetadataStore::GetBackupPolicy(const std::string &policy_id) const {
    for (const auto &policy : backup_policies_) {
        if (policy.policy_id == policy_id) {
            return policy;
        }
    }
    return std::nullopt;
}

std::optional<TransferPolicy> InMemoryMetadataStore::GetTransferPolicy(const std::string &policy_id) const {
    for (const auto &policy : transfer_policies_) {
        if (policy.policy_id == policy_id) {
            return policy;
        }
    }
    return std::nullopt;
}

std::optional<ScheduledTaskRecord> InMemoryMetadataStore::GetScheduledTaskRecord(const std::string &task_id) const {
    for (const auto &task : scheduled_tasks_) {
        if (task.task_id == task_id) {
            return task;
        }
    }
    return std::nullopt;
}

std::optional<BackupJob> InMemoryMetadataStore::GetBackupJob(const std::string &job_id) const {
    for (const auto &job : backup_jobs_) {
        if (job.job_id == job_id) {
            return job;
        }
    }
    return std::nullopt;
}

std::optional<RestoreRequest> InMemoryMetadataStore::GetRestoreRequest(const std::string &request_id) const {
    for (const auto &request : restore_requests_) {
        if (request.request_id == request_id) {
            return request;
        }
    }
    return std::nullopt;
}

std::optional<SharedLibraryEntry> InMemoryMetadataStore::GetSharedLibraryEntry(const std::string &entry_id) const {
    for (const auto &entry : shared_library_entries_) {
        if (entry.entry_id == entry_id) {
            return entry;
        }
    }
    return std::nullopt;
}

std::optional<FileRecord> InMemoryMetadataStore::GetFile(const std::string &file_id) const {
    for (const auto &file : files_) {
        if (file.file_id == file_id) {
            return file;
        }
    }
    return std::nullopt;
}

std::optional<FileRecord> InMemoryMetadataStore::GetFileByPath(const std::string &absolute_path) const {
    for (const auto &file : files_) {
        if (file.absolute_path == absolute_path) {
            return file;
        }
    }
    return std::nullopt;
}

std::optional<SyncTask> InMemoryMetadataStore::GetTask(const std::string &task_id) const {
    for (const auto &task : tasks_) {
        if (task.task_id == task_id) {
            return task;
        }
    }
    return std::nullopt;
}

std::optional<StoredObjectRecord> InMemoryMetadataStore::GetStoredObject(const std::string &file_id) const {
    for (const auto &record : stored_objects_) {
        if (record.file_id == file_id) {
            return record;
        }
    }
    return std::nullopt;
}

std::vector<NodeInfo> InMemoryMetadataStore::ListNodes() const {
    return nodes_;
}

std::vector<BackupPolicy> InMemoryMetadataStore::ListBackupPolicies() const {
    return backup_policies_;
}

std::vector<TransferPolicy> InMemoryMetadataStore::ListTransferPolicies() const {
    return transfer_policies_;
}

std::vector<ScheduledTaskRecord> InMemoryMetadataStore::ListScheduledTasks() const {
    return scheduled_tasks_;
}

std::vector<ScheduledTaskRecord> InMemoryMetadataStore::ListScheduledTasksByState(JobState state) const {
    std::vector<ScheduledTaskRecord> result;
    for (const auto &task : scheduled_tasks_) {
        if (task.state == state) {
            result.push_back(task);
        }
    }
    return result;
}

std::vector<BackupJob> InMemoryMetadataStore::ListBackupJobs() const {
    return backup_jobs_;
}

std::vector<RestoreRequest> InMemoryMetadataStore::ListRestoreRequests() const {
    return restore_requests_;
}

std::vector<SharedLibraryEntry> InMemoryMetadataStore::ListSharedLibraryEntries() const {
    return shared_library_entries_;
}

std::vector<FileRecord> InMemoryMetadataStore::ListFiles() const {
    return files_;
}

std::vector<StoredObjectRecord> InMemoryMetadataStore::ListStoredObjects() const {
    return stored_objects_;
}

std::vector<BackupRecord> InMemoryMetadataStore::ListBackupRecords() const {
    return backup_records_;
}

std::vector<DeviceFileRecord> InMemoryMetadataStore::ListDeviceFiles() const {
    return device_files_;
}

std::vector<AuditEvent> InMemoryMetadataStore::ListAuditEvents() const {
    return audit_events_;
}

std::vector<AuditEvent> InMemoryMetadataStore::ListAuditEventsByCategory(const std::string &category) const {
    std::vector<AuditEvent> result;
    for (const auto &event : audit_events_) {
        if (event.category == category) {
            result.push_back(event);
        }
    }
    return result;
}

std::vector<ChunkRecord> InMemoryMetadataStore::ListChunks(const std::string &file_id) const {
    for (const auto &file : files_) {
        if (file.file_id == file_id) {
            return file.chunks;
        }
    }
    return {};
}

std::vector<SyncTaskChunk> InMemoryMetadataStore::ListTaskChunks(const std::string &task_id) const {
    std::vector<SyncTaskChunk> result;
    for (const auto &chunk : task_chunks_) {
        if (chunk.task_id == task_id) {
            result.push_back(chunk);
        }
    }
    return result;
}

std::optional<std::filesystem::path> InMemoryMetadataStore::GetLocalMetadataPath() const {
    return std::nullopt;
}

namespace {

void ExecOrThrow(sqlite3 *db, const char *sql) {
    char *errmsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        const std::string message = errmsg ? errmsg : "sqlite exec failed";
        sqlite3_free(errmsg);
        throw std::runtime_error(message);
    }
}

void BindText(sqlite3_stmt *stmt, int index, const std::string &value) {
    sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

void StepDoneOrThrow(sqlite3 *db, sqlite3_stmt *stmt) {
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

bool ColumnExists(sqlite3 *db, const std::string &table, const std::string &column_name) {
    const std::string sql = "PRAGMA table_info(" + table + ")";
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto *column_text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        if (column_text != nullptr && column_name == column_text) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

void EnsureColumnExists(sqlite3 *db,
                        const std::string &table,
                        const std::string &column_name,
                        const std::string &column_definition) {
    if (ColumnExists(db, table, column_name)) {
        return;
    }

    const std::string sql = "ALTER TABLE " + table + " ADD COLUMN " + column_name + " " + column_definition;
    ExecOrThrow(db, sql.c_str());
}

}  // namespace

struct SqliteMetadataStore::Impl {
    sqlite3 *db = nullptr;
    std::filesystem::path db_path;
};

SqliteMetadataStore::SqliteMetadataStore(const std::string &db_path) : impl_(new Impl) {
    impl_->db_path = std::filesystem::path(db_path).lexically_normal();
    if (sqlite3_open(db_path.c_str(), &impl_->db) != SQLITE_OK) {
        const std::string message = impl_->db ? sqlite3_errmsg(impl_->db) : "failed to open sqlite database";
        if (impl_->db != nullptr) {
            sqlite3_close(impl_->db);
            impl_->db = nullptr;
        }
        throw std::runtime_error(message);
    }
    InitializeSchema();
}

SqliteMetadataStore::~SqliteMetadataStore() {
    if (impl_ && impl_->db) {
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
    }
}

void SqliteMetadataStore::InitializeSchema() {
    static const char *kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS node_records (
    node_id TEXT PRIMARY KEY,
    device_name TEXT NOT NULL,
    zt_ip TEXT NOT NULL,
    last_seen_epoch INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS backup_policies (
    policy_id TEXT PRIMARY KEY,
    node_id TEXT NOT NULL,
    source_path TEXT NOT NULL,
    recursive INTEGER NOT NULL,
    enabled INTEGER NOT NULL,
    schedule_expression TEXT NOT NULL,
    created_at_epoch INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS transfer_policies (
    policy_id TEXT PRIMARY KEY,
    retention_seconds INTEGER NOT NULL,
    auto_cleanup INTEGER NOT NULL,
    private_library_only INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS scheduled_tasks (
    task_id TEXT PRIMARY KEY,
    domain INTEGER NOT NULL,
    related_id TEXT NOT NULL,
    source_node TEXT NOT NULL,
    target_node TEXT NOT NULL,
    state INTEGER NOT NULL,
    created_at_epoch INTEGER NOT NULL,
    retry_count INTEGER NOT NULL DEFAULT 0,
    detail TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS backup_jobs (
    job_id TEXT PRIMARY KEY,
    node_id TEXT NOT NULL,
    source_path TEXT NOT NULL,
    destination_label TEXT NOT NULL,
    created_at_epoch INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS restore_requests (
    request_id TEXT PRIMARY KEY,
    file_id TEXT NOT NULL,
    source_node_id TEXT NOT NULL,
    target_node_id TEXT NOT NULL,
    destination_path TEXT NOT NULL,
    created_at_epoch INTEGER NOT NULL,
    state INTEGER NOT NULL DEFAULT 0,
    completed_at_epoch INTEGER NOT NULL DEFAULT 0,
    error_message TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS shared_library_entries (
    entry_id TEXT PRIMARY KEY,
    owner_node_id TEXT NOT NULL,
    source_path TEXT NOT NULL,
    file_id TEXT NOT NULL,
    display_name TEXT NOT NULL,
    note TEXT NOT NULL,
    created_at_epoch INTEGER NOT NULL,
    expires_at_epoch INTEGER NOT NULL,
    expired INTEGER NOT NULL DEFAULT 0,
    delivered INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS shared_library_recipients (
    entry_id TEXT NOT NULL,
    node_id TEXT NOT NULL,
    state INTEGER NOT NULL DEFAULT 0,
    received_path TEXT NOT NULL DEFAULT '',
    error_message TEXT NOT NULL DEFAULT '',
    updated_at_epoch INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (entry_id, node_id),
    FOREIGN KEY (entry_id) REFERENCES shared_library_entries(entry_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS file_records (
    file_id TEXT PRIMARY KEY,
    relative_path TEXT NOT NULL,
    absolute_path TEXT NOT NULL UNIQUE,
    size INTEGER NOT NULL,
    modified_time_epoch INTEGER NOT NULL,
    mode INTEGER NOT NULL,
    root_hash TEXT NOT NULL,
    status INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS chunk_records (
    file_id TEXT NOT NULL,
    chunk_index INTEGER NOT NULL,
    chunk_offset INTEGER NOT NULL,
    chunk_size INTEGER NOT NULL,
    chunk_hash TEXT NOT NULL,
    PRIMARY KEY (file_id, chunk_index),
    FOREIGN KEY (file_id) REFERENCES file_records(file_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS sync_tasks (
    task_id TEXT PRIMARY KEY,
    source_node TEXT NOT NULL,
    target_node TEXT NOT NULL,
    file_id TEXT NOT NULL,
    relative_path TEXT NOT NULL DEFAULT '',
    expected_root_hash TEXT NOT NULL DEFAULT '',
    temp_path TEXT NOT NULL DEFAULT '',
    total_chunks INTEGER NOT NULL DEFAULT 0,
    completed_chunks INTEGER NOT NULL DEFAULT 0,
    state INTEGER NOT NULL,
    retry_count INTEGER NOT NULL,
    error_message TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS sync_task_chunks (
    task_id TEXT NOT NULL,
    file_id TEXT NOT NULL,
    chunk_index INTEGER NOT NULL,
    chunk_offset INTEGER NOT NULL,
    chunk_size INTEGER NOT NULL,
    chunk_hash TEXT NOT NULL,
    PRIMARY KEY (task_id, chunk_index),
    FOREIGN KEY (task_id) REFERENCES sync_tasks(task_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS stored_objects (
    file_id TEXT PRIMARY KEY,
    backend_name TEXT NOT NULL,
    storage_key TEXT NOT NULL,
    access_url TEXT NOT NULL,
    source_path TEXT NOT NULL,
    stored_at_epoch INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS backup_records (
    record_id TEXT PRIMARY KEY,
    job_id TEXT NOT NULL,
    node_id TEXT NOT NULL,
    file_id TEXT NOT NULL,
    relative_path TEXT NOT NULL,
    source_path TEXT NOT NULL,
    backend_name TEXT NOT NULL,
    storage_key TEXT NOT NULL,
    backed_up_at_epoch INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS device_file_records (
    record_id TEXT PRIMARY KEY,
    node_id TEXT NOT NULL,
    file_id TEXT NOT NULL,
    relative_path TEXT NOT NULL,
    absolute_path TEXT NOT NULL,
    source_kind TEXT NOT NULL,
    source_ref_id TEXT NOT NULL,
    updated_at_epoch INTEGER NOT NULL,
    UNIQUE(node_id, absolute_path)
);

CREATE TABLE IF NOT EXISTS audit_events (
    event_id TEXT PRIMARY KEY,
    category TEXT NOT NULL,
    detail TEXT NOT NULL,
    created_at_epoch INTEGER NOT NULL
);
)SQL";

    ExecOrThrow(impl_->db, "PRAGMA foreign_keys = ON;");
    ExecOrThrow(impl_->db, kSchema);
    EnsureColumnExists(impl_->db, "sync_tasks", "relative_path", "TEXT NOT NULL DEFAULT ''");
    EnsureColumnExists(impl_->db, "sync_tasks", "expected_root_hash", "TEXT NOT NULL DEFAULT ''");
    EnsureColumnExists(impl_->db, "sync_tasks", "temp_path", "TEXT NOT NULL DEFAULT ''");
    EnsureColumnExists(impl_->db, "sync_tasks", "total_chunks", "INTEGER NOT NULL DEFAULT 0");
    EnsureColumnExists(impl_->db, "sync_tasks", "completed_chunks", "INTEGER NOT NULL DEFAULT 0");
    EnsureColumnExists(impl_->db, "shared_library_entries", "expired", "INTEGER NOT NULL DEFAULT 0");
    EnsureColumnExists(impl_->db, "scheduled_tasks", "retry_count", "INTEGER NOT NULL DEFAULT 0");
    EnsureColumnExists(impl_->db, "restore_requests", "state", "INTEGER NOT NULL DEFAULT 0");
    EnsureColumnExists(impl_->db, "restore_requests", "completed_at_epoch", "INTEGER NOT NULL DEFAULT 0");
    EnsureColumnExists(impl_->db, "restore_requests", "error_message", "TEXT NOT NULL DEFAULT ''");
    EnsureColumnExists(impl_->db, "shared_library_recipients", "state", "INTEGER NOT NULL DEFAULT 0");
    EnsureColumnExists(impl_->db, "shared_library_recipients", "received_path", "TEXT NOT NULL DEFAULT ''");
    EnsureColumnExists(impl_->db, "shared_library_recipients", "error_message", "TEXT NOT NULL DEFAULT ''");
    EnsureColumnExists(impl_->db, "shared_library_recipients", "updated_at_epoch", "INTEGER NOT NULL DEFAULT 0");
}

void SqliteMetadataStore::UpsertNode(const NodeInfo &node) {
    static const char *kSql =
        "INSERT INTO node_records(node_id, device_name, zt_ip, last_seen_epoch) "
        "VALUES(?, ?, ?, ?) "
        "ON CONFLICT(node_id) DO UPDATE SET "
        "device_name=excluded.device_name, zt_ip=excluded.zt_ip, last_seen_epoch=excluded.last_seen_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, node.node_id);
    BindText(stmt, 2, node.device_name);
    BindText(stmt, 3, node.zt_ip);
    sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(node.last_seen_epoch));
    StepDoneOrThrow(impl_->db, stmt);
    sqlite3_finalize(stmt);
}

void SqliteMetadataStore::UpsertBackupPolicy(const BackupPolicy &policy) {
    static const char *kSql =
        "INSERT INTO backup_policies(policy_id, node_id, source_path, recursive, enabled, schedule_expression, "
        "created_at_epoch) VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(policy_id) DO UPDATE SET "
        "node_id=excluded.node_id, source_path=excluded.source_path, recursive=excluded.recursive, "
        "enabled=excluded.enabled, schedule_expression=excluded.schedule_expression, "
        "created_at_epoch=excluded.created_at_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, policy.policy_id);
    BindText(stmt, 2, policy.node_id);
    BindText(stmt, 3, policy.source_path);
    sqlite3_bind_int(stmt, 4, policy.recursive ? 1 : 0);
    sqlite3_bind_int(stmt, 5, policy.enabled ? 1 : 0);
    BindText(stmt, 6, policy.schedule_expression);
    sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(policy.created_at_epoch));
    StepDoneOrThrow(impl_->db, stmt);
    sqlite3_finalize(stmt);
}

void SqliteMetadataStore::UpsertTransferPolicy(const TransferPolicy &policy) {
    static const char *kSql =
        "INSERT INTO transfer_policies(policy_id, retention_seconds, auto_cleanup, private_library_only) "
        "VALUES(?, ?, ?, ?) "
        "ON CONFLICT(policy_id) DO UPDATE SET "
        "retention_seconds=excluded.retention_seconds, auto_cleanup=excluded.auto_cleanup, "
        "private_library_only=excluded.private_library_only";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, policy.policy_id);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(policy.retention_seconds));
    sqlite3_bind_int(stmt, 3, policy.auto_cleanup ? 1 : 0);
    sqlite3_bind_int(stmt, 4, policy.private_library_only ? 1 : 0);
    StepDoneOrThrow(impl_->db, stmt);
    sqlite3_finalize(stmt);
}

void SqliteMetadataStore::UpsertScheduledTask(const ScheduledTaskRecord &task) {
    static const char *kSql =
        "INSERT INTO scheduled_tasks(task_id, domain, related_id, source_node, target_node, state, created_at_epoch, "
        "retry_count, detail) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(task_id) DO UPDATE SET "
        "domain=excluded.domain, related_id=excluded.related_id, source_node=excluded.source_node, "
        "target_node=excluded.target_node, state=excluded.state, created_at_epoch=excluded.created_at_epoch, "
        "retry_count=excluded.retry_count, detail=excluded.detail";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, task.task_id);
    sqlite3_bind_int(stmt, 2, static_cast<int>(task.domain));
    BindText(stmt, 3, task.related_id);
    BindText(stmt, 4, task.source_node);
    BindText(stmt, 5, task.target_node);
    sqlite3_bind_int(stmt, 6, static_cast<int>(task.state));
    sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(task.created_at_epoch));
    sqlite3_bind_int(stmt, 8, static_cast<int>(task.retry_count));
    BindText(stmt, 9, task.detail);
    StepDoneOrThrow(impl_->db, stmt);
    sqlite3_finalize(stmt);
}

void SqliteMetadataStore::UpsertBackupJob(const BackupJob &job) {
    static const char *kSql =
        "INSERT INTO backup_jobs(job_id, node_id, source_path, destination_label, created_at_epoch) "
        "VALUES(?, ?, ?, ?, ?) "
        "ON CONFLICT(job_id) DO UPDATE SET "
        "node_id=excluded.node_id, source_path=excluded.source_path, destination_label=excluded.destination_label, "
        "created_at_epoch=excluded.created_at_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, job.job_id);
    BindText(stmt, 2, job.node_id);
    BindText(stmt, 3, job.source_path);
    BindText(stmt, 4, job.destination_label);
    sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(job.created_at_epoch));
    StepDoneOrThrow(impl_->db, stmt);
    sqlite3_finalize(stmt);
}

void SqliteMetadataStore::UpsertRestoreRequest(const RestoreRequest &request) {
    static const char *kSql =
        "INSERT INTO restore_requests(request_id, file_id, source_node_id, target_node_id, destination_path, "
        "created_at_epoch, state, completed_at_epoch, error_message) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(request_id) DO UPDATE SET "
        "file_id=excluded.file_id, source_node_id=excluded.source_node_id, "
        "target_node_id=excluded.target_node_id, destination_path=excluded.destination_path, "
        "created_at_epoch=excluded.created_at_epoch, state=excluded.state, "
        "completed_at_epoch=excluded.completed_at_epoch, error_message=excluded.error_message";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, request.request_id);
    BindText(stmt, 2, request.file_id);
    BindText(stmt, 3, request.source_node_id);
    BindText(stmt, 4, request.target_node_id);
    BindText(stmt, 5, request.destination_path);
    sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(request.created_at_epoch));
    sqlite3_bind_int(stmt, 7, static_cast<int>(request.state));
    sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(request.completed_at_epoch));
    BindText(stmt, 9, request.error_message);
    StepDoneOrThrow(impl_->db, stmt);
    sqlite3_finalize(stmt);
}

void SqliteMetadataStore::UpsertSharedLibraryEntry(const SharedLibraryEntry &entry) {
    ExecOrThrow(impl_->db, "BEGIN IMMEDIATE TRANSACTION;");

    try {
        static const char *kEntrySql =
            "INSERT INTO shared_library_entries(entry_id, owner_node_id, source_path, file_id, display_name, note, "
            "created_at_epoch, expires_at_epoch, expired, delivered) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(entry_id) DO UPDATE SET "
            "owner_node_id=excluded.owner_node_id, source_path=excluded.source_path, file_id=excluded.file_id, "
            "display_name=excluded.display_name, note=excluded.note, created_at_epoch=excluded.created_at_epoch, "
            "expires_at_epoch=excluded.expires_at_epoch, expired=excluded.expired, delivered=excluded.delivered";

        sqlite3_stmt *entry_stmt = nullptr;
        sqlite3_prepare_v2(impl_->db, kEntrySql, -1, &entry_stmt, nullptr);
        BindText(entry_stmt, 1, entry.entry_id);
        BindText(entry_stmt, 2, entry.owner_node_id);
        BindText(entry_stmt, 3, entry.source_path);
        BindText(entry_stmt, 4, entry.file_id);
        BindText(entry_stmt, 5, entry.display_name);
        BindText(entry_stmt, 6, entry.note);
        sqlite3_bind_int64(entry_stmt, 7, static_cast<sqlite3_int64>(entry.created_at_epoch));
        sqlite3_bind_int64(entry_stmt, 8, static_cast<sqlite3_int64>(entry.expires_at_epoch));
        sqlite3_bind_int(entry_stmt, 9, entry.expired ? 1 : 0);
        sqlite3_bind_int(entry_stmt, 10, entry.delivered ? 1 : 0);
        StepDoneOrThrow(impl_->db, entry_stmt);
        sqlite3_finalize(entry_stmt);

        sqlite3_stmt *delete_stmt = nullptr;
        sqlite3_prepare_v2(impl_->db, "DELETE FROM shared_library_recipients WHERE entry_id = ?", -1, &delete_stmt, nullptr);
        BindText(delete_stmt, 1, entry.entry_id);
        StepDoneOrThrow(impl_->db, delete_stmt);
        sqlite3_finalize(delete_stmt);

        static const char *kRecipientSql =
            "INSERT INTO shared_library_recipients(entry_id, node_id, state, received_path, error_message, "
            "updated_at_epoch) VALUES(?, ?, ?, ?, ?, ?)";
        sqlite3_stmt *recipient_stmt = nullptr;
        sqlite3_prepare_v2(impl_->db, kRecipientSql, -1, &recipient_stmt, nullptr);
        if (entry.recipients.empty()) {
            for (const auto &node_id : entry.recipient_nodes) {
                sqlite3_reset(recipient_stmt);
                sqlite3_clear_bindings(recipient_stmt);
                BindText(recipient_stmt, 1, entry.entry_id);
                BindText(recipient_stmt, 2, node_id);
                sqlite3_bind_int(recipient_stmt, 3, static_cast<int>(SharedRecipientState::kDelivered));
                BindText(recipient_stmt, 4, "");
                BindText(recipient_stmt, 5, "");
                sqlite3_bind_int64(recipient_stmt, 6, static_cast<sqlite3_int64>(entry.created_at_epoch));
                StepDoneOrThrow(impl_->db, recipient_stmt);
            }
        }
        for (const auto &recipient : entry.recipients) {
            sqlite3_reset(recipient_stmt);
            sqlite3_clear_bindings(recipient_stmt);
            BindText(recipient_stmt, 1, entry.entry_id);
            BindText(recipient_stmt, 2, recipient.node_id);
            sqlite3_bind_int(recipient_stmt, 3, static_cast<int>(recipient.state));
            BindText(recipient_stmt, 4, recipient.received_path);
            BindText(recipient_stmt, 5, recipient.error_message);
            sqlite3_bind_int64(recipient_stmt, 6, static_cast<sqlite3_int64>(recipient.updated_at_epoch));
            StepDoneOrThrow(impl_->db, recipient_stmt);
        }
        sqlite3_finalize(recipient_stmt);

        ExecOrThrow(impl_->db, "COMMIT;");
    } catch (...) {
        ExecOrThrow(impl_->db, "ROLLBACK;");
        throw;
    }
}

void SqliteMetadataStore::UpsertFile(const FileRecord &file) {
    ExecOrThrow(impl_->db, "BEGIN IMMEDIATE TRANSACTION;");

    try {
        // A file may keep the same absolute path while receiving a new content hash.
        // In that case we treat the path as the stable identity on a node and replace
        // the old version record before inserting the new file_id.
        std::string existing_file_id;
        sqlite3_stmt *lookup_stmt = nullptr;
        sqlite3_prepare_v2(impl_->db, "SELECT file_id FROM file_records WHERE absolute_path = ?", -1, &lookup_stmt, nullptr);
        BindText(lookup_stmt, 1, file.absolute_path);
        if (sqlite3_step(lookup_stmt) == SQLITE_ROW) {
            existing_file_id = reinterpret_cast<const char *>(sqlite3_column_text(lookup_stmt, 0));
        }
        sqlite3_finalize(lookup_stmt);

        if (!existing_file_id.empty() && existing_file_id != file.file_id) {
            sqlite3_stmt *delete_chunks_stmt = nullptr;
            sqlite3_prepare_v2(impl_->db, "DELETE FROM chunk_records WHERE file_id = ?", -1, &delete_chunks_stmt, nullptr);
            BindText(delete_chunks_stmt, 1, existing_file_id);
            StepDoneOrThrow(impl_->db, delete_chunks_stmt);
            sqlite3_finalize(delete_chunks_stmt);

            sqlite3_stmt *delete_file_stmt = nullptr;
            sqlite3_prepare_v2(impl_->db, "DELETE FROM file_records WHERE absolute_path = ?", -1, &delete_file_stmt, nullptr);
            BindText(delete_file_stmt, 1, file.absolute_path);
            StepDoneOrThrow(impl_->db, delete_file_stmt);
            sqlite3_finalize(delete_file_stmt);
        }

        static const char *kFileSql =
            "INSERT INTO file_records(file_id, relative_path, absolute_path, size, modified_time_epoch, mode, root_hash, status) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(file_id) DO UPDATE SET "
            "relative_path=excluded.relative_path, absolute_path=excluded.absolute_path, size=excluded.size, "
            "modified_time_epoch=excluded.modified_time_epoch, mode=excluded.mode, root_hash=excluded.root_hash, status=excluded.status";

        sqlite3_stmt *file_stmt = nullptr;
        sqlite3_prepare_v2(impl_->db, kFileSql, -1, &file_stmt, nullptr);
        BindText(file_stmt, 1, file.file_id);
        BindText(file_stmt, 2, file.relative_path);
        BindText(file_stmt, 3, file.absolute_path);
        sqlite3_bind_int64(file_stmt, 4, static_cast<sqlite3_int64>(file.size));
        sqlite3_bind_int64(file_stmt, 5, static_cast<sqlite3_int64>(file.modified_time_epoch));
        sqlite3_bind_int(file_stmt, 6, static_cast<int>(file.mode));
        BindText(file_stmt, 7, file.root_hash);
        sqlite3_bind_int(file_stmt, 8, static_cast<int>(file.status));
        StepDoneOrThrow(impl_->db, file_stmt);
        sqlite3_finalize(file_stmt);

        sqlite3_stmt *delete_stmt = nullptr;
        sqlite3_prepare_v2(impl_->db, "DELETE FROM chunk_records WHERE file_id = ?", -1, &delete_stmt, nullptr);
        BindText(delete_stmt, 1, file.file_id);
        StepDoneOrThrow(impl_->db, delete_stmt);
        sqlite3_finalize(delete_stmt);

        static const char *kChunkSql =
            "INSERT INTO chunk_records(file_id, chunk_index, chunk_offset, chunk_size, chunk_hash) VALUES(?, ?, ?, ?, ?)";

        sqlite3_stmt *chunk_stmt = nullptr;
        sqlite3_prepare_v2(impl_->db, kChunkSql, -1, &chunk_stmt, nullptr);
        for (const auto &chunk : file.chunks) {
            sqlite3_reset(chunk_stmt);
            sqlite3_clear_bindings(chunk_stmt);
            BindText(chunk_stmt, 1, file.file_id);
            sqlite3_bind_int(chunk_stmt, 2, static_cast<int>(chunk.chunk_index));
            sqlite3_bind_int64(chunk_stmt, 3, static_cast<sqlite3_int64>(chunk.offset));
            sqlite3_bind_int64(chunk_stmt, 4, static_cast<sqlite3_int64>(chunk.size));
            BindText(chunk_stmt, 5, chunk.chunk_hash);
            StepDoneOrThrow(impl_->db, chunk_stmt);
        }
        sqlite3_finalize(chunk_stmt);

        ExecOrThrow(impl_->db, "COMMIT;");
    } catch (...) {
        ExecOrThrow(impl_->db, "ROLLBACK;");
        throw;
    }
}

void SqliteMetadataStore::UpsertTask(const SyncTask &task) {
    static const char *kSql =
        "INSERT INTO sync_tasks(task_id, source_node, target_node, file_id, relative_path, expected_root_hash, temp_path, "
        "total_chunks, completed_chunks, state, retry_count, error_message) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(task_id) DO UPDATE SET "
        "source_node=excluded.source_node, target_node=excluded.target_node, file_id=excluded.file_id, "
        "relative_path=excluded.relative_path, expected_root_hash=excluded.expected_root_hash, temp_path=excluded.temp_path, "
        "total_chunks=excluded.total_chunks, completed_chunks=excluded.completed_chunks, "
        "state=excluded.state, retry_count=excluded.retry_count, error_message=excluded.error_message";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, task.task_id);
    BindText(stmt, 2, task.source_node);
    BindText(stmt, 3, task.target_node);
    BindText(stmt, 4, task.file_id);
    BindText(stmt, 5, task.relative_path);
    BindText(stmt, 6, task.expected_root_hash);
    BindText(stmt, 7, task.temp_path);
    sqlite3_bind_int(stmt, 8, static_cast<int>(task.total_chunks));
    sqlite3_bind_int(stmt, 9, static_cast<int>(task.completed_chunks));
    sqlite3_bind_int(stmt, 10, static_cast<int>(task.state));
    sqlite3_bind_int(stmt, 11, static_cast<int>(task.retry_count));
    BindText(stmt, 12, task.error_message);
    StepDoneOrThrow(impl_->db, stmt);
    sqlite3_finalize(stmt);
}

void SqliteMetadataStore::MarkTaskChunkComplete(const SyncTaskChunk &chunk) {
    static const char *kSql =
        "INSERT INTO sync_task_chunks(task_id, file_id, chunk_index, chunk_offset, chunk_size, chunk_hash) "
        "VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(task_id, chunk_index) DO UPDATE SET "
        "file_id=excluded.file_id, chunk_offset=excluded.chunk_offset, "
        "chunk_size=excluded.chunk_size, chunk_hash=excluded.chunk_hash";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, chunk.task_id);
    BindText(stmt, 2, chunk.file_id);
    sqlite3_bind_int(stmt, 3, static_cast<int>(chunk.chunk_index));
    sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(chunk.offset));
    sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(chunk.size));
    BindText(stmt, 6, chunk.chunk_hash);
    StepDoneOrThrow(impl_->db, stmt);
    sqlite3_finalize(stmt);
}

void SqliteMetadataStore::ClearTaskChunks(const std::string &task_id) {
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, "DELETE FROM sync_task_chunks WHERE task_id = ?", -1, &stmt, nullptr);
    BindText(stmt, 1, task_id);
    StepDoneOrThrow(impl_->db, stmt);
    sqlite3_finalize(stmt);
}

void SqliteMetadataStore::UpsertStoredObject(const StoredObjectRecord &record) {
    static const char *kSql =
        "INSERT INTO stored_objects(file_id, backend_name, storage_key, access_url, source_path, stored_at_epoch) "
        "VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(file_id) DO UPDATE SET "
        "backend_name=excluded.backend_name, storage_key=excluded.storage_key, access_url=excluded.access_url, "
        "source_path=excluded.source_path, stored_at_epoch=excluded.stored_at_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, record.file_id);
    BindText(stmt, 2, record.backend_name);
    BindText(stmt, 3, record.storage_key);
    BindText(stmt, 4, record.access_url);
    BindText(stmt, 5, record.source_path);
    sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(record.stored_at_epoch));
    StepDoneOrThrow(impl_->db, stmt);
    sqlite3_finalize(stmt);
}

void SqliteMetadataStore::UpsertBackupRecord(const BackupRecord &record) {
    static const char *kSql =
        "INSERT INTO backup_records(record_id, job_id, node_id, file_id, relative_path, source_path, backend_name, "
        "storage_key, backed_up_at_epoch) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(record_id) DO UPDATE SET "
        "job_id=excluded.job_id, node_id=excluded.node_id, file_id=excluded.file_id, "
        "relative_path=excluded.relative_path, source_path=excluded.source_path, "
        "backend_name=excluded.backend_name, storage_key=excluded.storage_key, "
        "backed_up_at_epoch=excluded.backed_up_at_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, record.record_id);
    BindText(stmt, 2, record.job_id);
    BindText(stmt, 3, record.node_id);
    BindText(stmt, 4, record.file_id);
    BindText(stmt, 5, record.relative_path);
    BindText(stmt, 6, record.source_path);
    BindText(stmt, 7, record.backend_name);
    BindText(stmt, 8, record.storage_key);
    sqlite3_bind_int64(stmt, 9, static_cast<sqlite3_int64>(record.backed_up_at_epoch));
    StepDoneOrThrow(impl_->db, stmt);
    sqlite3_finalize(stmt);
}

void SqliteMetadataStore::UpsertDeviceFile(const DeviceFileRecord &record) {
    static const char *kSql =
        "INSERT INTO device_file_records(record_id, node_id, file_id, relative_path, absolute_path, source_kind, "
        "source_ref_id, updated_at_epoch) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(node_id, absolute_path) DO UPDATE SET "
        "record_id=excluded.record_id, file_id=excluded.file_id, relative_path=excluded.relative_path, "
        "source_kind=excluded.source_kind, source_ref_id=excluded.source_ref_id, "
        "updated_at_epoch=excluded.updated_at_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, record.record_id);
    BindText(stmt, 2, record.node_id);
    BindText(stmt, 3, record.file_id);
    BindText(stmt, 4, record.relative_path);
    BindText(stmt, 5, record.absolute_path);
    BindText(stmt, 6, record.source_kind);
    BindText(stmt, 7, record.source_ref_id);
    sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(record.updated_at_epoch));
    StepDoneOrThrow(impl_->db, stmt);
    sqlite3_finalize(stmt);
}

void SqliteMetadataStore::AppendAuditEvent(const AuditEvent &event) {
    static const char *kSql =
        "INSERT INTO audit_events(event_id, category, detail, created_at_epoch) VALUES(?, ?, ?, ?)";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, event.event_id);
    BindText(stmt, 2, event.category);
    BindText(stmt, 3, event.detail);
    sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(event.created_at_epoch));
    StepDoneOrThrow(impl_->db, stmt);
    sqlite3_finalize(stmt);
}

std::optional<NodeInfo> SqliteMetadataStore::GetNode(const std::string &node_id) const {
    static const char *kSql =
        "SELECT node_id, device_name, zt_ip, last_seen_epoch FROM node_records WHERE node_id = ?";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, node_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    NodeInfo node;
    node.node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    node.device_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    node.zt_ip = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    node.last_seen_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 3));
    sqlite3_finalize(stmt);
    return node;
}

std::optional<BackupPolicy> SqliteMetadataStore::GetBackupPolicy(const std::string &policy_id) const {
    static const char *kSql =
        "SELECT policy_id, node_id, source_path, recursive, enabled, schedule_expression, created_at_epoch "
        "FROM backup_policies WHERE policy_id = ?";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, policy_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    BackupPolicy policy;
    policy.policy_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    policy.node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    policy.source_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    policy.recursive = sqlite3_column_int(stmt, 3) != 0;
    policy.enabled = sqlite3_column_int(stmt, 4) != 0;
    policy.schedule_expression = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
    policy.created_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 6));
    sqlite3_finalize(stmt);
    return policy;
}

std::optional<TransferPolicy> SqliteMetadataStore::GetTransferPolicy(const std::string &policy_id) const {
    static const char *kSql =
        "SELECT policy_id, retention_seconds, auto_cleanup, private_library_only "
        "FROM transfer_policies WHERE policy_id = ?";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, policy_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    TransferPolicy policy;
    policy.policy_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    policy.retention_seconds = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1));
    policy.auto_cleanup = sqlite3_column_int(stmt, 2) != 0;
    policy.private_library_only = sqlite3_column_int(stmt, 3) != 0;
    sqlite3_finalize(stmt);
    return policy;
}

std::optional<ScheduledTaskRecord> SqliteMetadataStore::GetScheduledTaskRecord(const std::string &task_id) const {
    static const char *kSql =
        "SELECT task_id, domain, related_id, source_node, target_node, state, created_at_epoch, retry_count, detail "
        "FROM scheduled_tasks WHERE task_id = ?";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, task_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    ScheduledTaskRecord task;
    task.task_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    task.domain = static_cast<JobDomain>(sqlite3_column_int(stmt, 1));
    task.related_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    task.source_node = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    task.target_node = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
    task.state = static_cast<JobState>(sqlite3_column_int(stmt, 5));
    task.created_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 6));
    task.retry_count = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 7));
    task.detail = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
    sqlite3_finalize(stmt);
    return task;
}

std::optional<BackupJob> SqliteMetadataStore::GetBackupJob(const std::string &job_id) const {
    static const char *kSql =
        "SELECT job_id, node_id, source_path, destination_label, created_at_epoch FROM backup_jobs WHERE job_id = ?";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, job_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    BackupJob job;
    job.job_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    job.node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    job.source_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    job.destination_label = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    job.created_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 4));
    sqlite3_finalize(stmt);
    return job;
}

std::optional<RestoreRequest> SqliteMetadataStore::GetRestoreRequest(const std::string &request_id) const {
    static const char *kSql =
        "SELECT request_id, file_id, source_node_id, target_node_id, destination_path, created_at_epoch, "
        "state, completed_at_epoch, error_message "
        "FROM restore_requests WHERE request_id = ?";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, request_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    RestoreRequest request;
    request.request_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    request.file_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    request.source_node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    request.target_node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    request.destination_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
    request.created_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 5));
    request.state = static_cast<RestoreRequestState>(sqlite3_column_int(stmt, 6));
    request.completed_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 7));
    request.error_message = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
    sqlite3_finalize(stmt);
    return request;
}

std::optional<SharedLibraryEntry> SqliteMetadataStore::GetSharedLibraryEntry(const std::string &entry_id) const {
    static const char *kSql =
        "SELECT entry_id, owner_node_id, source_path, file_id, display_name, note, created_at_epoch, "
        "expires_at_epoch, expired, delivered FROM shared_library_entries WHERE entry_id = ?";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, entry_id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    SharedLibraryEntry entry;
    entry.entry_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    entry.owner_node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    entry.source_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    entry.file_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    entry.display_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
    entry.note = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
    entry.created_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 6));
    entry.expires_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 7));
    entry.expired = sqlite3_column_int(stmt, 8) != 0;
    entry.delivered = sqlite3_column_int(stmt, 9) != 0;
    sqlite3_finalize(stmt);

    static const char *kRecipientSql =
        "SELECT node_id, state, received_path, error_message, updated_at_epoch "
        "FROM shared_library_recipients WHERE entry_id = ? ORDER BY node_id";
    sqlite3_stmt *recipient_stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kRecipientSql, -1, &recipient_stmt, nullptr);
    BindText(recipient_stmt, 1, entry_id);
    while (sqlite3_step(recipient_stmt) == SQLITE_ROW) {
        SharedLibraryRecipient recipient;
        recipient.entry_id = entry_id;
        recipient.node_id = reinterpret_cast<const char *>(sqlite3_column_text(recipient_stmt, 0));
        recipient.state = static_cast<SharedRecipientState>(sqlite3_column_int(recipient_stmt, 1));
        recipient.received_path = reinterpret_cast<const char *>(sqlite3_column_text(recipient_stmt, 2));
        recipient.error_message = reinterpret_cast<const char *>(sqlite3_column_text(recipient_stmt, 3));
        recipient.updated_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(recipient_stmt, 4));
        entry.recipients.push_back(recipient);
        entry.recipient_nodes.push_back(recipient.node_id);
    }
    sqlite3_finalize(recipient_stmt);

    return entry;
}

std::optional<FileRecord> SqliteMetadataStore::GetFile(const std::string &file_id) const {
    static const char *kSql =
        "SELECT file_id, relative_path, absolute_path, size, modified_time_epoch, mode, root_hash, status "
        "FROM file_records WHERE file_id = ?";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, file_id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    FileRecord file;
    file.file_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    file.relative_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    file.absolute_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    file.size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 3));
    file.modified_time_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 4));
    file.mode = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 5));
    file.root_hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
    file.status = static_cast<FileStatus>(sqlite3_column_int(stmt, 7));
    sqlite3_finalize(stmt);

    file.chunks = ListChunks(file.file_id);
    return file;
}

std::optional<FileRecord> SqliteMetadataStore::GetFileByPath(const std::string &absolute_path) const {
    static const char *kSql =
        "SELECT file_id, relative_path, absolute_path, size, modified_time_epoch, mode, root_hash, status "
        "FROM file_records WHERE absolute_path = ?";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, absolute_path);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    FileRecord file;
    file.file_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    file.relative_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    file.absolute_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    file.size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 3));
    file.modified_time_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 4));
    file.mode = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 5));
    file.root_hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
    file.status = static_cast<FileStatus>(sqlite3_column_int(stmt, 7));
    sqlite3_finalize(stmt);

    file.chunks = ListChunks(file.file_id);
    return file;
}

std::optional<SyncTask> SqliteMetadataStore::GetTask(const std::string &task_id) const {
    static const char *kSql =
        "SELECT task_id, source_node, target_node, file_id, relative_path, expected_root_hash, temp_path, "
        "total_chunks, completed_chunks, state, retry_count, error_message "
        "FROM sync_tasks WHERE task_id = ?";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, task_id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    SyncTask task;
    task.task_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    task.source_node = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    task.target_node = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    task.file_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    task.relative_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
    task.expected_root_hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
    task.temp_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
    task.total_chunks = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 7));
    task.completed_chunks = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 8));
    task.state = static_cast<SyncTaskState>(sqlite3_column_int(stmt, 9));
    task.retry_count = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 10));
    task.error_message = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 11));
    sqlite3_finalize(stmt);
    return task;
}

std::optional<StoredObjectRecord> SqliteMetadataStore::GetStoredObject(const std::string &file_id) const {
    static const char *kSql =
        "SELECT file_id, backend_name, storage_key, access_url, source_path, stored_at_epoch "
        "FROM stored_objects WHERE file_id = ?";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, file_id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    StoredObjectRecord record;
    record.file_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    record.backend_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    record.storage_key = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    record.access_url = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    record.source_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
    record.stored_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 5));
    sqlite3_finalize(stmt);
    return record;
}

std::vector<NodeInfo> SqliteMetadataStore::ListNodes() const {
    static const char *kSql =
        "SELECT node_id, device_name, zt_ip, last_seen_epoch FROM node_records ORDER BY node_id";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);

    std::vector<NodeInfo> nodes;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        NodeInfo node;
        node.node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        node.device_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        node.zt_ip = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        node.last_seen_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 3));
        nodes.push_back(node);
    }

    sqlite3_finalize(stmt);
    return nodes;
}

std::vector<BackupPolicy> SqliteMetadataStore::ListBackupPolicies() const {
    static const char *kSql =
        "SELECT policy_id, node_id, source_path, recursive, enabled, schedule_expression, created_at_epoch "
        "FROM backup_policies ORDER BY created_at_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);

    std::vector<BackupPolicy> policies;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        BackupPolicy policy;
        policy.policy_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        policy.node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        policy.source_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        policy.recursive = sqlite3_column_int(stmt, 3) != 0;
        policy.enabled = sqlite3_column_int(stmt, 4) != 0;
        policy.schedule_expression = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
        policy.created_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 6));
        policies.push_back(policy);
    }

    sqlite3_finalize(stmt);
    return policies;
}

std::vector<TransferPolicy> SqliteMetadataStore::ListTransferPolicies() const {
    static const char *kSql =
        "SELECT policy_id, retention_seconds, auto_cleanup, private_library_only "
        "FROM transfer_policies ORDER BY policy_id";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);

    std::vector<TransferPolicy> policies;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TransferPolicy policy;
        policy.policy_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        policy.retention_seconds = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1));
        policy.auto_cleanup = sqlite3_column_int(stmt, 2) != 0;
        policy.private_library_only = sqlite3_column_int(stmt, 3) != 0;
        policies.push_back(policy);
    }

    sqlite3_finalize(stmt);
    return policies;
}

std::vector<ScheduledTaskRecord> SqliteMetadataStore::ListScheduledTasks() const {
    static const char *kSql =
        "SELECT task_id, domain, related_id, source_node, target_node, state, created_at_epoch, retry_count, detail "
        "FROM scheduled_tasks ORDER BY created_at_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);

    std::vector<ScheduledTaskRecord> tasks;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ScheduledTaskRecord task;
        task.task_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        task.domain = static_cast<JobDomain>(sqlite3_column_int(stmt, 1));
        task.related_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        task.source_node = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        task.target_node = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        task.state = static_cast<JobState>(sqlite3_column_int(stmt, 5));
        task.created_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 6));
        task.retry_count = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 7));
        task.detail = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        tasks.push_back(task);
    }

    sqlite3_finalize(stmt);
    return tasks;
}

std::vector<ScheduledTaskRecord> SqliteMetadataStore::ListScheduledTasksByState(JobState state) const {
    static const char *kSql =
        "SELECT task_id, domain, related_id, source_node, target_node, state, created_at_epoch, retry_count, detail "
        "FROM scheduled_tasks WHERE state = ? ORDER BY created_at_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, static_cast<int>(state));

    std::vector<ScheduledTaskRecord> tasks;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ScheduledTaskRecord task;
        task.task_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        task.domain = static_cast<JobDomain>(sqlite3_column_int(stmt, 1));
        task.related_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        task.source_node = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        task.target_node = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        task.state = static_cast<JobState>(sqlite3_column_int(stmt, 5));
        task.created_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 6));
        task.retry_count = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 7));
        task.detail = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        tasks.push_back(task);
    }

    sqlite3_finalize(stmt);
    return tasks;
}

std::vector<BackupJob> SqliteMetadataStore::ListBackupJobs() const {
    static const char *kSql =
        "SELECT job_id, node_id, source_path, destination_label, created_at_epoch FROM backup_jobs ORDER BY created_at_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);

    std::vector<BackupJob> jobs;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        BackupJob job;
        job.job_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        job.node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        job.source_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        job.destination_label = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        job.created_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 4));
        jobs.push_back(job);
    }

    sqlite3_finalize(stmt);
    return jobs;
}

std::vector<RestoreRequest> SqliteMetadataStore::ListRestoreRequests() const {
    static const char *kSql =
        "SELECT request_id, file_id, source_node_id, target_node_id, destination_path, created_at_epoch, "
        "state, completed_at_epoch, error_message "
        "FROM restore_requests ORDER BY created_at_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);

    std::vector<RestoreRequest> requests;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RestoreRequest request;
        request.request_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        request.file_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        request.source_node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        request.target_node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        request.destination_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        request.created_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 5));
        request.state = static_cast<RestoreRequestState>(sqlite3_column_int(stmt, 6));
        request.completed_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 7));
        request.error_message = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        requests.push_back(request);
    }

    sqlite3_finalize(stmt);
    return requests;
}

std::vector<SharedLibraryEntry> SqliteMetadataStore::ListSharedLibraryEntries() const {
    static const char *kSql =
        "SELECT entry_id FROM shared_library_entries ORDER BY created_at_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);

    std::vector<SharedLibraryEntry> entries;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto *entry_text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        if (entry_text == nullptr) {
            continue;
        }
        auto entry = GetSharedLibraryEntry(entry_text);
        if (entry.has_value()) {
            entries.push_back(*entry);
        }
    }

    sqlite3_finalize(stmt);
    return entries;
}

std::vector<FileRecord> SqliteMetadataStore::ListFiles() const {
    static const char *kSql =
        "SELECT file_id, relative_path, absolute_path, size, modified_time_epoch, mode, root_hash, status "
        "FROM file_records ORDER BY absolute_path";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);

    std::vector<FileRecord> files;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FileRecord file;
        file.file_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        file.relative_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        file.absolute_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        file.size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 3));
        file.modified_time_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 4));
        file.mode = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 5));
        file.root_hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        file.status = static_cast<FileStatus>(sqlite3_column_int(stmt, 7));
        file.chunks = ListChunks(file.file_id);
        files.push_back(file);
    }

    sqlite3_finalize(stmt);
    return files;
}

std::vector<StoredObjectRecord> SqliteMetadataStore::ListStoredObjects() const {
    static const char *kSql =
        "SELECT file_id, backend_name, storage_key, access_url, source_path, stored_at_epoch "
        "FROM stored_objects ORDER BY stored_at_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);

    std::vector<StoredObjectRecord> records;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StoredObjectRecord record;
        record.file_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        record.backend_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        record.storage_key = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        record.access_url = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        record.source_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        record.stored_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 5));
        records.push_back(record);
    }

    sqlite3_finalize(stmt);
    return records;
}

std::vector<BackupRecord> SqliteMetadataStore::ListBackupRecords() const {
    static const char *kSql =
        "SELECT record_id, job_id, node_id, file_id, relative_path, source_path, backend_name, storage_key, "
        "backed_up_at_epoch FROM backup_records ORDER BY backed_up_at_epoch";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);

    std::vector<BackupRecord> records;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        BackupRecord record;
        record.record_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        record.job_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        record.node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        record.file_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        record.relative_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        record.source_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
        record.backend_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        record.storage_key = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        record.backed_up_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 8));
        records.push_back(record);
    }

    sqlite3_finalize(stmt);
    return records;
}

std::vector<DeviceFileRecord> SqliteMetadataStore::ListDeviceFiles() const {
    static const char *kSql =
        "SELECT record_id, node_id, file_id, relative_path, absolute_path, source_kind, source_ref_id, "
        "updated_at_epoch FROM device_file_records ORDER BY node_id, absolute_path";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);

    std::vector<DeviceFileRecord> records;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DeviceFileRecord record;
        record.record_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        record.node_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        record.file_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        record.relative_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        record.absolute_path = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        record.source_kind = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
        record.source_ref_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        record.updated_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 7));
        records.push_back(record);
    }

    sqlite3_finalize(stmt);
    return records;
}

std::vector<AuditEvent> SqliteMetadataStore::ListAuditEvents() const {
    static const char *kSql =
        "SELECT event_id, category, detail, created_at_epoch FROM audit_events ORDER BY created_at_epoch, event_id";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);

    std::vector<AuditEvent> events;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AuditEvent event;
        event.event_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        event.category = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        event.detail = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        event.created_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 3));
        events.push_back(event);
    }

    sqlite3_finalize(stmt);
    return events;
}

std::vector<AuditEvent> SqliteMetadataStore::ListAuditEventsByCategory(const std::string &category) const {
    static const char *kSql =
        "SELECT event_id, category, detail, created_at_epoch FROM audit_events WHERE category = ? "
        "ORDER BY created_at_epoch, event_id";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, category);

    std::vector<AuditEvent> events;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AuditEvent event;
        event.event_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        event.category = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        event.detail = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        event.created_at_epoch = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 3));
        events.push_back(event);
    }

    sqlite3_finalize(stmt);
    return events;
}

std::vector<ChunkRecord> SqliteMetadataStore::ListChunks(const std::string &file_id) const {
    static const char *kSql =
        "SELECT chunk_index, chunk_offset, chunk_size, chunk_hash "
        "FROM chunk_records WHERE file_id = ? ORDER BY chunk_index";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, file_id);

    std::vector<ChunkRecord> chunks;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ChunkRecord chunk;
        chunk.file_id = file_id;
        chunk.chunk_index = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 0));
        chunk.offset = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1));
        chunk.size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 2));
        chunk.chunk_hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        chunks.push_back(chunk);
    }

    sqlite3_finalize(stmt);
    return chunks;
}

std::vector<SyncTaskChunk> SqliteMetadataStore::ListTaskChunks(const std::string &task_id) const {
    static const char *kSql =
        "SELECT file_id, chunk_index, chunk_offset, chunk_size, chunk_hash "
        "FROM sync_task_chunks WHERE task_id = ? ORDER BY chunk_index";

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(impl_->db, kSql, -1, &stmt, nullptr);
    BindText(stmt, 1, task_id);

    std::vector<SyncTaskChunk> chunks;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SyncTaskChunk chunk;
        chunk.task_id = task_id;
        chunk.file_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        chunk.chunk_index = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 1));
        chunk.offset = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 2));
        chunk.size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 3));
        chunk.chunk_hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        chunks.push_back(chunk);
    }

    sqlite3_finalize(stmt);
    return chunks;
}

std::optional<std::filesystem::path> SqliteMetadataStore::GetLocalMetadataPath() const {
    return impl_->db_path;
}

}  // namespace sync
