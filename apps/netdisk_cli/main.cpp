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

std::string RestoreRequestStateName(sync::RestoreRequestState state) {
    switch (state) {
    case sync::RestoreRequestState::kQueued:
        return "queued";
    case sync::RestoreRequestState::kRunning:
        return "running";
    case sync::RestoreRequestState::kCompleted:
        return "completed";
    case sync::RestoreRequestState::kFailed:
        return "failed";
    }
    return "unknown";
}

std::string SharedRecipientStateName(sync::SharedRecipientState state) {
    switch (state) {
    case sync::SharedRecipientState::kPending:
        return "pending";
    case sync::SharedRecipientState::kDelivered:
        return "delivered";
    case sync::SharedRecipientState::kFailed:
        return "failed";
    }
    return "unknown";
}

void PrintUsage() {
    std::cout
        << "usage:\n"
        << "  netdisk_cli backup-run <db_path> <node_id> <source_path> <backend_name> [backend_arg]\n"
        << "  netdisk_cli share-publish <db_path> <owner_node_id> <entry_id> <source_file> <display_name> "
           "<library_root>\n"
        << "  netdisk_cli share-receive <db_path> <entry_id> <target_node_id> <receive_root>\n"
        << "  netdisk_cli share-run <db_path> <owner_node_id> <entry_id> <source_file> <display_name> "
           "<library_root> <target_node_id> <receive_root>\n"
        << "  netdisk_cli share-list <db_path> <node_id> <scope>\n"
        << "  netdisk_cli share-cleanup <db_path> <now_epoch>\n"
        << "  netdisk_cli coord-status <db_path> <now_epoch> <offline_after_seconds>\n"
        << "  netdisk_cli task-retry <db_path> <task_id> [detail]\n"
        << "  netdisk_cli restore-submit <db_path> <request_id> <file_id> <source_node_id> <target_node_id> "
           "<destination_path>\n"
        << "  netdisk_cli restore-run <db_path> <request_id>\n"
        << "  netdisk_cli restore-retry <db_path> <request_id>\n"
        << "  netdisk_cli restore-latest <db_path> <source_node_id> <relative_path> <target_node_id> "
           "<destination_path>\n"
        << "  netdisk_cli share-retry <db_path> <entry_id> <target_node_id> <receive_root>\n"
        << "  netdisk_cli backup-history <db_path> <node_id> <relative_path>\n"
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

int RunSharePublish(int argc, char **argv) {
    if (argc < 8) {
        PrintUsage();
        return 1;
    }

    const std::string db_path = argv[2];
    const std::string owner_node_id = argv[3];
    const std::string entry_id = argv[4];
    const std::string source_file = argv[5];
    const std::string display_name = argv[6];
    const std::string library_root = argv[7];

    sync::SqliteMetadataStore metadata_store(db_path);
    netdisk::SystemRuntime runtime(metadata_store);
    EnsureNode(runtime, owner_node_id);

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
    entry.note = "cli-share-publish";
    entry.created_at_epoch = NowEpoch();
    entry.expires_at_epoch = entry.created_at_epoch + policy.retention_seconds;

    status = runtime.shared_library().PublishFile(entry, library_root);
    if (!status.ok()) {
        std::cerr << "publish failed: " << status.message << "\n";
        return 3;
    }

    std::cout << "share_publish_ok entries=" << runtime.shared_library().ListEntries().size() << "\n";
    return 0;
}

int RunShareReceive(int argc, char **argv) {
    if (argc < 6) {
        PrintUsage();
        return 1;
    }

    const std::string db_path = argv[2];
    const std::string entry_id = argv[3];
    const std::string target_node_id = argv[4];
    const std::string receive_root = argv[5];

    sync::SqliteMetadataStore metadata_store(db_path);
    netdisk::SystemRuntime runtime(metadata_store);
    EnsureNode(runtime, target_node_id);

    auto entry = runtime.shared_library().FindEntry(entry_id);
    if (!entry.has_value()) {
        std::cerr << "receive failed: shared entry not found\n";
        return 2;
    }

    const auto status = runtime.shared_library().ReceiveEntry(entry_id, target_node_id, receive_root);
    if (!status.ok()) {
        std::cerr << "receive failed: " << status.message << "\n";
        return 3;
    }

    std::cout << "share_receive_ok entry=" << entry_id << " received="
              << std::filesystem::exists(std::filesystem::path(receive_root) / entry->display_name) << "\n";
    return 0;
}

int RunInspect(int argc, char **argv) {
    if (argc < 3) {
        PrintUsage();
        return 1;
    }

    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const auto files = metadata_store.ListFiles();
    const auto stored = metadata_store.ListStoredObjects();
    const auto backup_records = metadata_store.ListBackupRecords();
    const auto device_files = metadata_store.ListDeviceFiles();
    runtime.network().RefreshPeersFromDevices(runtime.devices().ListDevices(), NowEpoch(), 300);
    const auto nodes = runtime.devices().ListDevices();
    const auto online_nodes = runtime.devices().ListOnlineDevices(NowEpoch(), 300);
    const auto peers = runtime.network().ListPeers();
    const auto online_peers = runtime.network().ListOnlinePeers();
    const auto backup_policies = runtime.policies().ListBackupPolicies();
    const auto transfer_policies = runtime.policies().ListTransferPolicies();
    const auto tasks = runtime.tasks().ListTasks();
    const auto backup_jobs = runtime.backup().ListJobs();
    const auto restore_requests = runtime.recovery().ListRestoreRequests();
    const auto shared_entries = runtime.shared_library().ListEntries();

    std::cout << "nodes=" << nodes.size() << "\n";
    for (const auto &node : nodes) {
        std::cout << "node " << node.node_id << " " << node.device_name << " " << node.zt_ip << "\n";
    }
    std::cout << "online_nodes=" << online_nodes.size() << "\n";
    std::cout << "peers=" << peers.size() << "\n";
    std::cout << "online_peers=" << online_peers.size() << "\n";

    std::cout << "backup_policies=" << backup_policies.size() << "\n";
    for (const auto &policy : backup_policies) {
        std::cout << "backup_policy " << policy.policy_id << " " << policy.node_id << " " << policy.source_path
                  << "\n";
    }

    std::cout << "transfer_policies=" << transfer_policies.size() << "\n";
    for (const auto &policy : transfer_policies) {
        std::cout << "transfer_policy " << policy.policy_id << " retention=" << policy.retention_seconds << "\n";
    }

    std::cout << "scheduled_tasks=" << tasks.size() << "\n";
    for (const auto &task : tasks) {
        std::cout << "task " << task.task_id << " domain=" << static_cast<int>(task.domain)
                  << " state=" << static_cast<int>(task.state) << " retry=" << task.retry_count
                  << " detail=" << task.detail << "\n";
    }

    std::cout << "backup_jobs=" << backup_jobs.size() << "\n";
    for (const auto &job : backup_jobs) {
        std::cout << "backup_job " << job.job_id << " " << job.node_id << " " << job.source_path << "\n";
    }

    std::cout << "restore_requests=" << restore_requests.size() << "\n";
    for (const auto &request : restore_requests) {
        std::cout << "restore_request " << request.request_id << " " << request.file_id << " "
                  << request.target_node_id << " " << request.destination_path
                  << " state=" << RestoreRequestStateName(request.state)
                  << " completed_at=" << request.completed_at_epoch
                  << " error=" << request.error_message << "\n";
    }

    std::cout << "shared_entries=" << shared_entries.size() << "\n";
    for (const auto &entry : shared_entries) {
        std::cout << "shared_entry " << entry.entry_id << " owner=" << entry.owner_node_id << " file_id="
                  << entry.file_id << " expired=" << entry.expired << " delivered=" << entry.delivered
                  << " recipients=";
        for (std::size_t i = 0; i < entry.recipients.size(); ++i) {
            if (i != 0) {
                std::cout << ",";
            }
            const auto &recipient = entry.recipients[i];
            std::cout << recipient.node_id << ":" << SharedRecipientStateName(recipient.state);
        }
        std::cout << "\n";
    }

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
    std::cout << "device_files=" << device_files.size() << "\n";
    for (const auto &record : device_files) {
        std::cout << "device_file " << record.node_id << " " << record.relative_path << " " << record.absolute_path
                  << " " << record.file_id << " source=" << record.source_kind
                  << " ref=" << record.source_ref_id << "\n";
    }
    return 0;
}

int RunShareList(int argc, char **argv) {
    if (argc < 5) {
        PrintUsage();
        return 1;
    }

    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const std::string node_id = argv[3];
    const std::string scope = argv[4];

    std::vector<sync::SharedLibraryEntry> entries;
    if (scope == "all") {
        entries = runtime.shared_library().ListEntriesForNode(node_id);
    } else if (scope == "active") {
        for (const auto &entry : runtime.shared_library().ListEntriesForNode(node_id)) {
            if (!entry.expired) {
                entries.push_back(entry);
            }
        }
    } else if (scope == "pending") {
        entries = runtime.shared_library().ListPendingEntriesForNode(node_id);
    } else {
        std::cerr << "share list scope must be one of: all | active | pending\n";
        return 2;
    }

    std::cout << "share_list=" << entries.size() << "\n";
    for (const auto &entry : entries) {
        std::cout << "entry " << entry.entry_id << " owner=" << entry.owner_node_id << " expired=" << entry.expired
                  << " delivered=" << entry.delivered << " recipients=";
        for (std::size_t i = 0; i < entry.recipients.size(); ++i) {
            if (i != 0) {
                std::cout << ",";
            }
            const auto &recipient = entry.recipients[i];
            std::cout << recipient.node_id << ":" << SharedRecipientStateName(recipient.state);
        }
        std::cout << "\n";
    }
    return 0;
}

int RunShareCleanup(int argc, char **argv) {
    if (argc < 4) {
        PrintUsage();
        return 1;
    }

    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const auto now_epoch = static_cast<std::uint64_t>(std::stoull(argv[3]));
    const auto removed = runtime.shared_library().CleanupExpired(now_epoch);
    std::cout << "share_cleanup removed=" << removed << "\n";
    return 0;
}

int RunCoordStatus(int argc, char **argv) {
    if (argc < 5) {
        PrintUsage();
        return 1;
    }

    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const auto now_epoch = static_cast<std::uint64_t>(std::stoull(argv[3]));
    const auto offline_after_seconds = static_cast<std::uint64_t>(std::stoull(argv[4]));
    const auto devices = runtime.devices().ListDevices();
    runtime.network().RefreshPeersFromDevices(devices, now_epoch, offline_after_seconds);

    const auto online_devices = runtime.devices().ListOnlineDevices(now_epoch, offline_after_seconds);
    const auto online_peers = runtime.network().ListOnlinePeers();
    const auto queued_tasks = runtime.tasks().ListTasksByState(sync::JobState::kQueued);
    const auto failed_tasks = runtime.tasks().ListRetryableTasks();

    std::cout << "coord devices=" << devices.size() << " online_devices=" << online_devices.size()
              << " online_peers=" << online_peers.size() << " queued_tasks=" << queued_tasks.size()
              << " retryable_tasks=" << failed_tasks.size() << "\n";
    return 0;
}

int RunTaskRetry(int argc, char **argv) {
    if (argc < 4) {
        PrintUsage();
        return 1;
    }

    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const std::string detail = argc >= 5 ? argv[4] : "manual retry requested";
    const auto status = runtime.tasks().RetryTask(argv[3], detail);
    if (!status.ok()) {
        std::cerr << "task retry failed: " << status.message << "\n";
        return 2;
    }

    std::cout << "task_retry_ok task=" << argv[3] << "\n";
    return 0;
}

int RunBackupHistory(int argc, char **argv) {
    if (argc < 5) {
        PrintUsage();
        return 1;
    }

    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const auto history = runtime.file_index().ListBackupHistory(argv[3], argv[4]);
    std::cout << "backup_history=" << history.size() << "\n";
    for (const auto &record : history) {
        std::cout << "history " << record.node_id << " " << record.relative_path << " " << record.file_id << " "
                  << record.backend_name << " " << record.storage_key << " " << record.backed_up_at_epoch << "\n";
    }
    return 0;
}

int RunRestoreSubmit(int argc, char **argv) {
    if (argc < 8) {
        PrintUsage();
        return 1;
    }

    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    EnsureNode(runtime, argv[5]);
    EnsureNode(runtime, argv[6]);

    sync::RestoreRequest request;
    request.request_id = argv[3];
    request.file_id = argv[4];
    request.source_node_id = argv[5];
    request.target_node_id = argv[6];
    request.destination_path = argv[7];
    request.created_at_epoch = NowEpoch();

    const auto status = runtime.recovery().SubmitRestore(request);
    if (!status.ok()) {
        std::cerr << "restore submit failed: " << status.message << "\n";
        return 2;
    }

    std::cout << "restore_ok requests=" << runtime.recovery().ListRestoreRequests().size() << "\n";
    return 0;
}

int RunRestoreRun(int argc, char **argv) {
    if (argc < 4) {
        PrintUsage();
        return 1;
    }

    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const auto status = runtime.recovery().ExecuteRestore(argv[3]);
    if (!status.ok()) {
        std::cerr << "restore run failed: " << status.message << "\n";
        return 2;
    }

    std::cout << "restore_run_ok request=" << argv[3] << "\n";
    return 0;
}

int RunRestoreRetry(int argc, char **argv) {
    if (argc < 4) {
        PrintUsage();
        return 1;
    }

    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const auto status = runtime.recovery().RetryRestore(argv[3]);
    if (!status.ok()) {
        std::cerr << "restore retry failed: " << status.message << "\n";
        return 2;
    }

    std::cout << "restore_retry_ok request=" << argv[3] << "\n";
    return 0;
}

int RunRestoreLatest(int argc, char **argv) {
    if (argc < 7) {
        PrintUsage();
        return 1;
    }

    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    EnsureNode(runtime, argv[3]);
    EnsureNode(runtime, argv[5]);

    const auto status = runtime.recovery().ExecuteLatestRestore(argv[3], argv[4], argv[5], argv[6]);
    if (!status.ok()) {
        std::cerr << "restore latest failed: " << status.message << "\n";
        return 2;
    }

    std::cout << "restore_latest_ok source_node=" << argv[3] << " relative_path=" << argv[4] << "\n";
    return 0;
}

int RunShareRetry(int argc, char **argv) {
    if (argc < 6) {
        PrintUsage();
        return 1;
    }

    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const auto status = runtime.shared_library().RetryReceive(argv[3], argv[4], argv[5]);
    if (!status.ok()) {
        std::cerr << "share retry failed: " << status.message << "\n";
        return 2;
    }

    std::cout << "share_retry_ok entry=" << argv[3] << " target=" << argv[4] << "\n";
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
    if (command == "share-publish") {
        return RunSharePublish(argc, argv);
    }
    if (command == "share-receive") {
        return RunShareReceive(argc, argv);
    }
    if (command == "share-run") {
        return RunShare(argc, argv);
    }
    if (command == "share-list") {
        return RunShareList(argc, argv);
    }
    if (command == "share-cleanup") {
        return RunShareCleanup(argc, argv);
    }
    if (command == "coord-status") {
        return RunCoordStatus(argc, argv);
    }
    if (command == "task-retry") {
        return RunTaskRetry(argc, argv);
    }
    if (command == "inspect") {
        return RunInspect(argc, argv);
    }
    if (command == "backup-history") {
        return RunBackupHistory(argc, argv);
    }
    if (command == "restore-submit") {
        return RunRestoreSubmit(argc, argv);
    }
    if (command == "restore-run") {
        return RunRestoreRun(argc, argv);
    }
    if (command == "restore-retry") {
        return RunRestoreRetry(argc, argv);
    }
    if (command == "restore-latest") {
        return RunRestoreLatest(argc, argv);
    }
    if (command == "share-retry") {
        return RunShareRetry(argc, argv);
    }

    PrintUsage();
    return 1;
}
