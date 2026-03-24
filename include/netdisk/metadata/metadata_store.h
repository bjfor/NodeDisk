#ifndef SYNC_METADATA_STORE_H
#define SYNC_METADATA_STORE_H

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "netdisk/core/types.h"

namespace sync {

class MetadataStore {
public:
    virtual ~MetadataStore() = default;

    virtual void UpsertNode(const NodeInfo &node) = 0;
    virtual void UpsertFile(const FileRecord &file) = 0;
    virtual void UpsertTask(const SyncTask &task) = 0;
    virtual void MarkTaskChunkComplete(const SyncTaskChunk &chunk) = 0;
    virtual void ClearTaskChunks(const std::string &task_id) = 0;
    virtual void UpsertStoredObject(const StoredObjectRecord &record) = 0;
    virtual void UpsertBackupRecord(const BackupRecord &record) = 0;

    virtual std::optional<FileRecord> GetFileByPath(const std::string &absolute_path) const = 0;
    virtual std::optional<SyncTask> GetTask(const std::string &task_id) const = 0;
    virtual std::optional<StoredObjectRecord> GetStoredObject(const std::string &file_id) const = 0;
    virtual std::vector<FileRecord> ListFiles() const = 0;
    virtual std::vector<StoredObjectRecord> ListStoredObjects() const = 0;
    virtual std::vector<BackupRecord> ListBackupRecords() const = 0;
    virtual std::vector<ChunkRecord> ListChunks(const std::string &file_id) const = 0;
    virtual std::vector<SyncTaskChunk> ListTaskChunks(const std::string &task_id) const = 0;
    virtual std::optional<std::filesystem::path> GetLocalMetadataPath() const = 0;
};

// Phase 1 starts with an in-memory backend so the sync engine contracts
// can stabilize before we add SQLite or RocksDB persistence.
class InMemoryMetadataStore : public MetadataStore {
public:
    void UpsertNode(const NodeInfo &node) override;
    void UpsertFile(const FileRecord &file) override;
    void UpsertTask(const SyncTask &task) override;
    void MarkTaskChunkComplete(const SyncTaskChunk &chunk) override;
    void ClearTaskChunks(const std::string &task_id) override;
    void UpsertStoredObject(const StoredObjectRecord &record) override;
    void UpsertBackupRecord(const BackupRecord &record) override;

    std::optional<FileRecord> GetFileByPath(const std::string &absolute_path) const override;
    std::optional<SyncTask> GetTask(const std::string &task_id) const override;
    std::optional<StoredObjectRecord> GetStoredObject(const std::string &file_id) const override;
    std::vector<FileRecord> ListFiles() const override;
    std::vector<StoredObjectRecord> ListStoredObjects() const override;
    std::vector<BackupRecord> ListBackupRecords() const override;
    std::vector<ChunkRecord> ListChunks(const std::string &file_id) const override;
    std::vector<SyncTaskChunk> ListTaskChunks(const std::string &task_id) const override;
    std::optional<std::filesystem::path> GetLocalMetadataPath() const override;

private:
    std::vector<NodeInfo> nodes_;
    std::vector<FileRecord> files_;
    std::vector<SyncTask> tasks_;
    std::vector<SyncTaskChunk> task_chunks_;
    std::vector<StoredObjectRecord> stored_objects_;
    std::vector<BackupRecord> backup_records_;
};

class SqliteMetadataStore : public MetadataStore {
public:
    explicit SqliteMetadataStore(const std::string &db_path);
    ~SqliteMetadataStore() override;

    SqliteMetadataStore(const SqliteMetadataStore &) = delete;
    SqliteMetadataStore &operator=(const SqliteMetadataStore &) = delete;

    void UpsertNode(const NodeInfo &node) override;
    void UpsertFile(const FileRecord &file) override;
    void UpsertTask(const SyncTask &task) override;
    void MarkTaskChunkComplete(const SyncTaskChunk &chunk) override;
    void ClearTaskChunks(const std::string &task_id) override;
    void UpsertStoredObject(const StoredObjectRecord &record) override;
    void UpsertBackupRecord(const BackupRecord &record) override;

    std::optional<FileRecord> GetFileByPath(const std::string &absolute_path) const override;
    std::optional<SyncTask> GetTask(const std::string &task_id) const override;
    std::optional<StoredObjectRecord> GetStoredObject(const std::string &file_id) const override;
    std::vector<FileRecord> ListFiles() const override;
    std::vector<StoredObjectRecord> ListStoredObjects() const override;
    std::vector<BackupRecord> ListBackupRecords() const override;
    std::vector<ChunkRecord> ListChunks(const std::string &file_id) const override;
    std::vector<SyncTaskChunk> ListTaskChunks(const std::string &task_id) const override;
    std::optional<std::filesystem::path> GetLocalMetadataPath() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void InitializeSchema();
};

}  // namespace sync

#endif
