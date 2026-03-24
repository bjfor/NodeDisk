#ifndef SYNC_DIFF_ENGINE_H
#define SYNC_DIFF_ENGINE_H

#include <vector>

#include "netdisk/metadata/metadata_store.h"
#include "netdisk/network/protocol.h"

namespace sync {

struct DiffResult {
    std::vector<ManifestEntry> identical_files;
    std::vector<ManifestEntry> missing_files;
    std::vector<MissingChunksRequest> chunk_requests;
};

class DiffEngine {
public:
    std::vector<ManifestFile> BuildManifest(const std::vector<FileRecord> &files) const;
    DiffResult Compare(const MetadataStore &local_store,
                       const std::vector<ManifestFile> &remote_manifest) const;
};

}  // namespace sync

#endif
