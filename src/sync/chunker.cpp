#include "netdisk/sync/chunker.h"

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace sync {

namespace {

std::uint64_t ToUnixSeconds(const std::filesystem::file_time_type &time_point) {
    const auto adjusted = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        time_point - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    return static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(adjusted));
}

}  // namespace

Chunker::Chunker(ChunkerOptions options, std::unique_ptr<IHasher> hasher)
    : options_(options), hasher_(std::move(hasher)) {}

std::string Chunker::HashBuffer(const std::uint8_t *data, std::size_t size) const {
    hasher_->Reset();
    hasher_->Update(data, size);
    return hasher_->FinalHex();
}

FileRecord Chunker::BuildFileRecord(const std::filesystem::path &file_path,
                                    const std::filesystem::path &root_path) const {
    FileRecord file;
    file.absolute_path = file_path.lexically_normal().string();
    file.relative_path = std::filesystem::relative(file_path, root_path).lexically_normal().string();
    file.size = std::filesystem::file_size(file_path);
    file.modified_time_epoch = ToUnixSeconds(std::filesystem::last_write_time(file_path));
    file.mode = 0;

    std::ifstream input(file_path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open file for chunking: " + file.absolute_path);
    }

    std::vector<std::uint8_t> buffer(options_.chunk_size);
    std::unique_ptr<IHasher> file_hasher = hasher_->Clone();
    file_hasher->Reset();
    std::uint64_t offset = 0;
    std::uint32_t index = 0;

    while (input) {
        input.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const auto read_count = static_cast<std::size_t>(input.gcount());
        if (read_count == 0) {
            break;
        }

        ChunkRecord chunk;
        chunk.offset = offset;
        chunk.size = read_count;
        chunk.chunk_index = index++;
        chunk.chunk_hash = HashBuffer(buffer.data(), read_count);
        file.chunks.push_back(chunk);
        file_hasher->Update(buffer.data(), read_count);
        offset += read_count;
    }

    file.root_hash = file_hasher->FinalHex();
    file.file_id = file.root_hash;
    for (auto &chunk : file.chunks) {
        chunk.file_id = file.file_id;
    }
    file.status = FileStatus::kReady;

    return file;
}

}  // namespace sync
