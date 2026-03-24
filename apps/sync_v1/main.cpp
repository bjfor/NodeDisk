#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "netdisk/sync/chunk_reader.h"
#include "netdisk/sync/chunker.h"
#include "netdisk/sync/diff_engine.h"
#include "netdisk/sync/hash.h"
#include "netdisk/metadata/metadata_store.h"
#include "netdisk/control_plane/system_runtime.h"
#include "netdisk/network/protocol_codec.h"
#include "netdisk/sync/scanner.h"
#include "netdisk/storage/storage_backend.h"
#include "netdisk/sync/sync_writer.h"
#include "netdisk/network/transport.h"

namespace {

std::unordered_map<std::uint32_t, sync::ChunkRecord> BuildLocalChunkMap(const std::optional<sync::FileRecord> &file) {
    std::unordered_map<std::uint32_t, sync::ChunkRecord> chunk_map;
    if (!file.has_value()) {
        return chunk_map;
    }
    for (const auto &chunk : file->chunks) {
        chunk_map.emplace(chunk.chunk_index, chunk);
    }
    return chunk_map;
}

std::string MakeTaskId(const sync::MissingChunksRequest &request) {
    return request.file_id + "|" + request.relative_path;
}

sync::SyncTaskChunk ToTaskChunk(const std::string &task_id,
                                const std::string &file_id,
                                const sync::ChunkManifestEntry &chunk) {
    sync::SyncTaskChunk task_chunk;
    task_chunk.task_id = task_id;
    task_chunk.file_id = file_id;
    task_chunk.chunk_index = chunk.chunk_index;
    task_chunk.offset = chunk.offset;
    task_chunk.size = chunk.size;
    task_chunk.chunk_hash = chunk.chunk_hash;
    return task_chunk;
}

std::uint32_t ComputeResumePrefixCount(const std::vector<sync::SyncTaskChunk> &completed_chunks,
                                       const std::vector<sync::ChunkManifestEntry> &remote_chunks) {
    std::unordered_map<std::uint32_t, sync::SyncTaskChunk> by_index;
    for (const auto &chunk : completed_chunks) {
        by_index.emplace(chunk.chunk_index, chunk);
    }

    std::uint32_t prefix = 0;
    for (const auto &remote_chunk : remote_chunks) {
        const auto it = by_index.find(remote_chunk.chunk_index);
        if (it == by_index.end()) {
            break;
        }
        if (it->second.chunk_hash != remote_chunk.chunk_hash ||
            it->second.size != remote_chunk.size ||
            it->second.offset != remote_chunk.offset) {
            break;
        }
        ++prefix;
    }
    return prefix;
}

std::uint64_t ComputePrefixBytes(const std::vector<sync::ChunkManifestEntry> &remote_chunks,
                                 std::uint32_t prefix_count) {
    std::uint64_t total = 0;
    for (const auto &chunk : remote_chunks) {
        if (chunk.chunk_index >= prefix_count) {
            break;
        }
        total += chunk.size;
    }
    return total;
}

sync::MissingChunksRequest TrimCompletedChunks(const sync::MissingChunksRequest &request,
                                               std::uint32_t resume_prefix_count) {
    sync::MissingChunksRequest trimmed = request;
    trimmed.missing_chunks.clear();
    for (const auto &chunk : request.missing_chunks) {
        if (chunk.chunk_index >= resume_prefix_count) {
            trimmed.missing_chunks.push_back(chunk);
        }
    }
    return trimmed;
}

const sync::ManifestFile *FindRemoteFile(const std::vector<sync::ManifestFile> &manifest,
                                         const std::string &file_id,
                                         const std::string &relative_path) {
    for (const auto &file : manifest) {
        if (file.summary.file_id == file_id && file.summary.relative_path == relative_path) {
            return &file;
        }
    }
    return nullptr;
}

std::unique_ptr<sync::TransportEndpoint> ConnectWithRetry(const std::string &host,
                                                          std::uint16_t port,
                                                          int attempts,
                                                          std::chrono::milliseconds delay) {
    std::runtime_error last_error("connect retry exhausted");
    for (int attempt = 0; attempt < attempts; ++attempt) {
        try {
            return sync::ConnectTcp(host, port);
        } catch (const std::runtime_error &ex) {
            last_error = std::runtime_error(ex.what());
            if (attempt + 1 == attempts) {
                break;
            }
            std::this_thread::sleep_for(delay);
        }
    }
    throw last_error;
}

void RunServer(const std::filesystem::path &remote_root,
               const std::string &remote_db_path,
               std::uint16_t port) {
    sync::ChunkerOptions options;
    options.chunk_size = 4 * 1024 * 1024;

    sync::Chunker remote_chunker(options, std::make_unique<sync::Md5Hasher>());
    sync::SqliteMetadataStore remote_store(remote_db_path);
    sync::Scanner remote_scanner(remote_chunker, remote_store);
    remote_scanner.ScanDirectory(remote_root);

    sync::DiffEngine diff_engine;
    const auto remote_files = remote_store.ListFiles();
    const auto manifest = diff_engine.BuildManifest(remote_files);

    std::unordered_map<std::string, std::filesystem::path> source_paths;
    for (const auto &file : remote_files) {
        source_paths[file.file_id] = file.absolute_path;
    }

    sync::TcpListener listener(port);
    auto endpoint = listener.Accept();

    sync::HelloMessage hello;
    hello.node_id = "remote-node";
    hello.device_name = "yun-cunchu-v1-server";
    hello.protocol_version = "1.0";
    endpoint->Send(sync::ProtocolCodec::EncodeHello(hello));
    std::cout << "[server] hello sent\n";
    endpoint->Send(sync::ProtocolCodec::EncodeManifestSummary(manifest));
    std::cout << "[server] manifest sent files=" << manifest.size() << "\n";

    sync::ChunkReader reader(std::make_unique<sync::Md5Hasher>());
    sync::MessageFrame frame;
    while (endpoint->Receive(&frame)) {
        if (frame.type == sync::MessageType::kMissingChunksRequest) {
            const auto request = sync::ProtocolCodec::DecodeMissingChunksRequest(frame);
            const auto *remote_file = FindRemoteFile(manifest, request.file_id, request.relative_path);
            if (remote_file == nullptr) {
                endpoint->Send(sync::ProtocolCodec::EncodeError({"unknown file in missing chunk request"}));
                continue;
            }
            const auto path_it = source_paths.find(request.file_id);
            if (path_it == source_paths.end()) {
                endpoint->Send(sync::ProtocolCodec::EncodeError({"missing source path for file request"}));
                continue;
            }

            for (const auto &chunk : request.missing_chunks) {
                auto bytes = reader.ReadAndVerify(path_it->second, chunk);
                sync::ChunkDataMessage response;
                response.header.file_id = request.file_id;
                response.header.chunk_hash = chunk.chunk_hash;
                response.header.chunk_index = chunk.chunk_index;
                response.header.offset = chunk.offset;
                response.header.payload_size = bytes.size();
                response.payload_bytes.assign(reinterpret_cast<const char *>(bytes.data()), bytes.size());
                endpoint->Send(sync::ProtocolCodec::EncodeChunkData(response));
            }
            continue;
        }

        if (frame.type == sync::MessageType::kSyncAck) {
            const auto ack = sync::ProtocolCodec::DecodeSyncAck(frame);
            std::cout << "[server] sync ack success=" << ack.success
                      << " detail=" << ack.detail << "\n";
            break;
        }
    }
}

void RunClient(const std::filesystem::path &local_root,
               const std::string &local_db_path,
               std::uint16_t port,
               sync::IStorageBackend &backend) {
    sync::ChunkerOptions options;
    options.chunk_size = 4 * 1024 * 1024;

    sync::Chunker local_chunker(options, std::make_unique<sync::Md5Hasher>());
    sync::SqliteMetadataStore local_store(local_db_path);
    sync::Scanner local_scanner(local_chunker, local_store);
    local_scanner.ScanDirectory(local_root);

    auto endpoint = ConnectWithRetry("127.0.0.1", port, 20, std::chrono::milliseconds(100));
    sync::MessageFrame frame;

    if (!endpoint->Receive(&frame)) {
        throw std::runtime_error("client failed to receive hello");
    }
    const auto hello = sync::ProtocolCodec::DecodeHello(frame);
    std::cout << "[client] hello from " << hello.node_id
              << " device=" << hello.device_name << "\n";

    if (!endpoint->Receive(&frame)) {
        throw std::runtime_error("client failed to receive manifest");
    }
    std::cout << "[client] manifest frame received\n";
    const auto remote_manifest = sync::ProtocolCodec::DecodeManifestSummary(frame);
    sync::DiffEngine diff_engine;
    const auto diff_result = diff_engine.Compare(local_store, remote_manifest);
    std::cout << "[client] remote files=" << remote_manifest.size()
              << " identical=" << diff_result.identical_files.size()
              << " requests=" << diff_result.chunk_requests.size() << "\n";

    sync::ChunkReader local_reader(std::make_unique<sync::Md5Hasher>());
    for (const auto &request : diff_result.chunk_requests) {
        const auto *remote_file = FindRemoteFile(remote_manifest, request.file_id, request.relative_path);
        if (remote_file == nullptr) {
            throw std::runtime_error("client missing remote manifest file");
        }

        const std::filesystem::path final_path = local_root / request.relative_path;
        const std::string task_id = MakeTaskId(request);
        sync::SyncTask task = local_store.GetTask(task_id).value_or(sync::SyncTask{});
        task.task_id = task_id;
        task.source_node = "remote-node";
        task.target_node = "local-node";
        task.file_id = request.file_id;
        task.relative_path = request.relative_path;
        task.expected_root_hash = request.expected_root_hash;
        task.temp_path = (final_path.string() + ".part");
        task.total_chunks = static_cast<std::uint32_t>(remote_file->chunks.size());
        task.state = sync::SyncTaskState::kRunning;
        task.error_message.clear();
        local_store.UpsertTask(task);

        const auto completed_chunks = local_store.ListTaskChunks(task_id);
        std::uint32_t resume_prefix_count = ComputeResumePrefixCount(completed_chunks, remote_file->chunks);
        const std::uint64_t expected_prefix_bytes = ComputePrefixBytes(remote_file->chunks, resume_prefix_count);
        const auto trimmed_request = TrimCompletedChunks(request, resume_prefix_count);

        if (!trimmed_request.missing_chunks.empty()) {
            endpoint->Send(sync::ProtocolCodec::EncodeMissingChunksRequest(trimmed_request));
        }

        std::unordered_map<std::uint32_t, std::vector<std::uint8_t>> received_chunks;
        for (std::size_t i = 0; i < trimmed_request.missing_chunks.size(); ++i) {
            if (!endpoint->Receive(&frame)) {
                throw std::runtime_error("client failed to receive chunk data");
            }
            if (frame.type == sync::MessageType::kError) {
                throw std::runtime_error(sync::ProtocolCodec::DecodeError(frame).detail);
            }
            const auto chunk_message = sync::ProtocolCodec::DecodeChunkData(frame);
            received_chunks.emplace(
                chunk_message.header.chunk_index,
                std::vector<std::uint8_t>(chunk_message.payload_bytes.begin(), chunk_message.payload_bytes.end()));
        }

        const auto local_file = local_store.GetFileByPath(final_path.lexically_normal().string());
        const auto local_chunks = BuildLocalChunkMap(local_file);
        std::unordered_set<std::uint32_t> missing_indexes;
        for (const auto &chunk : trimmed_request.missing_chunks) {
            missing_indexes.insert(chunk.chunk_index);
        }

        sync::SyncWriter writer(std::make_unique<sync::Md5Hasher>());
        if (resume_prefix_count > 0) {
            writer.Resume(final_path);
            if (writer.BytesWritten() != expected_prefix_bytes) {
                writer.Abort();
                local_store.ClearTaskChunks(task_id);
                task.completed_chunks = 0;
                local_store.UpsertTask(task);
                resume_prefix_count = 0;
                writer.Begin(final_path);
            } else {
                task.completed_chunks = resume_prefix_count;
                local_store.UpsertTask(task);
            }
        } else {
            local_store.ClearTaskChunks(task_id);
            task.completed_chunks = 0;
            local_store.UpsertTask(task);
            writer.Begin(final_path);
        }

        try {
            for (const auto &remote_chunk : remote_file->chunks) {
                if (remote_chunk.chunk_index < resume_prefix_count) {
                    continue;
                }

                std::vector<std::uint8_t> bytes;
                if (missing_indexes.count(remote_chunk.chunk_index) > 0) {
                    const auto recv_it = received_chunks.find(remote_chunk.chunk_index);
                    if (recv_it == received_chunks.end()) {
                        throw std::runtime_error("missing received chunk during reconstruction");
                    }
                    bytes = recv_it->second;
                } else {
                    const auto local_it = local_chunks.find(remote_chunk.chunk_index);
                    if (local_it == local_chunks.end() || !local_file.has_value() ||
                        local_it->second.chunk_hash != remote_chunk.chunk_hash ||
                        local_it->second.size != remote_chunk.size) {
                        throw std::runtime_error("local reusable chunk missing during reconstruction");
                    }
                    bytes = local_reader.ReadAndVerify(local_file->absolute_path, remote_chunk);
                }

                writer.Append(bytes);
                local_store.MarkTaskChunkComplete(ToTaskChunk(task_id, request.file_id, remote_chunk));
                task.completed_chunks = remote_chunk.chunk_index + 1;
                local_store.UpsertTask(task);
            }

            task.state = sync::SyncTaskState::kVerifying;
            local_store.UpsertTask(task);
            if (!writer.VerifyAndCommit(remote_file->summary.root_hash)) {
                throw std::runtime_error("client verify/commit failed");
            }

            local_store.ClearTaskChunks(task_id);
            task.completed_chunks = task.total_chunks;
            task.state = sync::SyncTaskState::kCommitted;
            local_store.UpsertTask(task);
        } catch (const std::exception &ex) {
            task.state = sync::SyncTaskState::kFailed;
            ++task.retry_count;
            task.error_message = ex.what();
            local_store.UpsertTask(task);
            throw;
        }

        sync::FileRecord refreshed = local_chunker.BuildFileRecord(final_path, local_root);
        local_store.UpsertFile(refreshed);

        const auto stored_object = backend.StoreFile(refreshed, final_path);
        local_store.UpsertStoredObject(stored_object);
        std::cout << "[client] synced " << request.relative_path
                  << " backend=" << stored_object.backend_name
                  << " key=" << stored_object.storage_key;
        if (!stored_object.access_url.empty()) {
            std::cout << " url=" << stored_object.access_url;
        }
        std::cout << "\n";
    }

    sync::SyncAckMessage ack;
    ack.file_id = "";
    ack.relative_path = "";
    ack.success = true;
    ack.detail = "sync_v1 completed";
    endpoint->Send(sync::ProtocolCodec::EncodeSyncAck(ack));
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 5 || argc > 7) {
        std::cerr << "usage: sync_v1 <local_root> <local_db_path> <remote_root> <remote_db_path> [backend_name] [backend_arg]\n";
        std::cerr << "backend_name: none | mirror | fastdfs\n";
        return 1;
    }

    const std::filesystem::path local_root = argv[1];
    const std::string local_db_path = argv[2];
    const std::filesystem::path remote_root = argv[3];
    const std::string remote_db_path = argv[4];
    const std::string backend_name = (argc >= 6) ? argv[5] : "mirror";
    std::string backend_arg = (argc >= 7) ? argv[6] : "";

    if (backend_name == "mirror" && backend_arg.empty()) {
        backend_arg = (local_root / ".sync_store").string();
    }

    sync::InMemoryMetadataStore runtime_metadata;
    netdisk::SystemRuntime runtime(runtime_metadata);
    (void)runtime;

    auto backend = sync::CreateStorageBackend(backend_name, backend_arg);
    const std::uint16_t port = 39123;
    int server_rc = 0;

    std::thread server([&]() {
        try {
            RunServer(remote_root, remote_db_path, port);
        } catch (const std::exception &ex) {
            server_rc = 1;
            std::cerr << "[server] error: " << ex.what() << "\n";
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int rc = 0;
    try {
        RunClient(local_root, local_db_path, port, *backend);
    } catch (const std::exception &ex) {
        rc = 1;
        std::cerr << "[client] error: " << ex.what() << "\n";
    }

    server.join();
    return (rc != 0) ? rc : server_rc;
}
