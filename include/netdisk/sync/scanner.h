#ifndef SYNC_SCANNER_H
#define SYNC_SCANNER_H

#include <filesystem>
#include <vector>

#include "netdisk/sync/chunker.h"
#include "netdisk/metadata/metadata_store.h"

namespace sync {

class Scanner {
public:
    Scanner(const Chunker &chunker, MetadataStore &store);
    std::vector<FileRecord> ScanDirectory(const std::filesystem::path &root_path,
                                         const std::vector<std::filesystem::path> &excluded_paths = {});

private:
    const Chunker &chunker_;
    MetadataStore &store_;
};

}  // namespace sync

#endif
