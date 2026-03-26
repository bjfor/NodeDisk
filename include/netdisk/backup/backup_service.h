#ifndef NETDISK_BACKUP_BACKUP_SERVICE_H
#define NETDISK_BACKUP_BACKUP_SERVICE_H

#include <optional>
#include <vector>

#include "netdisk/common/status.h"
#include "netdisk/control_plane/policy_manager.h"
#include "netdisk/control_plane/task_scheduler.h"
#include "netdisk/core/types.h"
#include "netdisk/metadata/metadata_store.h"
#include "netdisk/security/audit_logger.h"
#include "netdisk/storage/integrity_checker.h"
#include "netdisk/storage/storage_backend.h"

namespace netdisk {

class BackupService {
public:
    BackupService(TaskScheduler &scheduler,
                  PolicyManager &policies,
                  AuditLogger &audit_logger,
                  sync::MetadataStore &metadata_store,
                  IntegrityChecker &integrity_checker);

    Status ConfigurePolicy(const sync::BackupPolicy &policy);
    Status StartBackup(const sync::BackupJob &job);
    Status ExecuteBackup(const sync::BackupJob &job, sync::IStorageBackend &backend);
    std::optional<sync::BackupJob> FindJob(const std::string &job_id) const;
    std::vector<sync::BackupJob> ListJobs() const;
    std::vector<sync::BackupJob> ListJobsForNode(const std::string &node_id) const;
    std::vector<sync::BackupRecord> ListBackupHistory(const std::string &node_id,
                                                      const std::string &relative_path) const;
    std::optional<sync::BackupRecord> FindLatestBackup(const std::string &node_id,
                                                       const std::string &relative_path) const;

private:
    TaskScheduler &scheduler_;
    PolicyManager &policies_;
    AuditLogger &audit_logger_;
    sync::MetadataStore &metadata_store_;
    IntegrityChecker &integrity_checker_;
};

}  // namespace netdisk

#endif
