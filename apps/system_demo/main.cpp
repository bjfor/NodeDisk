#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "netdisk/control_plane/system_runtime.h"
#include "netdisk/metadata/metadata_store.h"
#include "netdisk/storage/storage_backend.h"

namespace {

std::uint64_t NowEpoch() {
    return static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

}  // namespace

int main() {
    sync::InMemoryMetadataStore metadata_store;
    netdisk::SystemRuntime runtime(metadata_store);

    const auto now = NowEpoch();
    const std::filesystem::path demo_root = std::filesystem::path("/tmp/netdisk_v1_demo_source");
    const std::filesystem::path store_root = std::filesystem::path("/tmp/netdisk_v1_demo_store");
    const std::filesystem::path library_root = std::filesystem::path("/tmp/netdisk_v1_demo_library");
    const std::filesystem::path receive_root = std::filesystem::path("/tmp/netdisk_v1_demo_receive");
    std::filesystem::create_directories(demo_root / "docs");
    std::filesystem::create_directories(store_root);
    std::filesystem::create_directories(library_root);
    std::filesystem::create_directories(receive_root);
    {
        std::ofstream output(demo_root / "docs" / "report.txt", std::ios::binary);
        output << "important backup payload\n";
    }
    {
        std::ofstream output(demo_root / "docs" / "handoff.txt", std::ios::binary);
        output << "shared library payload\n";
    }

    sync::NodeInfo laptop;
    laptop.node_id = "node-laptop";
    laptop.device_name = "laptop";
    laptop.zt_ip = "10.147.17.2";
    laptop.last_seen_epoch = now;
    runtime.devices().RegisterDevice(laptop);
    metadata_store.UpsertNode(laptop);

    sync::NodeInfo desktop;
    desktop.node_id = "node-desktop";
    desktop.device_name = "desktop";
    desktop.zt_ip = "10.147.17.3";
    desktop.last_seen_epoch = now;
    runtime.devices().RegisterDevice(desktop);
    metadata_store.UpsertNode(desktop);

    sync::AuthIdentity identity;
    identity.user_id = "user-1";
    identity.username = "hjh";
    identity.device_id = laptop.node_id;
    runtime.auth().RegisterIdentity(identity);

    sync::AuthSession session;
    session.session_id = "session-1";
    session.user_id = identity.user_id;
    session.device_id = laptop.node_id;
    session.access_token = "token-demo";
    session.expires_at_epoch = now + 3600;
    runtime.auth().UpsertSession(session);

    sync::OverlayPeer laptop_peer;
    laptop_peer.node_id = laptop.node_id;
    laptop_peer.virtual_ip = laptop.zt_ip;
    laptop_peer.online = true;
    laptop_peer.last_seen_epoch = now;
    runtime.network().UpsertPeer(laptop_peer);

    sync::OverlayPeer desktop_peer;
    desktop_peer.node_id = desktop.node_id;
    desktop_peer.virtual_ip = desktop.zt_ip;
    desktop_peer.online = true;
    desktop_peer.last_seen_epoch = now;
    runtime.network().UpsertPeer(desktop_peer);

    sync::BackupPolicy backup_policy;
    backup_policy.policy_id = "policy-docs";
    backup_policy.node_id = laptop.node_id;
    backup_policy.source_path = demo_root.string();
    backup_policy.schedule_expression = "manual";
    backup_policy.created_at_epoch = now;
    runtime.backup().ConfigurePolicy(backup_policy);

    sync::BackupJob backup_job;
    backup_job.job_id = "backup-job-1";
    backup_job.node_id = laptop.node_id;
    backup_job.source_path = demo_root.string();
    backup_job.destination_label = "primary-store";
    backup_job.created_at_epoch = now;
    auto backend = sync::CreateStorageBackend("mirror", store_root.string());
    runtime.backup().ExecuteBackup(backup_job, *backend);

    sync::TransferPolicy transfer_policy;
    transfer_policy.policy_id = "transfer-default";
    transfer_policy.retention_seconds = 3 * 24 * 3600;
    transfer_policy.auto_cleanup = true;
    transfer_policy.private_library_only = true;
    runtime.shared_library().ConfigurePolicy(transfer_policy);

    sync::SharedLibraryEntry entry;
    entry.entry_id = "entry-report";
    entry.owner_node_id = laptop.node_id;
    entry.source_path = (demo_root / "docs" / "handoff.txt").string();
    entry.display_name = "handoff.txt";
    entry.note = "send to desktop";
    entry.created_at_epoch = now;
    entry.expires_at_epoch = now + transfer_policy.retention_seconds;
    runtime.shared_library().PublishFile(entry, library_root);
    runtime.shared_library().ReceiveEntry(entry.entry_id, desktop.node_id, receive_root);

    sync::RestoreRequest restore_request;
    restore_request.request_id = "restore-job-1";
    restore_request.file_id = "file-report";
    restore_request.source_node_id = laptop.node_id;
    restore_request.target_node_id = desktop.node_id;
    restore_request.destination_path = "/restore/report.zip";
    restore_request.created_at_epoch = now;
    runtime.recovery().SubmitRestore(restore_request);

    std::cout << "devices=" << runtime.devices().ListDevices().size() << "\n";
    std::cout << "online_peers=" << runtime.network().ListOnlinePeers().size() << "\n";
    std::cout << "backup_policies=" << runtime.policies().ListBackupPolicies().size() << "\n";
    std::cout << "backup_jobs=" << runtime.backup().ListJobs().size() << "\n";
    std::cout << "indexed_files=" << runtime.file_index().ListIndexedFiles().size() << "\n";
    std::cout << "backup_records=" << runtime.file_index().ListBackupRecordsForNode(laptop.node_id).size() << "\n";
    std::cout << "transfer_entries=" << runtime.shared_library().ListEntries().size() << "\n";
    std::cout << "transfer_tasks=" << runtime.tasks().ListTasksByDomain(sync::JobDomain::kTransfer).size() << "\n";
    std::cout << "received_exists="
              << std::filesystem::exists(receive_root / "handoff.txt") << "\n";
    std::cout << "restore_requests=" << runtime.recovery().ListRestoreRequests().size() << "\n";
    std::cout << "audit_events=" << runtime.audit().ListEvents().size() << "\n";
    return 0;
}
