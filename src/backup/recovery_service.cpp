#include "netdisk/backup/recovery_service.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>

#include "netdisk/sync/chunker.h"
#include "netdisk/sync/hash.h"

namespace {

sync::AuditEvent MakeRecoveryAuditEvent(const std::string &detail) {
    sync::AuditEvent event;
    event.category = "restore.request";
    event.detail = detail;
    return event;
}

std::uint64_t NowEpoch() {
    return static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

std::optional<sync::BackupRecord> FindLatestBackupRecord(sync::MetadataStore &store,
                                                         const std::string &source_node_id,
                                                         const std::string &relative_path) {
    std::optional<sync::BackupRecord> latest;
    for (const auto &record : store.ListBackupRecords()) {
        if (record.node_id != source_node_id || record.relative_path != relative_path) {
            continue;
        }
        if (!latest.has_value() || record.backed_up_at_epoch > latest->backed_up_at_epoch) {
            latest = record;
        }
    }
    return latest;
}

netdisk::Status VerifyRestoredFile(sync::MetadataStore &store,
                                   const sync::StoredObjectRecord &stored,
                                   const std::filesystem::path &target_path,
                                   sync::FileRecord *restored_out = nullptr) {
    auto file = store.GetFile(stored.file_id);
    if (!file.has_value()) {
        return netdisk::Status::Error(netdisk::StatusCode::kNotFound, "file metadata not found for restore");
    }

    sync::ChunkerOptions options;
    options.chunk_size = 4 * 1024 * 1024;
    sync::Chunker chunker(options, std::make_unique<sync::Md5Hasher>());
    sync::FileRecord restored = chunker.BuildFileRecord(target_path, target_path.parent_path());
    if (restored.root_hash != file->root_hash) {
        return netdisk::Status::Error(netdisk::StatusCode::kInternalError, "restored file hash does not match metadata");
    }
    if (restored_out != nullptr) {
        *restored_out = std::move(restored);
    }
    return netdisk::Status::Ok();
}

std::string MakeDeviceRelativePath(const std::filesystem::path &target_path) {
    const auto normalized = target_path.lexically_normal();
    if (normalized.is_absolute()) {
        return normalized.filename().string();
    }
    return normalized.string();
}

sync::DeviceFileRecord MakeRestoredDeviceFileRecord(const sync::RestoreRequest &request,
                                                    const sync::FileRecord &restored_file) {
    sync::DeviceFileRecord record;
    record.record_id = request.target_node_id + "|" + restored_file.absolute_path;
    record.node_id = request.target_node_id;
    record.file_id = request.file_id;
    record.relative_path = MakeDeviceRelativePath(restored_file.absolute_path);
    record.absolute_path = restored_file.absolute_path;
    record.source_kind = "restore";
    record.source_ref_id = request.request_id;
    record.updated_at_epoch = NowEpoch();
    return record;
}

}  // namespace

namespace netdisk {

RecoveryService::RecoveryService(TaskScheduler &scheduler, AuditLogger &audit_logger, sync::MetadataStore &store)
    : scheduler_(scheduler), audit_logger_(audit_logger), store_(store) {}

Status RecoveryService::SubmitRestore(const sync::RestoreRequest &request) {
    if (request.request_id.empty() || request.file_id.empty() || request.target_node_id.empty() ||
        request.destination_path.empty()) {
        return Status::Error(StatusCode::kInvalidArgument, "restore request is missing required fields");
    }
    sync::ScheduledTaskRecord task;
    task.task_id = request.request_id;
    task.domain = sync::JobDomain::kRestore;
    task.related_id = request.file_id;
    task.source_node = request.source_node_id;
    task.target_node = request.target_node_id;
    task.created_at_epoch = request.created_at_epoch;
    task.detail = request.destination_path;
    sync::RestoreRequest stored_request = request;
    stored_request.state = sync::RestoreRequestState::kQueued;
    stored_request.completed_at_epoch = 0;
    stored_request.error_message.clear();
    const Status status = scheduler_.ScheduleTask(task);
    if (status.ok()) {
        store_.UpsertRestoreRequest(stored_request);
        audit_logger_.Append(MakeRecoveryAuditEvent("scheduled restore request: " + stored_request.request_id));
    }
    return status;
}

Status RecoveryService::ExecuteRestore(const std::string &request_id) {
    auto request = store_.GetRestoreRequest(request_id);
    if (!request.has_value()) {
        return Status::Error(StatusCode::kNotFound, "restore request not found");
    }

    auto stored = store_.GetStoredObject(request->file_id);
    if (!stored.has_value()) {
        scheduler_.UpdateTaskState(request_id, sync::JobState::kFailed, "stored object not found");
        return Status::Error(StatusCode::kNotFound, "stored object not found");
    }

    auto file = store_.GetFile(request->file_id);
    if (!file.has_value()) {
        scheduler_.UpdateTaskState(request_id, sync::JobState::kFailed, "file metadata not found");
        return Status::Error(StatusCode::kNotFound, "file metadata not found");
    }

    request->state = sync::RestoreRequestState::kRunning;
    request->error_message.clear();
    request->completed_at_epoch = 0;
    store_.UpsertRestoreRequest(*request);
    scheduler_.UpdateTaskState(request_id, sync::JobState::kRunning, request->destination_path);

    try {
        auto backend = sync::CreateStorageBackend(stored->backend_name, "");
        const std::filesystem::path target_path(request->destination_path);
        backend->RestoreFile(*stored, target_path);
        sync::FileRecord restored_file;
        const Status verify_status = VerifyRestoredFile(store_, *stored, target_path, &restored_file);
        if (!verify_status.ok()) {
            request->state = sync::RestoreRequestState::kFailed;
            request->error_message = verify_status.message;
            request->completed_at_epoch = NowEpoch();
            store_.UpsertRestoreRequest(*request);
            scheduler_.UpdateTaskState(request_id, sync::JobState::kFailed, verify_status.message);
            return verify_status;
        }
        store_.UpsertDeviceFile(MakeRestoredDeviceFileRecord(*request, restored_file));
        request->state = sync::RestoreRequestState::kCompleted;
        request->error_message.clear();
        request->completed_at_epoch = NowEpoch();
        store_.UpsertRestoreRequest(*request);
        scheduler_.UpdateTaskState(request_id, sync::JobState::kCompleted, target_path.lexically_normal().string());
        audit_logger_.Append(MakeRecoveryAuditEvent("completed restore request: " + request_id));
        return Status::Ok();
    } catch (const std::exception &ex) {
        request->state = sync::RestoreRequestState::kFailed;
        request->error_message = ex.what();
        request->completed_at_epoch = NowEpoch();
        store_.UpsertRestoreRequest(*request);
        scheduler_.UpdateTaskState(request_id, sync::JobState::kFailed, ex.what());
        return Status::Error(StatusCode::kInternalError, ex.what());
    }
}

Status RecoveryService::ExecuteLatestRestore(const std::string &source_node_id,
                                             const std::string &relative_path,
                                             const std::string &target_node_id,
                                             const std::string &destination_path) {
    const auto latest = FindLatestBackupRecord(store_, source_node_id, relative_path);
    if (!latest.has_value()) {
        return Status::Error(StatusCode::kNotFound, "latest backup record not found");
    }

    sync::RestoreRequest request;
    request.request_id = "restore-" + source_node_id + "-" + latest->file_id;
    request.file_id = latest->file_id;
    request.source_node_id = source_node_id;
    request.target_node_id = target_node_id;
    request.destination_path = destination_path;
    request.created_at_epoch = latest->backed_up_at_epoch;

    const auto submit_status = SubmitRestore(request);
    if (!submit_status.ok()) {
        return submit_status;
    }
    return ExecuteRestore(request.request_id);
}

Status RecoveryService::RetryRestore(const std::string &request_id) {
    auto request = store_.GetRestoreRequest(request_id);
    if (!request.has_value()) {
        return Status::Error(StatusCode::kNotFound, "restore request not found");
    }
    const auto retry_status = scheduler_.RetryTask(request_id, "manual restore retry requested");
    if (!retry_status.ok()) {
        return retry_status;
    }
    request->state = sync::RestoreRequestState::kQueued;
    request->completed_at_epoch = 0;
    request->error_message.clear();
    store_.UpsertRestoreRequest(*request);
    audit_logger_.Append(MakeRecoveryAuditEvent("retry restore request: " + request_id));
    return ExecuteRestore(request_id);
}

std::optional<sync::RestoreRequest> RecoveryService::FindRestoreRequest(const std::string &request_id) const {
    return store_.GetRestoreRequest(request_id);
}

std::vector<sync::RestoreRequest> RecoveryService::ListRestoreRequests() const {
    return store_.ListRestoreRequests();
}

}  // namespace netdisk
