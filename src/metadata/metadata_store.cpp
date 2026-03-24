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

std::vector<FileRecord> InMemoryMetadataStore::ListFiles() const {
    return files_;
}

std::vector<StoredObjectRecord> InMemoryMetadataStore::ListStoredObjects() const {
    return stored_objects_;
}

std::vector<BackupRecord> InMemoryMetadataStore::ListBackupRecords() const {
    return backup_records_;
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
)SQL";

    ExecOrThrow(impl_->db, "PRAGMA foreign_keys = ON;");
    ExecOrThrow(impl_->db, kSchema);
    EnsureColumnExists(impl_->db, "sync_tasks", "relative_path", "TEXT NOT NULL DEFAULT ''");
    EnsureColumnExists(impl_->db, "sync_tasks", "expected_root_hash", "TEXT NOT NULL DEFAULT ''");
    EnsureColumnExists(impl_->db, "sync_tasks", "temp_path", "TEXT NOT NULL DEFAULT ''");
    EnsureColumnExists(impl_->db, "sync_tasks", "total_chunks", "INTEGER NOT NULL DEFAULT 0");
    EnsureColumnExists(impl_->db, "sync_tasks", "completed_chunks", "INTEGER NOT NULL DEFAULT 0");
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
