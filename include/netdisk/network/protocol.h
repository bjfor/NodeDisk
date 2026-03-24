#ifndef SYNC_PROTOCOL_H
#define SYNC_PROTOCOL_H

#include <cstdint>
#include <string>
#include <vector>

namespace sync {

enum class MessageType : std::uint8_t {
    kHello = 0,
    kManifestSummary = 1,
    kMissingChunksRequest = 2,
    kChunkData = 3,
    kSyncAck = 4,
    kError = 5,
};

struct HelloMessage {
    std::string node_id;
    std::string device_name;
    std::string protocol_version;
};

struct ManifestEntry {
    std::string file_id;
    std::string relative_path;
    std::string root_hash;
    std::uint64_t size = 0;
    std::uint64_t modified_time_epoch = 0;
    std::uint32_t chunk_count = 0;
};

struct ChunkManifestEntry {
    std::uint32_t chunk_index = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::string chunk_hash;
};

struct ManifestFile {
    ManifestEntry summary;
    std::vector<ChunkManifestEntry> chunks;
};

struct MissingChunksRequest {
    std::string file_id;
    std::string relative_path;
    std::string expected_root_hash;
    std::vector<ChunkManifestEntry> missing_chunks;
};

struct ChunkPayloadHeader {
    std::string file_id;
    std::string chunk_hash;
    std::uint32_t chunk_index = 0;
    std::uint64_t offset = 0;
    std::uint64_t payload_size = 0;
};

struct ChunkDataMessage {
    ChunkPayloadHeader header;
    std::string payload_bytes;
};

struct SyncAckMessage {
    std::string file_id;
    std::string relative_path;
    bool success = false;
    std::string detail;
};

struct ErrorMessage {
    std::string detail;
};

}  // namespace sync

#endif
