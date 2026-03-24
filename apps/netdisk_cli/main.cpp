#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

#include "netdisk/control_plane/system_runtime.h"
#include "netdisk/metadata/metadata_store.h"
#include "netdisk/storage/storage_backend.h"

namespace {

std::uint64_t NowEpoch() {
    return static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

void PrintUsage() {
    std::cout
        << "usage:\n"
        << "  netdisk_cli backup-run <db_path> <node_id> <source_path> <backend_name> [backend_arg]\n"
        << "  netdisk_cli share-run <db_path> <owner_node_id> <entry_id> <source_file> <display_name> "
           "<library_root> <target_node_id> <receive_root>\n"
        << "  netdisk_cli inspect <db_path>\n";
}

netdisk::Status EnsureNode(netdisk::SystemRuntime &runtime, const std::string &node_id) {
    sync::NodeInfo node;
    node.node_id = node_id;
    node.device_name = node_id;
    node.zt_ip = "127.0.0.1";
    node.last_seen_epoch = NowEpoch();
    return runtime.devices().RegisterDevice(node);
}

int RunBackup(int argc, char **argv) {
    if (argc < 6) {
        PrintUsage();
        return 1;
    }

    const std::string db_path = argv[2];
    const std::string node_id = argv[3];
    const std::string source_path = argv[4];
    const std::string backend_name = argv[5];
    const std::string backend_arg = argc >= 7 ? argv[6] : "";

    sync::SqliteMetadataStore metadata_store(db_path);
    netdisk::SystemRuntime runtime(metadata_store);
    EnsureNode(runtime, node_id);

    sync::BackupPolicy policy;
    policy.policy_id = "policy-" + node_id;
    policy.node_id = node_id;
    policy.source_path = source_path;
    policy.schedule_expression = "manual";
    policy.created_at_epoch = NowEpoch();
    auto status = runtime.backup().ConfigurePolicy(policy);
    if (!status.ok()) {
        std::cerr << "configure policy failed: " << status.message << "\n";
        return 2;
    }

    sync::BackupJob job;
    job.job_id = "backup-" + std::to_string(NowEpoch());
    job.node_id = node_id;
    job.source_path = source_path;
    job.destination_label = backend_name;
    job.created_at_epoch = NowEpoch();

    auto backend = sync::CreateStorageBackend(backend_name, backend_arg);
    status = runtime.backup().ExecuteBackup(job, *backend);
    if (!status.ok()) {
        std::cerr << "backup failed: " << status.message << "\n";
        return 3;
    }

    std::cout << "backup_ok files=" << runtime.file_index().ListIndexedFiles().size()
              << " stored=" << metadata_store.ListStoredObjects().size()
              << " backup_records=" << runtime.file_index().ListBackupRecordsForNode(node_id).size() << "\n";
    return 0;
}

int RunShare(int argc, char **argv) {
    if (argc < 10) {
        PrintUsage();
        return 1;
    }

    const std::string db_path = argv[2];
    const std::string owner_node_id = argv[3];
    const std::string entry_id = argv[4];
    const std::string source_file = argv[5];
    const std::string display_name = argv[6];
    const std::string library_root = argv[7];
    const std::string target_node_id = argv[8];
    const std::string receive_root = argv[9];

    sync::SqliteMetadataStore metadata_store(db_path);
    netdisk::SystemRuntime runtime(metadata_store);
    EnsureNode(runtime, owner_node_id);
    EnsureNode(runtime, target_node_id);

    sync::TransferPolicy policy;
    policy.policy_id = "transfer-default";
    policy.retention_seconds = 3 * 24 * 3600;
    policy.auto_cleanup = true;
    policy.private_library_only = true;
    auto status = runtime.shared_library().ConfigurePolicy(policy);
    if (!status.ok()) {
        std::cerr << "configure transfer policy failed: " << status.message << "\n";
        return 2;
    }

    sync::SharedLibraryEntry entry;
    entry.entry_id = entry_id;
    entry.owner_node_id = owner_node_id;
    entry.source_path = source_file;
    entry.display_name = display_name;
    entry.note = "cli-share";
    entry.created_at_epoch = NowEpoch();
    entry.expires_at_epoch = entry.created_at_epoch + policy.retention_seconds;

    status = runtime.shared_library().PublishFile(entry, library_root);
    if (!status.ok()) {
        std::cerr << "publish failed: " << status.message << "\n";
        return 3;
    }

    status = runtime.shared_library().ReceiveEntry(entry_id, target_node_id, receive_root);
    if (!status.ok()) {
        std::cerr << "receive failed: " << status.message << "\n";
        return 4;
    }

    std::cout << "share_ok entries=" << runtime.shared_library().ListEntries().size()
              << " received="
              << std::filesystem::exists(std::filesystem::path(receive_root) / display_name) << "\n";
    return 0;
}

int RunInspect(int argc, char **argv) {
    if (argc < 3) {
        PrintUsage();
        return 1;
    }

    sync::SqliteMetadataStore metadata_store(argv[2]);
    const auto files = metadata_store.ListFiles();
    const auto stored = metadata_store.ListStoredObjects();
    const auto backup_records = metadata_store.ListBackupRecords();

    std::cout << "files=" << files.size() << "\n";
    for (const auto &file : files) {
        std::cout << "file " << file.relative_path << " " << file.root_hash << "\n";
    }

    std::cout << "stored_objects=" << stored.size() << "\n";
    for (const auto &record : stored) {
        std::cout << "stored " << record.file_id << " " << record.backend_name << " " << record.storage_key << "\n";
    }

    std::cout << "backup_records=" << backup_records.size() << "\n";
    for (const auto &record : backup_records) {
        std::cout << "backup " << record.node_id << " " << record.relative_path << " " << record.file_id << "\n";
    }
    return 0;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    const std::string command = argv[1];
    if (command == "backup-run") {
        return RunBackup(argc, argv);
    }
    if (command == "share-run") {
        return RunShare(argc, argv);
    }
    if (command == "inspect") {
        return RunInspect(argc, argv);
    }

    PrintUsage();
    return 1;
}
