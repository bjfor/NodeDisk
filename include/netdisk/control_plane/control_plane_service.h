#ifndef NETDISK_CONTROL_PLANE_CONTROL_PLANE_SERVICE_H
#define NETDISK_CONTROL_PLANE_CONTROL_PLANE_SERVICE_H

#include <cstdint>
#include <optional>
#include <vector>

#include "netdisk/backup/backup_service.h"
#include "netdisk/backup/file_index_service.h"
#include "netdisk/backup/recovery_service.h"
#include "netdisk/control_plane/device_manager.h"
#include "netdisk/control_plane/policy_manager.h"
#include "netdisk/control_plane/task_scheduler.h"
#include "netdisk/core/types.h"
#include "netdisk/security/audit_logger.h"
#include "netdisk/security/node_network_service.h"
#include "netdisk/transfer/shared_library_service.h"

namespace netdisk {

struct ControlPlaneSummary {
    std::size_t devices = 0;
    std::size_t online_devices = 0;
    std::size_t peers = 0;
    std::size_t online_peers = 0;
    std::size_t backup_policies = 0;
    std::size_t transfer_policies = 0;
    std::size_t scheduled_tasks = 0;
    std::size_t queued_tasks = 0;
    std::size_t failed_tasks = 0;
    std::size_t backup_jobs = 0;
    std::size_t restore_requests = 0;
    std::size_t running_restore_requests = 0;
    std::size_t failed_restore_requests = 0;
    std::size_t shared_entries = 0;
    std::size_t active_shared_entries = 0;
    std::size_t pending_shared_deliveries = 0;
    std::size_t indexed_files = 0;
    std::size_t device_files = 0;
    std::size_t stored_objects = 0;
    std::size_t backup_records = 0;
    std::size_t audit_events = 0;
};

class ControlPlaneService {
public:
    ControlPlaneService(DeviceManager &devices,
                        PolicyManager &policies,
                        TaskScheduler &tasks,
                        NodeNetworkService &network,
                        AuditLogger &audit,
                        BackupService &backup,
                        RecoveryService &recovery,
                        SharedLibraryService &shared_library,
                        FileIndexService &file_index);

    void RefreshNetworkView(std::uint64_t now_epoch, std::uint64_t offline_after_seconds);
    ControlPlaneSummary BuildSummary(std::uint64_t now_epoch, std::uint64_t offline_after_seconds);

    std::vector<sync::NodeInfo> ListDevices(bool online_only,
                                            std::uint64_t now_epoch,
                                            std::uint64_t offline_after_seconds) const;
    std::vector<sync::ScheduledTaskRecord> ListTasks(std::optional<sync::JobState> state = std::nullopt) const;
    std::vector<sync::SharedLibraryEntry> ListSharedEntries(const std::string &node_id,
                                                            const std::string &scope) const;
    std::vector<sync::BackupJob> ListBackupJobs(const std::optional<std::string> &node_id = std::nullopt) const;
    std::vector<sync::RestoreRequest> ListRestoreRequests() const;
    std::vector<sync::AuditEvent> ListAuditEvents(const std::optional<std::string> &category = std::nullopt) const;
    Status RetryTask(const std::string &task_id, const std::string &detail);
    Status RetryRestore(const std::string &request_id);
    Status RetrySharedReceive(const std::string &entry_id,
                              const std::string &target_node_id,
                              const std::string &receive_root);

private:
    DeviceManager &devices_;
    PolicyManager &policies_;
    TaskScheduler &tasks_;
    NodeNetworkService &network_;
    AuditLogger &audit_;
    BackupService &backup_;
    RecoveryService &recovery_;
    SharedLibraryService &shared_library_;
    FileIndexService &file_index_;
};

}  // namespace netdisk

#endif
