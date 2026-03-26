#include "netdisk/control_plane/control_plane_service.h"

namespace netdisk {

ControlPlaneService::ControlPlaneService(DeviceManager &devices,
                                         PolicyManager &policies,
                                         TaskScheduler &tasks,
                                         NodeNetworkService &network,
                                         AuditLogger &audit,
                                         BackupService &backup,
                                         RecoveryService &recovery,
                                         SharedLibraryService &shared_library,
                                         FileIndexService &file_index)
    : devices_(devices),
      policies_(policies),
      tasks_(tasks),
      network_(network),
      audit_(audit),
      backup_(backup),
      recovery_(recovery),
      shared_library_(shared_library),
      file_index_(file_index) {}

void ControlPlaneService::RefreshNetworkView(std::uint64_t now_epoch, std::uint64_t offline_after_seconds) {
    network_.RefreshPeersFromDevices(devices_.ListDevices(), now_epoch, offline_after_seconds);
}

ControlPlaneSummary ControlPlaneService::BuildSummary(std::uint64_t now_epoch, std::uint64_t offline_after_seconds) {
    RefreshNetworkView(now_epoch, offline_after_seconds);

    ControlPlaneSummary summary;
    summary.devices = devices_.ListDevices().size();
    summary.online_devices = devices_.ListOnlineDevices(now_epoch, offline_after_seconds).size();
    summary.peers = network_.ListPeers().size();
    summary.online_peers = network_.ListOnlinePeers().size();
    summary.backup_policies = policies_.ListBackupPolicies().size();
    summary.transfer_policies = policies_.ListTransferPolicies().size();
    summary.scheduled_tasks = tasks_.ListTasks().size();
    summary.queued_tasks = tasks_.ListTasksByState(sync::JobState::kQueued).size();
    summary.failed_tasks = tasks_.ListRetryableTasks().size();
    summary.backup_jobs = backup_.ListJobs().size();
    const auto restore_requests = recovery_.ListRestoreRequests();
    summary.restore_requests = restore_requests.size();
    for (const auto &request : restore_requests) {
        if (request.state == sync::RestoreRequestState::kRunning) {
            ++summary.running_restore_requests;
        } else if (request.state == sync::RestoreRequestState::kFailed) {
            ++summary.failed_restore_requests;
        }
    }
    const auto shared_entries = shared_library_.ListEntries();
    summary.shared_entries = shared_entries.size();
    summary.active_shared_entries = shared_library_.ListActiveEntries().size();
    for (const auto &entry : shared_entries) {
        for (const auto &recipient : entry.recipients) {
            if (recipient.state == sync::SharedRecipientState::kPending ||
                recipient.state == sync::SharedRecipientState::kFailed) {
                ++summary.pending_shared_deliveries;
            }
        }
    }
    summary.indexed_files = file_index_.ListIndexedFiles().size();
    summary.device_files = file_index_.ListDeviceFiles().size();
    summary.stored_objects = 0;
    for (const auto &file : file_index_.ListIndexedFiles()) {
        if (file_index_.FindStoredObject(file.file_id).has_value()) {
            ++summary.stored_objects;
        }
    }
    summary.backup_records = file_index_.ListBackupRecords().size();
    summary.audit_events = audit_.ListEvents().size();
    return summary;
}

std::vector<sync::NodeInfo> ControlPlaneService::ListDevices(bool online_only,
                                                             std::uint64_t now_epoch,
                                                             std::uint64_t offline_after_seconds) const {
    if (online_only) {
        return devices_.ListOnlineDevices(now_epoch, offline_after_seconds);
    }
    return devices_.ListDevices();
}

std::vector<sync::ScheduledTaskRecord> ControlPlaneService::ListTasks(std::optional<sync::JobState> state) const {
    if (state.has_value()) {
        return tasks_.ListTasksByState(*state);
    }
    return tasks_.ListTasks();
}

std::vector<sync::SharedLibraryEntry> ControlPlaneService::ListSharedEntries(const std::string &node_id,
                                                                             const std::string &scope) const {
    if (scope == "active") {
        std::vector<sync::SharedLibraryEntry> result;
        for (const auto &entry : shared_library_.ListEntriesForNode(node_id)) {
            if (!entry.expired) {
                result.push_back(entry);
            }
        }
        return result;
    }
    if (scope == "pending") {
        return shared_library_.ListPendingEntriesForNode(node_id);
    }
    if (scope == "all") {
        return shared_library_.ListEntriesForNode(node_id);
    }
    return {};
}

std::vector<sync::BackupJob> ControlPlaneService::ListBackupJobs(const std::optional<std::string> &node_id) const {
    if (node_id.has_value()) {
        return backup_.ListJobsForNode(*node_id);
    }
    return backup_.ListJobs();
}

std::vector<sync::RestoreRequest> ControlPlaneService::ListRestoreRequests() const {
    return recovery_.ListRestoreRequests();
}

std::vector<sync::AuditEvent> ControlPlaneService::ListAuditEvents(const std::optional<std::string> &category) const {
    if (category.has_value()) {
        return audit_.ListEventsByCategory(*category);
    }
    return audit_.ListEvents();
}

Status ControlPlaneService::RetryTask(const std::string &task_id, const std::string &detail) {
    return tasks_.RetryTask(task_id, detail);
}

Status ControlPlaneService::RetryRestore(const std::string &request_id) {
    return recovery_.RetryRestore(request_id);
}

Status ControlPlaneService::RetrySharedReceive(const std::string &entry_id,
                                               const std::string &target_node_id,
                                               const std::string &receive_root) {
    return shared_library_.RetryReceive(entry_id, target_node_id, receive_root);
}

}  // namespace netdisk
