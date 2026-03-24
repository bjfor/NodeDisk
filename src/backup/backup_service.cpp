#include "netdisk/backup/backup_service.h"

#include <filesystem>
#include <memory>

#include "netdisk/sync/chunker.h"
#include "netdisk/sync/hash.h"
#include "netdisk/sync/scanner.h"

namespace {

sync::AuditEvent MakeAuditEvent(const std::string &category, const std::string &detail) {
    sync::AuditEvent event;
    event.category = category;
    event.detail = detail;
    return event;
}

std::vector<sync::FileRecord> BuildBackupFiles(sync::MetadataStore &metadata_store,
                                               const std::filesystem::path &source_path) {
    sync::ChunkerOptions options;
    options.chunk_size = 4 * 1024 * 1024;
    sync::Chunker chunker(options, std::make_unique<sync::Md5Hasher>());

    if (std::filesystem::is_regular_file(source_path)) {
        sync::FileRecord file = chunker.BuildFileRecord(source_path, source_path.parent_path());
        metadata_store.UpsertFile(file);
        return {file};
    }

    std::vector<std::filesystem::path> excluded_paths;
    if (const auto metadata_path = metadata_store.GetLocalMetadataPath(); metadata_path.has_value()) {
        excluded_paths.push_back(metadata_path->lexically_normal());
    }

    sync::Scanner scanner(chunker, metadata_store);
    return scanner.ScanDirectory(source_path, excluded_paths);
}

sync::BackupRecord MakeBackupRecord(const sync::BackupJob &job,
                                    const sync::FileRecord &file,
                                    const sync::StoredObjectRecord &stored) {
    sync::BackupRecord record;
    record.record_id = job.node_id + "|" + file.absolute_path + "|" + file.file_id;
    record.job_id = job.job_id;
    record.node_id = job.node_id;
    record.file_id = file.file_id;
    record.relative_path = file.relative_path;
    record.source_path = file.absolute_path;
    record.backend_name = stored.backend_name;
    record.storage_key = stored.storage_key;
    record.backed_up_at_epoch = stored.stored_at_epoch;
    return record;
}

}  // namespace

namespace netdisk {

BackupService::BackupService(TaskScheduler &scheduler,
                             PolicyManager &policies,
                             AuditLogger &audit_logger,
                             sync::MetadataStore &metadata_store,
                             IntegrityChecker &integrity_checker)
    : scheduler_(scheduler),
      policies_(policies),
      audit_logger_(audit_logger),
      metadata_store_(metadata_store),
      integrity_checker_(integrity_checker) {}

Status BackupService::ConfigurePolicy(const sync::BackupPolicy &policy) {
    if (policy.policy_id.empty() || policy.node_id.empty() || policy.source_path.empty()) {
        return Status::Error(StatusCode::kInvalidArgument, "backup policy is missing required fields");
    }
    const Status status = policies_.UpsertBackupPolicy(policy);
    if (status.ok()) {
        audit_logger_.Append(MakeAuditEvent("backup.policy", "configured backup policy: " + policy.policy_id));
    }
    return status;
}

Status BackupService::StartBackup(const sync::BackupJob &job) {
    if (job.job_id.empty() || job.node_id.empty() || job.source_path.empty()) {
        return Status::Error(StatusCode::kInvalidArgument, "backup job is missing required fields");
    }
    if (policies_.ListBackupPoliciesForNode(job.node_id).empty()) {
        return Status::Error(StatusCode::kNotFound, "no backup policy configured for node");
    }

    sync::ScheduledTaskRecord task;
    task.task_id = job.job_id;
    task.domain = sync::JobDomain::kBackup;
    task.related_id = job.job_id;
    task.source_node = job.node_id;
    task.target_node = job.node_id;
    task.detail = job.source_path;
    const Status status = scheduler_.ScheduleTask(task);
    if (status.ok()) {
        bool updated = false;
        for (auto &current : jobs_) {
            if (current.job_id == job.job_id) {
                current = job;
                updated = true;
                break;
            }
        }
        if (!updated) {
            jobs_.push_back(job);
        }
        audit_logger_.Append(MakeAuditEvent("backup.job", "scheduled backup job: " + job.job_id));
    }
    return status;
}

Status BackupService::ExecuteBackup(const sync::BackupJob &job, sync::IStorageBackend &backend) {
    const Status scheduled = StartBackup(job);
    if (!scheduled.ok() && scheduled.code != StatusCode::kAlreadyExists) {
        return scheduled;
    }

    if (!std::filesystem::exists(job.source_path)) {
        scheduler_.UpdateTaskState(job.job_id, sync::JobState::kFailed, "backup source path does not exist");
        return Status::Error(StatusCode::kNotFound, "backup source path does not exist");
    }

    scheduler_.UpdateTaskState(job.job_id, sync::JobState::kRunning, "backup execution started");

    std::vector<sync::FileRecord> files;
    try {
        files = BuildBackupFiles(metadata_store_, job.source_path);
    } catch (const std::exception &ex) {
        scheduler_.UpdateTaskState(job.job_id, sync::JobState::kFailed, ex.what());
        return Status::Error(StatusCode::kInternalError, ex.what());
    }

    if (files.empty()) {
        scheduler_.UpdateTaskState(job.job_id, sync::JobState::kFailed, "no files discovered for backup");
        return Status::Error(StatusCode::kNotFound, "no files discovered for backup");
    }

    for (const auto &file : files) {
        const Status verify_status = integrity_checker_.VerifyFile(file, file.absolute_path);
        if (!verify_status.ok()) {
            scheduler_.UpdateTaskState(job.job_id, sync::JobState::kFailed, verify_status.message);
            return verify_status;
        }

        try {
            const auto stored = backend.StoreFile(file, file.absolute_path);
            metadata_store_.UpsertStoredObject(stored);
            metadata_store_.UpsertBackupRecord(MakeBackupRecord(job, file, stored));
        } catch (const std::exception &ex) {
            scheduler_.UpdateTaskState(job.job_id, sync::JobState::kFailed, ex.what());
            return Status::Error(StatusCode::kInternalError, ex.what());
        }
    }

    scheduler_.UpdateTaskState(job.job_id,
                               sync::JobState::kCompleted,
                               "backed up " + std::to_string(files.size()) + " file(s) via " + backend.Name());
    audit_logger_.Append(MakeAuditEvent("backup.execute",
                                        "completed backup job: " + job.job_id + " file_count=" +
                                            std::to_string(files.size())));
    return Status::Ok();
}

std::optional<sync::BackupJob> BackupService::FindJob(const std::string &job_id) const {
    for (const auto &job : jobs_) {
        if (job.job_id == job_id) {
            return job;
        }
    }
    return std::nullopt;
}

std::vector<sync::BackupJob> BackupService::ListJobs() const {
    return jobs_;
}

std::vector<sync::BackupJob> BackupService::ListJobsForNode(const std::string &node_id) const {
    std::vector<sync::BackupJob> result;
    for (const auto &job : jobs_) {
        if (job.node_id == node_id) {
            result.push_back(job);
        }
    }
    return result;
}

}  // namespace netdisk
