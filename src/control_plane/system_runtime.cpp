#include "netdisk/control_plane/system_runtime.h"

namespace netdisk {

SystemRuntime::SystemRuntime(sync::MetadataStore &metadata_store)
    : metadata_store_(metadata_store),
      device_manager_(metadata_store),
      policy_manager_(metadata_store),
      task_scheduler_(metadata_store),
      audit_logger_(metadata_store),
      backup_service_(task_scheduler_, policy_manager_, audit_logger_, metadata_store_, integrity_checker_),
      recovery_service_(task_scheduler_, audit_logger_, metadata_store_),
      shared_library_service_(task_scheduler_, audit_logger_, metadata_store_, integrity_checker_),
      file_index_service_(metadata_store_),
      control_plane_service_(device_manager_,
                             policy_manager_,
                             task_scheduler_,
                             node_network_service_,
                             audit_logger_,
                             backup_service_,
                             recovery_service_,
                             shared_library_service_,
                             file_index_service_) {}

sync::MetadataStore &SystemRuntime::metadata() {
    return metadata_store_;
}

DeviceManager &SystemRuntime::devices() {
    return device_manager_;
}

PolicyManager &SystemRuntime::policies() {
    return policy_manager_;
}

TaskScheduler &SystemRuntime::tasks() {
    return task_scheduler_;
}

AuditLogger &SystemRuntime::audit() {
    return audit_logger_;
}

AuthService &SystemRuntime::auth() {
    return auth_service_;
}

NodeNetworkService &SystemRuntime::network() {
    return node_network_service_;
}

ReplicaManager &SystemRuntime::replicas() {
    return replica_manager_;
}

IntegrityChecker &SystemRuntime::integrity() {
    return integrity_checker_;
}

BackupService &SystemRuntime::backup() {
    return backup_service_;
}

RecoveryService &SystemRuntime::recovery() {
    return recovery_service_;
}

SharedLibraryService &SystemRuntime::shared_library() {
    return shared_library_service_;
}

FileIndexService &SystemRuntime::file_index() {
    return file_index_service_;
}

ControlPlaneService &SystemRuntime::control() {
    return control_plane_service_;
}

}  // namespace netdisk
