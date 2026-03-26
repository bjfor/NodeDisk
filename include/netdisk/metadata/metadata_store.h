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
    virtual void UpsertBackupPolicy(const BackupPolicy &policy) = 0;
    virtual void UpsertTransferPolicy(const TransferPolicy &policy) = 0;
    virtual void UpsertScheduledTask(const ScheduledTaskRecord &task) = 0;
    virtual void UpsertBackupJob(const BackupJob &job) = 0;
    virtual void UpsertRestoreRequest(const RestoreRequest &request) = 0;
    virtual void UpsertSharedLibraryEntry(const SharedLibraryEntry &entry) = 0;
    virtual void UpsertFile(const FileRecord &file) = 0;
    virtual void UpsertTask(const SyncTask &task) = 0;
    virtual void MarkTaskChunkComplete(const SyncTaskChunk &chunk) = 0;
    virtual void ClearTaskChunks(const std::string &task_id) = 0;
    virtual void UpsertStoredObject(const StoredObjectRecord &record) = 0;
    virtual void UpsertBackupRecord(const BackupRecord &record) = 0;
    virtual void UpsertDeviceFile(const DeviceFileRecord &record) = 0;
    virtual void AppendAuditEvent(const AuditEvent &event) = 0;

    virtual std::optional<NodeInfo> GetNode(const std::string &node_id) const = 0;
    virtual std::optional<BackupPolicy> GetBackupPolicy(const std::string &policy_id) const = 0;
    virtual std::optional<TransferPolicy> GetTransferPolicy(const std::string &policy_id) const = 0;
    virtual std::optional<ScheduledTaskRecord> GetScheduledTaskRecord(const std::string &task_id) const = 0;
    virtual std::optional<BackupJob> GetBackupJob(const std::string &job_id) const = 0;
    virtual std::optional<RestoreRequest> GetRestoreRequest(const std::string &request_id) const = 0;
    virtual std::optional<SharedLibraryEntry> GetSharedLibraryEntry(const std::string &entry_id) const = 0;
    virtual std::optional<FileRecord> GetFile(const std::string &file_id) const = 0;
    virtual std::optional<FileRecord> GetFileByPath(const std::string &absolute_path) const = 0;
    virtual std::optional<SyncTask> GetTask(const std::string &task_id) const = 0;
    virtual std::optional<StoredObjectRecord> GetStoredObject(const std::string &file_id) const = 0;
    virtual std::vector<NodeInfo> ListNodes() const = 0;
    virtual std::vector<BackupPolicy> ListBackupPolicies() const = 0;
    virtual std::vector<TransferPolicy> ListTransferPolicies() const = 0;
    virtual std::vector<ScheduledTaskRecord> ListScheduledTasks() const = 0;
    virtual std::vector<ScheduledTaskRecord> ListScheduledTasksByState(JobState state) const = 0;
    virtual std::vector<BackupJob> ListBackupJobs() const = 0;
    virtual std::vector<RestoreRequest> ListRestoreRequests() const = 0;
    virtual std::vector<SharedLibraryEntry> ListSharedLibraryEntries() const = 0;
    virtual std::vector<FileRecord> ListFiles() const = 0;
    virtual std::vector<StoredObjectRecord> ListStoredObjects() const = 0;
    virtual std::vector<BackupRecord> ListBackupRecords() const = 0;
    virtual std::vector<DeviceFileRecord> ListDeviceFiles() const = 0;
    virtual std::vector<AuditEvent> ListAuditEvents() const = 0;
    virtual std::vector<AuditEvent> ListAuditEventsByCategory(const std::string &category) const = 0;
    virtual std::vector<ChunkRecord> ListChunks(const std::string &file_id) const = 0;
    virtual std::vector<SyncTaskChunk> ListTaskChunks(const std::string &task_id) const = 0;
    virtual std::optional<std::filesystem::path> GetLocalMetadataPath() const = 0;
};

// Phase 1 starts with an in-memory backend so the sync engine contracts
// can stabilize before we add SQLite or RocksDB persistence.
class InMemoryMetadataStore : public MetadataStore {
public:
    void UpsertNode(const NodeInfo &node) override;
    void UpsertBackupPolicy(const BackupPolicy &policy) override;
    void UpsertTransferPolicy(const TransferPolicy &policy) override;
    void UpsertScheduledTask(const ScheduledTaskRecord &task) override;
    void UpsertBackupJob(const BackupJob &job) override;
    void UpsertRestoreRequest(const RestoreRequest &request) override;
    void UpsertSharedLibraryEntry(const SharedLibraryEntry &entry) override;
    void UpsertFile(const FileRecord &file) override;
    void UpsertTask(const SyncTask &task) override;
    void MarkTaskChunkComplete(const SyncTaskChunk &chunk) override;
    void ClearTaskChunks(const std::string &task_id) override;
    void UpsertStoredObject(const StoredObjectRecord &record) override;
    void UpsertBackupRecord(const BackupRecord &record) override;
    void UpsertDeviceFile(const DeviceFileRecord &record) override;
    void AppendAuditEvent(const AuditEvent &event) override;

