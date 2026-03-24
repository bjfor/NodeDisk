#ifndef SYNC_STORAGE_BACKEND_H
#define SYNC_STORAGE_BACKEND_H

#include <filesystem>
#include <memory>
#include <string>

#include "netdisk/core/types.h"

namespace sync {

class IStorageBackend {
public:
    virtual ~IStorageBackend() = default;

    virtual std::string Name() const = 0;
    virtual StoredObjectRecord StoreFile(const FileRecord &file,
                                         const std::filesystem::path &local_path) = 0;
};

std::unique_ptr<IStorageBackend> CreateStorageBackend(const std::string &backend_name,
                                                      const std::string &backend_arg);

}  // namespace sync

#endif
