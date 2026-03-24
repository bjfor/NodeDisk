#ifndef SYNC_CHUNK_READER_H
#define SYNC_CHUNK_READER_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include "netdisk/sync/hash.h"
#include "netdisk/network/protocol.h"

namespace sync {

class ChunkReader {
public:
    explicit ChunkReader(std::unique_ptr<IHasher> hasher);
    std::vector<std::uint8_t> ReadAndVerify(const std::filesystem::path &file_path,
                                            const ChunkManifestEntry &chunk) const;

private:
    std::unique_ptr<IHasher> hasher_;
};

}  // namespace sync

#endif