    std::optional<NodeInfo> GetNode(const std::string &node_id) const override;
    std::optional<BackupPolicy> GetBackupPolicy(const std::string &policy_id) const override;
    std::optional<TransferPolicy> GetTransferPolicy(const std::string &policy_id) const override;
    std::optional<ScheduledTaskRecord> GetScheduledTaskRecord(const std::string &task_id) const override;
    std::optional<BackupJob> GetBackupJob(const std::string &job_id) const override;
    std::optional<RestoreRequest> GetRestoreRequest(const std::string &request_id) const override;
    std::optional<SharedLibraryEntry> GetSharedLibraryEntry(const std::string &entry_id) const override;
    std::optional<FileRecord> GetFile(const std::string &file_id) const override;
    std::optional<FileRecord> GetFileByPath(const std::string &absolute_path) const override;
    std::optional<SyncTask> GetTask(const std::string &task_id) const override;
    std::optional<StoredObjectRecord> GetStoredObject(const std::string &file_id) const override;
    std::vector<NodeInfo> ListNodes() const override;
    std::vector<BackupPolicy> ListBackupPolicies() const override;
    std::vector<TransferPolicy> ListTransferPolicies() const override;
    std::vector<ScheduledTaskRecord> ListScheduledTasks() const override;
    std::vector<ScheduledTaskRecord> ListScheduledTasksByState(JobState state) const override;
    std::vector<BackupJob> ListBackupJobs() const override;
    std::vector<RestoreRequest> ListRestoreRequests() const override;
    std::vector<SharedLibraryEntry> ListSharedLibraryEntries() const override;
    std::vector<FileRecord> ListFiles() const override;
    std::vector<StoredObjectRecord> ListStoredObjects() const override;
    std::vector<BackupRecord> ListBackupRecords() const override;
    std::vector<DeviceFileRecord> ListDeviceFiles() const override;
    std::vector<AuditEvent> ListAuditEvents() const override;
    std::vector<AuditEvent> ListAuditEventsByCategory(const std::string &category) const override;
    std::vector<ChunkRecord> ListChunks(const std::string &file_id) const override;
    std::vector<SyncTaskChunk> ListTaskChunks(const std::string &task_id) const override;
    std::optional<std::filesystem::path> GetLocalMetadataPath() const override;

private:
    std::vector<NodeInfo> nodes_;
    std::vector<BackupPolicy> backup_policies_;
    std::vector<TransferPolicy> transfer_policies_;
    std::vector<ScheduledTaskRecord> scheduled_tasks_;
    std::vector<BackupJob> backup_jobs_;
    std::vector<RestoreRequest> restore_requests_;
    std::vector<SharedLibraryEntry> shared_library_entries_;
    std::vector<FileRecord> files_;
    std::vector<SyncTask> tasks_;
    std::vector<SyncTaskChunk> task_chunks_;
    std::vector<StoredObjectRecord> stored_objects_;
    std::vector<BackupRecord> backup_records_;
    std::vector<DeviceFileRecord> device_files_;
    std::vector<AuditEvent> audit_events_;
};

class SqliteMetadataStore : public MetadataStore {
public:
    explicit SqliteMetadataStore(const std::string &db_path);
    ~SqliteMetadataStore() override;

    SqliteMetadataStore(const SqliteMetadataStore &) = delete;
    SqliteMetadataStore &operator=(const SqliteMetadataStore &) = delete;

    void UpsertNode(const NodeInfo &node) override;
    void UpsertBackupPolicy(const BackupPolicy &policy) override;
    void UpsertTransferPolicy(const TransferPolicy &policy) override;
    void UpsertScheduledTask(const ScheduledTaskRecord &task) override;
    void UpsertBackupJob(const BackupJob &job) override;
    void UpsertRestoreRequest(const RestoreRequest &request) override;
    void UpsertSharedLibraryEntry(const SharedLibraryEntry &entry) override;
    void UpsertFile(const FileRecord &file) override;
    void UpsertTask(const SyncTask &task) override;
    void MarkTaskChunkComplete(const SyncTaskChunk &chunk) override;
    void ClearTaskChunks(const std::string &task_id) override;
    void UpsertStoredObject(const StoredObjectRecord &record) override;
    void UpsertBackupRecord(const BackupRecord &record) override;
    void UpsertDeviceFile(const DeviceFileRecord &record) override;
    void AppendAuditEvent(const AuditEvent &event) override;

    std::optional<NodeInfo> GetNode(const std::string &node_id) const override;
    std::optional<BackupPolicy> GetBackupPolicy(const std::string &policy_id) const override;
    std::optional<TransferPolicy> GetTransferPolicy(const std::string &policy_id) const override;
    std::optional<ScheduledTaskRecord> GetScheduledTaskRecord(const std::string &task_id) const override;
    std::optional<BackupJob> GetBackupJob(const std::string &job_id) const override;
    std::optional<RestoreRequest> GetRestoreRequest(const std::string &request_id) const override;
    std::optional<SharedLibraryEntry> GetSharedLibraryEntry(const std::string &entry_id) const override;
    std::optional<FileRecord> GetFile(const std::string &file_id) const override;
    std::optional<FileRecord> GetFileByPath(const std::string &absolute_path) const override;
    std::optional<SyncTask> GetTask(const std::string &task_id) const override;
    std::optional<StoredObjectRecord> GetStoredObject(const std::string &file_id) const override;
    std::vector<NodeInfo> ListNodes() const override;
    std::vector<BackupPolicy> ListBackupPolicies() const override;
    std::vector<TransferPolicy> ListTransferPolicies() const override;
    std::vector<ScheduledTaskRecord> ListScheduledTasks() const override;
    std::vector<ScheduledTaskRecord> ListScheduledTasksByState(JobState state) const override;
    std::vector<BackupJob> ListBackupJobs() const override;
    std::vector<RestoreRequest> ListRestoreRequests() const override;
    std::vector<SharedLibraryEntry> ListSharedLibraryEntries() const override;
    std::vector<FileRecord> ListFiles() const override;
    std::vector<StoredObjectRecord> ListStoredObjects() const override;
    std::vector<BackupRecord> ListBackupRecords() const override;
    std::vector<DeviceFileRecord> ListDeviceFiles() const override;
    std::vector<AuditEvent> ListAuditEvents() const override;
    std::vector<AuditEvent> ListAuditEventsByCategory(const std::string &category) const override;
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
