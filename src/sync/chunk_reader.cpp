#include "netdisk/sync/chunk_reader.h"

#include <fstream>
#include <stdexcept>

namespace sync {

ChunkReader::ChunkReader(std::unique_ptr<IHasher> hasher) : hasher_(std::move(hasher)) {}

std::vector<std::uint8_t> ChunkReader::ReadAndVerify(const std::filesystem::path &file_path,
                                                     const ChunkManifestEntry &chunk) const {
    std::ifstream input(file_path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open file for chunk read: " + file_path.string());
    }

    input.seekg(static_cast<std::streamoff>(chunk.offset), std::ios::beg);
    if (!input.good()) {
        throw std::runtime_error("failed to seek file for chunk read: " + file_path.string());
    }

    std::vector<std::uint8_t> bytes(chunk.size);
    input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(chunk.size));
    if (static_cast<std::uint64_t>(input.gcount()) != chunk.size) {
        throw std::runtime_error("short read while loading chunk from: " + file_path.string());
    }

    std::unique_ptr<IHasher> hasher = hasher_->Clone();
    hasher->Reset();
    if (!bytes.empty()) {
        hasher->Update(bytes.data(), bytes.size());
    }
    if (hasher->FinalHex() != chunk.chunk_hash) {
        throw std::runtime_error("chunk hash mismatch while reading: " + file_path.string());
    }

    return bytes;
}

}  // namespace sync
