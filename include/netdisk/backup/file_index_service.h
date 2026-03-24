#ifndef NETDISK_BACKUP_FILE_INDEX_SERVICE_H
#define NETDISK_BACKUP_FILE_INDEX_SERVICE_H

#include <optional>
#include <vector>

#include "netdisk/core/types.h"
#include "netdisk/metadata/metadata_store.h"

namespace netdisk {

class FileIndexService {
public:
    explicit FileIndexService(const sync::MetadataStore &store);

    std::vector<sync::FileRecord> ListIndexedFiles() const;
    std::vector<sync::BackupRecord> ListBackupRecords() const;
    std::vector<sync::BackupRecord> ListBackupRecordsForNode(const std::string &node_id) const;
    std::vector<sync::FileRecord> ListFilesByPrefix(const std::string &prefix) const;
    std::optional<sync::FileRecord> FindByRelativePath(const std::string &relative_path) const;
    std::optional<sync::StoredObjectRecord> FindStoredObject(const std::string &file_id) const;

private:
    const sync::MetadataStore &store_;
};

}  // namespace netdisk

#endif
