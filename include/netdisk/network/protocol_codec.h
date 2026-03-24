#ifndef SYNC_PROTOCOL_CODEC_H
#define SYNC_PROTOCOL_CODEC_H

#include <string>
#include <vector>

#include "netdisk/network/protocol.h"

namespace sync {

struct MessageFrame {
    MessageType type = MessageType::kError;
    std::string payload;
};

class ProtocolCodec {
public:
    static MessageFrame EncodeHello(const HelloMessage &message);
    static MessageFrame EncodeManifestSummary(const std::vector<ManifestFile> &manifest);
    static MessageFrame EncodeMissingChunksRequest(const MissingChunksRequest &request);
    static MessageFrame EncodeChunkHeader(const ChunkPayloadHeader &header);
    static MessageFrame EncodeChunkData(const ChunkDataMessage &message);
    static MessageFrame EncodeSyncAck(const SyncAckMessage &message);
    static MessageFrame EncodeError(const ErrorMessage &message);

    static HelloMessage DecodeHello(const MessageFrame &frame);
    static std::vector<ManifestFile> DecodeManifestSummary(const MessageFrame &frame);
    static MissingChunksRequest DecodeMissingChunksRequest(const MessageFrame &frame);
    static ChunkPayloadHeader DecodeChunkHeader(const MessageFrame &frame);
    static ChunkDataMessage DecodeChunkData(const MessageFrame &frame);
    static SyncAckMessage DecodeSyncAck(const MessageFrame &frame);
    static ErrorMessage DecodeError(const MessageFrame &frame);
};

}  // namespace sync

#endif
