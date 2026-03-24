#include "netdisk/network/protocol_codec.h"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <stdexcept>

namespace sync {

namespace {

void AppendString(std::string *buffer, const std::string &value) {
    *buffer += std::to_string(value.size());
    *buffer += '\n';
    *buffer += value;
    *buffer += '\n';
}

std::string ConsumeString(const std::string &buffer, std::size_t *cursor) {
    const std::size_t length_end = buffer.find('\n', *cursor);
    if (length_end == std::string::npos) {
        throw std::runtime_error("protocol decode error: missing string length delimiter");
    }

    const std::size_t length = static_cast<std::size_t>(
        std::stoull(buffer.substr(*cursor, length_end - *cursor)));
    const std::size_t data_begin = length_end + 1;
    if (data_begin + length > buffer.size()) {
        throw std::runtime_error("protocol decode error: truncated string payload");
    }

    std::string value = buffer.substr(data_begin, length);
    const std::size_t data_end = data_begin + length;
    if (data_end >= buffer.size() || buffer[data_end] != '\n') {
        throw std::runtime_error("protocol decode error: missing string terminator");
    }

    *cursor = data_end + 1;
    return value;
}

void AppendU64(std::string *buffer, std::uint64_t value) {
    *buffer += std::to_string(value);
    *buffer += '\n';
}

std::uint64_t ConsumeU64(const std::string &buffer, std::size_t *cursor) {
    const std::size_t end = buffer.find('\n', *cursor);
    if (end == std::string::npos) {
        throw std::runtime_error("protocol decode error: missing integer delimiter");
    }

    const std::uint64_t value = std::stoull(buffer.substr(*cursor, end - *cursor));
    *cursor = end + 1;
    return value;
}

void AppendBool(std::string *buffer, bool value) {
    *buffer += value ? "1\n" : "0\n";
}

bool ConsumeBool(const std::string &buffer, std::size_t *cursor) {
    return ConsumeU64(buffer, cursor) != 0;
}

void RequireType(const MessageFrame &frame, MessageType expected) {
    if (frame.type != expected) {
        throw std::runtime_error("protocol decode error: unexpected message type");
    }
}

}  // namespace

MessageFrame ProtocolCodec::EncodeHello(const HelloMessage &message) {
    MessageFrame frame;
    frame.type = MessageType::kHello;
    AppendString(&frame.payload, message.node_id);
    AppendString(&frame.payload, message.device_name);
    AppendString(&frame.payload, message.protocol_version);
    return frame;
}

MessageFrame ProtocolCodec::EncodeManifestSummary(const std::vector<ManifestFile> &manifest) {
    MessageFrame frame;
    frame.type = MessageType::kManifestSummary;
    AppendU64(&frame.payload, manifest.size());
    for (const auto &file : manifest) {
        AppendString(&frame.payload, file.summary.file_id);
        AppendString(&frame.payload, file.summary.relative_path);
        AppendString(&frame.payload, file.summary.root_hash);
        AppendU64(&frame.payload, file.summary.size);
        AppendU64(&frame.payload, file.summary.modified_time_epoch);
        AppendU64(&frame.payload, file.chunks.size());
        for (const auto &chunk : file.chunks) {
            AppendU64(&frame.payload, chunk.chunk_index);
            AppendU64(&frame.payload, chunk.offset);
            AppendU64(&frame.payload, chunk.size);
            AppendString(&frame.payload, chunk.chunk_hash);
        }
    }
    return frame;
}

MessageFrame ProtocolCodec::EncodeMissingChunksRequest(const MissingChunksRequest &request) {
    MessageFrame frame;
    frame.type = MessageType::kMissingChunksRequest;
    AppendString(&frame.payload, request.file_id);
    AppendString(&frame.payload, request.relative_path);
    AppendString(&frame.payload, request.expected_root_hash);
    AppendU64(&frame.payload, request.missing_chunks.size());
    for (const auto &chunk : request.missing_chunks) {
        AppendU64(&frame.payload, chunk.chunk_index);
        AppendU64(&frame.payload, chunk.offset);
        AppendU64(&frame.payload, chunk.size);
        AppendString(&frame.payload, chunk.chunk_hash);
    }
    return frame;
}

MessageFrame ProtocolCodec::EncodeChunkHeader(const ChunkPayloadHeader &header) {
    MessageFrame frame;
    frame.type = MessageType::kChunkData;
    AppendString(&frame.payload, header.file_id);
    AppendString(&frame.payload, header.chunk_hash);
    AppendU64(&frame.payload, header.chunk_index);
    AppendU64(&frame.payload, header.offset);
    AppendU64(&frame.payload, header.payload_size);
    return frame;
}

MessageFrame ProtocolCodec::EncodeChunkData(const ChunkDataMessage &message) {
    MessageFrame frame = EncodeChunkHeader(message.header);
    AppendString(&frame.payload, message.payload_bytes);
    return frame;
}

MessageFrame ProtocolCodec::EncodeSyncAck(const SyncAckMessage &message) {
    MessageFrame frame;
    frame.type = MessageType::kSyncAck;
    AppendString(&frame.payload, message.file_id);
    AppendString(&frame.payload, message.relative_path);
    AppendBool(&frame.payload, message.success);
    AppendString(&frame.payload, message.detail);
    return frame;
}

MessageFrame ProtocolCodec::EncodeError(const ErrorMessage &message) {
    MessageFrame frame;
    frame.type = MessageType::kError;
    AppendString(&frame.payload, message.detail);
    return frame;
}

HelloMessage ProtocolCodec::DecodeHello(const MessageFrame &frame) {
    RequireType(frame, MessageType::kHello);
    std::size_t cursor = 0;
    HelloMessage message;
    message.node_id = ConsumeString(frame.payload, &cursor);
    message.device_name = ConsumeString(frame.payload, &cursor);
    message.protocol_version = ConsumeString(frame.payload, &cursor);
    return message;
}

std::vector<ManifestFile> ProtocolCodec::DecodeManifestSummary(const MessageFrame &frame) {
    RequireType(frame, MessageType::kManifestSummary);
    std::size_t cursor = 0;
    const std::size_t file_count = static_cast<std::size_t>(ConsumeU64(frame.payload, &cursor));

    std::vector<ManifestFile> manifest;
    manifest.reserve(file_count);
    for (std::size_t i = 0; i < file_count; ++i) {
        ManifestFile file;
        file.summary.file_id = ConsumeString(frame.payload, &cursor);
        file.summary.relative_path = ConsumeString(frame.payload, &cursor);
        file.summary.root_hash = ConsumeString(frame.payload, &cursor);
        file.summary.size = ConsumeU64(frame.payload, &cursor);
        file.summary.modified_time_epoch = ConsumeU64(frame.payload, &cursor);
        file.summary.chunk_count = static_cast<std::uint32_t>(ConsumeU64(frame.payload, &cursor));
        file.chunks.reserve(file.summary.chunk_count);
        for (std::uint32_t chunk_idx = 0; chunk_idx < file.summary.chunk_count; ++chunk_idx) {
            ChunkManifestEntry chunk;
            chunk.chunk_index = static_cast<std::uint32_t>(ConsumeU64(frame.payload, &cursor));
            chunk.offset = ConsumeU64(frame.payload, &cursor);
            chunk.size = ConsumeU64(frame.payload, &cursor);
            chunk.chunk_hash = ConsumeString(frame.payload, &cursor);
            file.chunks.push_back(std::move(chunk));
        }
        manifest.push_back(std::move(file));
    }
    return manifest;
}

MissingChunksRequest ProtocolCodec::DecodeMissingChunksRequest(const MessageFrame &frame) {
    RequireType(frame, MessageType::kMissingChunksRequest);
    std::size_t cursor = 0;
    MissingChunksRequest request;
    request.file_id = ConsumeString(frame.payload, &cursor);
    request.relative_path = ConsumeString(frame.payload, &cursor);
    request.expected_root_hash = ConsumeString(frame.payload, &cursor);
    const std::size_t chunk_count = static_cast<std::size_t>(ConsumeU64(frame.payload, &cursor));
    request.missing_chunks.reserve(chunk_count);
    for (std::size_t i = 0; i < chunk_count; ++i) {
        ChunkManifestEntry chunk;
        chunk.chunk_index = static_cast<std::uint32_t>(ConsumeU64(frame.payload, &cursor));
        chunk.offset = ConsumeU64(frame.payload, &cursor);
        chunk.size = ConsumeU64(frame.payload, &cursor);
        chunk.chunk_hash = ConsumeString(frame.payload, &cursor);
        request.missing_chunks.push_back(std::move(chunk));
    }
    return request;
}

ChunkPayloadHeader ProtocolCodec::DecodeChunkHeader(const MessageFrame &frame) {
    RequireType(frame, MessageType::kChunkData);
    std::size_t cursor = 0;
    ChunkPayloadHeader header;
    header.file_id = ConsumeString(frame.payload, &cursor);
    header.chunk_hash = ConsumeString(frame.payload, &cursor);
    header.chunk_index = static_cast<std::uint32_t>(ConsumeU64(frame.payload, &cursor));
    header.offset = ConsumeU64(frame.payload, &cursor);
    header.payload_size = ConsumeU64(frame.payload, &cursor);
    return header;
}

ChunkDataMessage ProtocolCodec::DecodeChunkData(const MessageFrame &frame) {
    RequireType(frame, MessageType::kChunkData);
    std::size_t cursor = 0;
    ChunkDataMessage message;
    message.header.file_id = ConsumeString(frame.payload, &cursor);
    message.header.chunk_hash = ConsumeString(frame.payload, &cursor);
    message.header.chunk_index = static_cast<std::uint32_t>(ConsumeU64(frame.payload, &cursor));
    message.header.offset = ConsumeU64(frame.payload, &cursor);
    message.header.payload_size = ConsumeU64(frame.payload, &cursor);
    message.payload_bytes = ConsumeString(frame.payload, &cursor);
    if (message.payload_bytes.size() != message.header.payload_size) {
        throw std::runtime_error("protocol decode error: chunk payload size mismatch");
    }
    return message;
}

SyncAckMessage ProtocolCodec::DecodeSyncAck(const MessageFrame &frame) {
    RequireType(frame, MessageType::kSyncAck);
    std::size_t cursor = 0;
    SyncAckMessage message;
    message.file_id = ConsumeString(frame.payload, &cursor);
    message.relative_path = ConsumeString(frame.payload, &cursor);
    message.success = ConsumeBool(frame.payload, &cursor);
    message.detail = ConsumeString(frame.payload, &cursor);
    return message;
}

ErrorMessage ProtocolCodec::DecodeError(const MessageFrame &frame) {
    RequireType(frame, MessageType::kError);
    std::size_t cursor = 0;
    ErrorMessage message;
    message.detail = ConsumeString(frame.payload, &cursor);
    return message;
}

}  // namespace sync
