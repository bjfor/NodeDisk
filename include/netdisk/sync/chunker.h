#ifndef SYNC_CHUNKER_H
#define SYNC_CHUNKER_H

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

#include "netdisk/sync/hash.h"
#include "netdisk/core/types.h"

namespace sync {

struct ChunkerOptions {
    std::size_t chunk_size = 4 * 1024 * 1024;
};

class Chunker {
public:
    Chunker(ChunkerOptions options, std::unique_ptr<IHasher> hasher);
    FileRecord BuildFileRecord(const std::filesystem::path &file_path,
                               const std::filesystem::path &root_path) const;

private:
    ChunkerOptions options_;
    mutable std::unique_ptr<IHasher> hasher_;

    std::string HashBuffer(const std::uint8_t *data, std::size_t size) const;
};

}  // namespace sync

#endif
