#include "netdisk/sync/diff_engine.h"

#include <unordered_map>

namespace sync {

namespace {

ManifestEntry ToManifestEntry(const FileRecord &file) {
    ManifestEntry entry;
    entry.file_id = file.file_id;
    entry.relative_path = file.relative_path;
    entry.root_hash = file.root_hash;
    entry.size = file.size;
    entry.modified_time_epoch = file.modified_time_epoch;
    entry.chunk_count = static_cast<std::uint32_t>(file.chunks.size());
    return entry;
}

ChunkManifestEntry ToChunkManifestEntry(const ChunkRecord &chunk) {
    ChunkManifestEntry entry;
    entry.chunk_index = chunk.chunk_index;
    entry.offset = chunk.offset;
    entry.size = chunk.size;
    entry.chunk_hash = chunk.chunk_hash;
    return entry;
}

std::unordered_map<std::uint32_t, ChunkRecord> BuildChunkIndex(const FileRecord &file) {
    std::unordered_map<std::uint32_t, ChunkRecord> chunk_index;
    for (const auto &chunk : file.chunks) {
        chunk_index.emplace(chunk.chunk_index, chunk);
    }
    return chunk_index;
}

}  // namespace

std::vector<ManifestFile> DiffEngine::BuildManifest(const std::vector<FileRecord> &files) const {
    std::vector<ManifestFile> manifest;
    manifest.reserve(files.size());

    for (const auto &file : files) {
        ManifestFile manifest_file;
        manifest_file.summary = ToManifestEntry(file);
        manifest_file.chunks.reserve(file.chunks.size());
        for (const auto &chunk : file.chunks) {
            manifest_file.chunks.push_back(ToChunkManifestEntry(chunk));
        }
        manifest.push_back(std::move(manifest_file));
    }

    return manifest;
}

DiffResult DiffEngine::Compare(const MetadataStore &local_store,
                               const std::vector<ManifestFile> &remote_manifest) const {
    DiffResult result;

    std::unordered_map<std::string, FileRecord> local_by_relative_path;
    for (const auto &file : local_store.ListFiles()) {
        local_by_relative_path.emplace(file.relative_path, file);
    }

    for (const auto &remote_file : remote_manifest) {
        const auto local_it = local_by_relative_path.find(remote_file.summary.relative_path);
        if (local_it == local_by_relative_path.end()) {
            MissingChunksRequest request;
            request.file_id = remote_file.summary.file_id;
            request.relative_path = remote_file.summary.relative_path;
            request.expected_root_hash = remote_file.summary.root_hash;
            request.missing_chunks = remote_file.chunks;

            result.missing_files.push_back(remote_file.summary);
            result.chunk_requests.push_back(std::move(request));
            continue;
        }

        const FileRecord &local_file = local_it->second;
        if (local_file.root_hash == remote_file.summary.root_hash) {
            result.identical_files.push_back(remote_file.summary);
            continue;
        }

        const auto local_chunks = BuildChunkIndex(local_file);
        MissingChunksRequest request;
        request.file_id = remote_file.summary.file_id;
        request.relative_path = remote_file.summary.relative_path;
        request.expected_root_hash = remote_file.summary.root_hash;

        for (const auto &remote_chunk : remote_file.chunks) {
            const auto chunk_it = local_chunks.find(remote_chunk.chunk_index);
            if (chunk_it == local_chunks.end() ||
                chunk_it->second.chunk_hash != remote_chunk.chunk_hash ||
                chunk_it->second.size != remote_chunk.size) {
                request.missing_chunks.push_back(remote_chunk);
            }
        }

        if (!request.missing_chunks.empty()) {
            result.chunk_requests.push_back(std::move(request));
        } else {
            // If the root hash differs but chunk-by-index comparison does not report
            // missing chunks, request the full remote file to keep recovery deterministic.
            MissingChunksRequest full_request;
            full_request.file_id = remote_file.summary.file_id;
            full_request.relative_path = remote_file.summary.relative_path;
            full_request.expected_root_hash = remote_file.summary.root_hash;
            full_request.missing_chunks = remote_file.chunks;
            result.chunk_requests.push_back(std::move(full_request));
        }
    }

    return result;
}

}  // namespace sync
