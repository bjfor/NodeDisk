#ifndef NETDISK_STORAGE_INTEGRITY_CHECKER_H
#define NETDISK_STORAGE_INTEGRITY_CHECKER_H

#include <filesystem>

#include "netdisk/common/status.h"
#include "netdisk/core/types.h"

namespace netdisk {

class IntegrityChecker {
public:
    Status VerifyFile(const sync::FileRecord &file, const std::filesystem::path &path) const;
};

}  // namespace netdisk

#endif
