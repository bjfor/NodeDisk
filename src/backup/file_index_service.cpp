#include "netdisk/backup/file_index_service.h"

#include <algorithm>

namespace netdisk {

FileIndexService::FileIndexService(const sync::MetadataStore &store) : store_(store) {}

std::vector<sync::FileRecord> FileIndexService::ListIndexedFiles() const {
    return store_.ListFiles();
}

std::vector<sync::BackupRecord> FileIndexService::ListBackupRecords() const {
    return store_.ListBackupRecords();
}

std::vector<sync::DeviceFileRecord> FileIndexService::ListDeviceFiles() const {
    return store_.ListDeviceFiles();
}

std::vector<sync::DeviceFileRecord> FileIndexService::ListDeviceFilesForNode(const std::string &node_id) const {
    std::vector<sync::DeviceFileRecord> result;
    for (const auto &record : store_.ListDeviceFiles()) {
        if (record.node_id == node_id) {
            result.push_back(record);
        }
    }
    return result;
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

std::vector<sync::BackupRecord> FileIndexService::ListBackupHistory(const std::string &node_id,
                                                                    const std::string &relative_path) const {
    std::vector<sync::BackupRecord> result;
    for (const auto &record : store_.ListBackupRecords()) {
        if (record.node_id == node_id && record.relative_path == relative_path) {
            result.push_back(record);
        }
    }
    std::sort(result.begin(), result.end(), [](const sync::BackupRecord &lhs, const sync::BackupRecord &rhs) {
        return lhs.backed_up_at_epoch > rhs.backed_up_at_epoch;
    });
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

std::optional<sync::FileRecord> FileIndexService::FindFile(const std::string &file_id) const {
    return store_.GetFile(file_id);
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

std::optional<sync::BackupRecord> FileIndexService::FindLatestBackup(const std::string &node_id,
                                                                     const std::string &relative_path) const {
    const auto history = ListBackupHistory(node_id, relative_path);
    if (history.empty()) {
        return std::nullopt;
    }
    return history.front();
}

}  // namespace netdisk
