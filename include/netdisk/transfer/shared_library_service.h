#ifndef NETDISK_TRANSFER_SHARED_LIBRARY_SERVICE_H
#define NETDISK_TRANSFER_SHARED_LIBRARY_SERVICE_H

#include <filesystem>
#include <optional>
#include <vector>

#include "netdisk/common/status.h"
#include "netdisk/control_plane/task_scheduler.h"
#include "netdisk/core/types.h"
#include "netdisk/metadata/metadata_store.h"
#include "netdisk/security/audit_logger.h"
#include "netdisk/storage/integrity_checker.h"

namespace netdisk {

class SharedLibraryService {
public:
    SharedLibraryService(TaskScheduler &scheduler,
                         AuditLogger &audit_logger,
                         sync::MetadataStore &metadata_store,
                         IntegrityChecker &integrity_checker);

    Status ConfigurePolicy(const sync::TransferPolicy &policy);
    Status PublishEntry(const sync::SharedLibraryEntry &entry);
    Status PublishFile(sync::SharedLibraryEntry entry, const std::filesystem::path &library_root);
    Status CreateReceiveTask(const std::string &entry_id, const std::string &target_node_id);
    Status ReceiveEntry(const std::string &entry_id,
                        const std::string &target_node_id,
                        const std::filesystem::path &target_root);
    Status RetryReceive(const std::string &entry_id,
                        const std::string &target_node_id,
                        const std::filesystem::path &target_root);
    Status MarkDelivered(const std::string &entry_id,
                         const std::string &target_node_id,
                         const std::string &received_path = "");
    std::vector<sync::SharedLibraryEntry> ListEntries() const;
    std::vector<sync::SharedLibraryEntry> ListActiveEntries() const;
    std::vector<sync::SharedLibraryEntry> ListEntriesForNode(const std::string &node_id) const;
    std::vector<sync::SharedLibraryEntry> ListPendingEntriesForNode(const std::string &node_id) const;
    std::optional<sync::SharedLibraryEntry> FindEntry(const std::string &entry_id) const;
    std::size_t CleanupExpired(std::uint64_t now_epoch);

private:
    TaskScheduler &scheduler_;
    AuditLogger &audit_logger_;
    sync::MetadataStore &metadata_store_;
    IntegrityChecker &integrity_checker_;
    sync::TransferPolicy policy_;
};

}  // namespace netdisk

#endif
