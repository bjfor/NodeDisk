#include "netdisk/backup/recovery_service.h"

namespace {

sync::AuditEvent MakeRecoveryAuditEvent(const std::string &detail) {
    sync::AuditEvent event;
    event.category = "restore.request";
    event.detail = detail;
    return event;
}

}  // namespace

namespace netdisk {

RecoveryService::RecoveryService(TaskScheduler &scheduler, AuditLogger &audit_logger)
    : scheduler_(scheduler), audit_logger_(audit_logger) {}

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
    task.detail = request.destination_path;
    const Status status = scheduler_.ScheduleTask(task);
    if (status.ok()) {
        bool updated = false;
        for (auto &current : requests_) {
            if (current.request_id == request.request_id) {
                current = request;
                updated = true;
                break;
            }
        }
        if (!updated) {
            requests_.push_back(request);
        }
        audit_logger_.Append(MakeRecoveryAuditEvent("scheduled restore request: " + request.request_id));
    }
    return status;
}

std::optional<sync::RestoreRequest> RecoveryService::FindRestoreRequest(const std::string &request_id) const {
    for (const auto &request : requests_) {
        if (request.request_id == request_id) {
            return request;
        }
    }
    return std::nullopt;
}

std::vector<sync::RestoreRequest> RecoveryService::ListRestoreRequests() const {
    return requests_;
}

}  // namespace netdisk
