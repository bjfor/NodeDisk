#include "netdisk/backup/file_index_service.h"

namespace netdisk {

FileIndexService::FileIndexService(const sync::MetadataStore &store) : store_(store) {}

std::vector<sync::FileRecord> FileIndexService::ListIndexedFiles() const {
    return store_.ListFiles();
}

std::vector<sync::BackupRecord> FileIndexService::ListBackupRecords() const {
    return store_.ListBackupRecords();
}

std::vector<sync::BackupRecord> FileIndexService::ListBackupRecordsForNode(const std::string &node_id) const {
    std::vector<sync::BackupRecord> result;
    for (const auto &record : store_.ListBackupRecords()) {
        if (record.node_id == node_id) {
            result.push_back(record);
        }
    }
    return result;
}

std::vector<sync::FileRecord> FileIndexService::ListFilesByPrefix(const std::string &prefix) const {
    std::vector<sync::FileRecord> result;
    for (const auto &file : store_.ListFiles()) {
        if (file.relative_path.rfind(prefix, 0) == 0) {
            result.push_back(file);
        }
    }
    return result;
}

std::optional<sync::FileRecord> FileIndexService::FindByRelativePath(const std::string &relative_path) const {
    for (const auto &file : store_.ListFiles()) {
        if (file.relative_path == relative_path) {
            return file;
        }
    }
    return std::nullopt;
}

std::optional<sync::StoredObjectRecord> FileIndexService::FindStoredObject(const std::string &file_id) const {
    return store_.GetStoredObject(file_id);
}

}  // namespace netdisk
