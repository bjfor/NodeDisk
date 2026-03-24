#ifndef NETDISK_BACKUP_RECOVERY_SERVICE_H
#define NETDISK_BACKUP_RECOVERY_SERVICE_H

#include <optional>
#include <vector>

#include "netdisk/common/status.h"
#include "netdisk/control_plane/task_scheduler.h"
#include "netdisk/core/types.h"
#include "netdisk/security/audit_logger.h"

namespace netdisk {

class RecoveryService {
public:
    RecoveryService(TaskScheduler &scheduler, AuditLogger &audit_logger);

    Status SubmitRestore(const sync::RestoreRequest &request);
    std::optional<sync::RestoreRequest> FindRestoreRequest(const std::string &request_id) const;
    std::vector<sync::RestoreRequest> ListRestoreRequests() const;

private:
    TaskScheduler &scheduler_;
    AuditLogger &audit_logger_;
    std::vector<sync::RestoreRequest> requests_;
};

}  // namespace netdisk

#endif
