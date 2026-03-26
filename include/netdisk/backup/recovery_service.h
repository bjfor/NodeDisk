#ifndef NETDISK_BACKUP_RECOVERY_SERVICE_H
#define NETDISK_BACKUP_RECOVERY_SERVICE_H

#include <optional>
#include <vector>

#include "netdisk/common/status.h"
#include "netdisk/control_plane/task_scheduler.h"
#include "netdisk/core/types.h"
#include "netdisk/metadata/metadata_store.h"
#include "netdisk/security/audit_logger.h"
#include "netdisk/storage/storage_backend.h"

namespace netdisk {

class RecoveryService {
public:
    RecoveryService(TaskScheduler &scheduler, AuditLogger &audit_logger, sync::MetadataStore &store);

    Status SubmitRestore(const sync::RestoreRequest &request);
    Status ExecuteRestore(const std::string &request_id);
    Status ExecuteLatestRestore(const std::string &source_node_id,
                                const std::string &relative_path,
                                const std::string &target_node_id,
                                const std::string &destination_path);
    Status RetryRestore(const std::string &request_id);
    std::optional<sync::RestoreRequest> FindRestoreRequest(const std::string &request_id) const;
    std::vector<sync::RestoreRequest> ListRestoreRequests() const;

private:
    TaskScheduler &scheduler_;
    AuditLogger &audit_logger_;
    sync::MetadataStore &store_;
};

}  // namespace netdisk

#endif
