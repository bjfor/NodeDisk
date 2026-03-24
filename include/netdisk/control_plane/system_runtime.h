#ifndef NETDISK_CONTROL_PLANE_SYSTEM_RUNTIME_H
#define NETDISK_CONTROL_PLANE_SYSTEM_RUNTIME_H

#include "netdisk/backup/backup_service.h"
#include "netdisk/backup/file_index_service.h"
#include "netdisk/backup/recovery_service.h"
#include "netdisk/control_plane/device_manager.h"
#include "netdisk/control_plane/policy_manager.h"
#include "netdisk/control_plane/task_scheduler.h"
#include "netdisk/metadata/metadata_store.h"
#include "netdisk/security/audit_logger.h"
#include "netdisk/security/auth_service.h"
#include "netdisk/security/node_network_service.h"
#include "netdisk/storage/integrity_checker.h"
#include "netdisk/storage/replica_manager.h"
#include "netdisk/transfer/shared_library_service.h"

namespace netdisk {

class SystemRuntime {
public:
    explicit SystemRuntime(sync::MetadataStore &metadata_store);

    DeviceManager &devices();
    PolicyManager &policies();
    TaskScheduler &tasks();
    AuditLogger &audit();
    AuthService &auth();
    NodeNetworkService &network();
    ReplicaManager &replicas();
    IntegrityChecker &integrity();
    BackupService &backup();
    RecoveryService &recovery();
    SharedLibraryService &shared_library();
    FileIndexService &file_index();

private:
    sync::MetadataStore &metadata_store_;
    DeviceManager device_manager_;
    PolicyManager policy_manager_;
    TaskScheduler task_scheduler_;
    AuditLogger audit_logger_;
    AuthService auth_service_;
    NodeNetworkService node_network_service_;
    ReplicaManager replica_manager_;
    IntegrityChecker integrity_checker_;
    BackupService backup_service_;
    RecoveryService recovery_service_;
    SharedLibraryService shared_library_service_;
    FileIndexService file_index_service_;
};

}  // namespace netdisk

#endif
