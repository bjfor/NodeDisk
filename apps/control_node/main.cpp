#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "netdisk/control_plane/control_node_server.h"
#include "netdisk/control_plane/system_runtime.h"
#include "netdisk/metadata/metadata_store.h"

namespace {

std::uint64_t NowEpoch() {
    return static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

void PrintUsage() {
    std::cout
        << "usage:\n"
        << "  control_node serve <db_path> [host] [port] [offline_after_seconds]\n"
        << "  control_node submit-backup <host> <port> <job_id> <node_id> <source_path>\n"
        << "  control_node submit-restore <host> <port> <request_id> <file_id> <source_node_id> <target_node_id> <destination_path>\n"
        << "  control_node submit-share-receive <host> <port> <entry_id> <target_node_id>\n"
        << "  control_node retry-task-remote <host> <port> <task_id> [detail]\n"
        << "  control_node retry-restore-remote <host> <port> <request_id>\n"
        << "  control_node retry-share-remote <host> <port> <entry_id> <target_node_id>\n"
        << "  control_node summary <db_path> [offline_after_seconds] [--json]\n"
        << "  control_node devices <db_path> <all|online> [offline_after_seconds] [--json]\n"
        << "  control_node tasks <db_path> <all|queued|failed> [--json]\n"
        << "  control_node task-retry <db_path> <task_id> [detail]\n"
        << "  control_node backups <db_path> [node_id] [--json]\n"
        << "  control_node shared <db_path> <node_id> <all|active|pending> [--json]\n"
        << "  control_node share-retry <db_path> <entry_id> <target_node_id> <receive_root>\n"
        << "  control_node restores <db_path> [--json]\n"
        << "  control_node restore-retry <db_path> <request_id>\n"
        << "  control_node audit <db_path> [category] [--json]\n";
}

bool WantsJson(int argc, char **argv) {
    return argc >= 2 && std::string(argv[argc - 1]) == "--json";
}

std::string JoinArgs(int argc, char **argv, int start_index) {
    std::string result;
    for (int i = start_index; i < argc; ++i) {
        if (i > start_index) {
            result += " ";
        }
        result += argv[i];
    }
    return result;
}

std::string JsonEscape(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

std::string ToJsonString(const std::string &value) {
    return "\"" + JsonEscape(value) + "\"";
}

std::string ToJsonBool(bool value) {
    return value ? "true" : "false";
}

std::string JobDomainName(sync::JobDomain domain) {
    switch (domain) {
    case sync::JobDomain::kBackup:
        return "backup";
    case sync::JobDomain::kTransfer:
        return "transfer";
    case sync::JobDomain::kRestore:
        return "restore";
    }
    return "unknown";
}

std::string JobStateName(sync::JobState state) {
    switch (state) {
    case sync::JobState::kQueued:
        return "queued";
    case sync::JobState::kRunning:
        return "running";
    case sync::JobState::kCompleted:
        return "completed";
    case sync::JobState::kFailed:
        return "failed";
    }
    return "unknown";
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

void PrintJsonArrayHeader(const std::string &key) {
    std::cout << "{\"" << key << "\":[";
}

void PrintJsonArrayFooter() {
    std::cout << "]}\n";
}

std::optional<sync::JobState> ParseTaskState(const std::string &scope) {
    if (scope == "queued") {
        return sync::JobState::kQueued;
    }
    if (scope == "failed") {
        return sync::JobState::kFailed;
    }
    return std::nullopt;
}

int RunSummary(int argc, char **argv, bool json_output) {
    if (argc < 3) {
        PrintUsage();
        return 1;
    }
    const auto offline_after = argc >= 4 ? static_cast<std::uint64_t>(std::stoull(argv[3])) : 300ULL;
    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const auto summary = runtime.control().BuildSummary(NowEpoch(), offline_after);
    if (json_output) {
        std::cout << "{"
                  << "\"devices\":" << summary.devices << ","
                  << "\"online_devices\":" << summary.online_devices << ","
                  << "\"peers\":" << summary.peers << ","
                  << "\"online_peers\":" << summary.online_peers << ","
                  << "\"backup_policies\":" << summary.backup_policies << ","
                  << "\"transfer_policies\":" << summary.transfer_policies << ","
                  << "\"scheduled_tasks\":" << summary.scheduled_tasks << ","
                  << "\"queued_tasks\":" << summary.queued_tasks << ","
                  << "\"failed_tasks\":" << summary.failed_tasks << ","
                  << "\"backup_jobs\":" << summary.backup_jobs << ","
                  << "\"restore_requests\":" << summary.restore_requests << ","
                  << "\"running_restore_requests\":" << summary.running_restore_requests << ","
                  << "\"failed_restore_requests\":" << summary.failed_restore_requests << ","
                  << "\"shared_entries\":" << summary.shared_entries << ","
                  << "\"active_shared_entries\":" << summary.active_shared_entries << ","
                  << "\"pending_shared_deliveries\":" << summary.pending_shared_deliveries << ","
                  << "\"indexed_files\":" << summary.indexed_files << ","
                  << "\"device_files\":" << summary.device_files << ","
                  << "\"stored_objects\":" << summary.stored_objects << ","
                  << "\"backup_records\":" << summary.backup_records << ","
                  << "\"audit_events\":" << summary.audit_events << "}\n";
        return 0;
    }
    std::cout << "devices=" << summary.devices << "\n";
    std::cout << "online_devices=" << summary.online_devices << "\n";
    std::cout << "peers=" << summary.peers << "\n";
    std::cout << "online_peers=" << summary.online_peers << "\n";
    std::cout << "backup_policies=" << summary.backup_policies << "\n";
    std::cout << "transfer_policies=" << summary.transfer_policies << "\n";
    std::cout << "scheduled_tasks=" << summary.scheduled_tasks << "\n";
    std::cout << "queued_tasks=" << summary.queued_tasks << "\n";
    std::cout << "failed_tasks=" << summary.failed_tasks << "\n";
    std::cout << "backup_jobs=" << summary.backup_jobs << "\n";
    std::cout << "restore_requests=" << summary.restore_requests << "\n";
    std::cout << "running_restore_requests=" << summary.running_restore_requests << "\n";
    std::cout << "failed_restore_requests=" << summary.failed_restore_requests << "\n";
    std::cout << "shared_entries=" << summary.shared_entries << "\n";
    std::cout << "active_shared_entries=" << summary.active_shared_entries << "\n";
    std::cout << "pending_shared_deliveries=" << summary.pending_shared_deliveries << "\n";
    std::cout << "indexed_files=" << summary.indexed_files << "\n";
    std::cout << "device_files=" << summary.device_files << "\n";
    std::cout << "stored_objects=" << summary.stored_objects << "\n";
    std::cout << "backup_records=" << summary.backup_records << "\n";
    std::cout << "audit_events=" << summary.audit_events << "\n";
    return 0;
}

int RunServe(int argc, char **argv) {
    if (argc < 3) {
        PrintUsage();
        return 1;
    }

    const std::string host = argc >= 4 ? argv[3] : "127.0.0.1";
    const auto port = argc >= 5 ? static_cast<std::uint16_t>(std::stoul(argv[4])) : 18765U;
    const auto offline_after = argc >= 6 ? static_cast<std::uint64_t>(std::stoull(argv[5])) : 300ULL;

    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    netdisk::ControlNodeServer server(runtime);
    const auto status = server.Serve(host, port, offline_after);
    if (!status.ok()) {
        std::cerr << "serve failed: " << status.message << "\n";
        return 2;
    }
    return 0;
}

int RunSubmitBackup(int argc, char **argv) {
    if (argc < 7) {
        PrintUsage();
        return 1;
    }
    netdisk::ControlNodeClient client(argv[2], static_cast<std::uint16_t>(std::stoul(argv[3])));
    sync::BackupJob job;
    job.job_id = argv[4];
    job.node_id = argv[5];
    job.source_path = JoinArgs(argc, argv, 6);
    job.destination_label = "service-submitted";
    job.created_at_epoch = NowEpoch();
    const auto status = client.SubmitBackupJob(job);
    if (!status.ok()) {
        std::cerr << "submit backup failed: " << status.message << "\n";
        return 2;
    }
    std::cout << "submit_backup_ok job=" << job.job_id << "\n";
    return 0;
}

int RunSubmitRestore(int argc, char **argv) {
    if (argc < 9) {
        PrintUsage();
        return 1;
    }
    netdisk::ControlNodeClient client(argv[2], static_cast<std::uint16_t>(std::stoul(argv[3])));
    sync::RestoreRequest request;
    request.request_id = argv[4];
    request.file_id = argv[5];
    request.source_node_id = argv[6];
    request.target_node_id = argv[7];
    request.destination_path = JoinArgs(argc, argv, 8);
    request.created_at_epoch = NowEpoch();
    const auto status = client.SubmitRestoreRequest(request);
    if (!status.ok()) {
        std::cerr << "submit restore failed: " << status.message << "\n";
        return 2;
    }
    std::cout << "submit_restore_ok request=" << request.request_id << "\n";
    return 0;
}

int RunSubmitShareReceive(int argc, char **argv) {
    if (argc < 6) {
        PrintUsage();
        return 1;
    }
    netdisk::ControlNodeClient client(argv[2], static_cast<std::uint16_t>(std::stoul(argv[3])));
    const auto status = client.SubmitShareReceive(argv[4], argv[5]);
    if (!status.ok()) {
        std::cerr << "submit share receive failed: " << status.message << "\n";
        return 2;
    }
    std::cout << "submit_share_receive_ok entry=" << argv[4] << " target=" << argv[5] << "\n";
    return 0;
}

int RunRetryTaskRemote(int argc, char **argv) {
    if (argc < 5) {
        PrintUsage();
        return 1;
    }
    netdisk::ControlNodeClient client(argv[2], static_cast<std::uint16_t>(std::stoul(argv[3])));
    const std::string detail = argc >= 6 ? JoinArgs(argc, argv, 5) : "task retried via remote control";
    const auto status = client.RetryTask(argv[4], detail);
    if (!status.ok()) {
        std::cerr << "remote retry task failed: " << status.message << "\n";
        return 2;
    }
    std::cout << "retry_task_remote_ok task=" << argv[4] << "\n";
    return 0;
}

int RunRetryRestoreRemote(int argc, char **argv) {
    if (argc < 5) {
        PrintUsage();
        return 1;
    }
    netdisk::ControlNodeClient client(argv[2], static_cast<std::uint16_t>(std::stoul(argv[3])));
    const auto status = client.RetryRestore(argv[4]);
    if (!status.ok()) {
        std::cerr << "remote retry restore failed: " << status.message << "\n";
        return 2;
    }
    std::cout << "retry_restore_remote_ok request=" << argv[4] << "\n";
    return 0;
}

int RunRetryShareRemote(int argc, char **argv) {
    if (argc < 6) {
        PrintUsage();
        return 1;
    }
    netdisk::ControlNodeClient client(argv[2], static_cast<std::uint16_t>(std::stoul(argv[3])));
    const auto status = client.RetryShareReceive(argv[4], argv[5]);
    if (!status.ok()) {
        std::cerr << "remote retry share failed: " << status.message << "\n";
        return 2;
    }
    std::cout << "retry_share_remote_ok entry=" << argv[4] << " target=" << argv[5] << "\n";
    return 0;
}

int RunDevices(int argc, char **argv, bool json_output) {
    if (argc < 4) {
        PrintUsage();
        return 1;
    }
    const std::string scope = argv[3];
    const auto offline_after = argc >= 5 ? static_cast<std::uint64_t>(std::stoull(argv[4])) : 300ULL;
    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const bool online_only = scope == "online";
    const auto devices = runtime.control().ListDevices(online_only, NowEpoch(), offline_after);
    if (json_output) {
        PrintJsonArrayHeader("devices");
        for (std::size_t i = 0; i < devices.size(); ++i) {
            const auto &node = devices[i];
            if (i > 0) {
                std::cout << ",";
            }
            std::cout << "{"
                      << "\"node_id\":" << ToJsonString(node.node_id) << ","
                      << "\"device_name\":" << ToJsonString(node.device_name) << ","
                      << "\"zt_ip\":" << ToJsonString(node.zt_ip) << ","
                      << "\"last_seen_epoch\":" << node.last_seen_epoch << "}";
        }
        PrintJsonArrayFooter();
        return 0;
    }
    std::cout << "devices=" << devices.size() << "\n";
    for (const auto &node : devices) {
        std::cout << "device " << node.node_id << " " << node.device_name << " " << node.zt_ip << " "
                  << node.last_seen_epoch << "\n";
    }
    return 0;
}

int RunTasks(int argc, char **argv, bool json_output) {
    if (argc < 4) {
        PrintUsage();
        return 1;
    }
    const std::string scope = argv[3];
    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    std::vector<sync::ScheduledTaskRecord> tasks;
    if (scope == "all") {
        tasks = runtime.control().ListTasks();
    } else {
        tasks = runtime.control().ListTasks(ParseTaskState(scope));
    }
    if (json_output) {
        PrintJsonArrayHeader("tasks");
        for (std::size_t i = 0; i < tasks.size(); ++i) {
            const auto &task = tasks[i];
            if (i > 0) {
                std::cout << ",";
            }
            std::cout << "{"
                      << "\"task_id\":" << ToJsonString(task.task_id) << ","
                      << "\"domain\":" << ToJsonString(JobDomainName(task.domain)) << ","
                      << "\"state\":" << ToJsonString(JobStateName(task.state)) << ","
                      << "\"related_id\":" << ToJsonString(task.related_id) << ","
                      << "\"source_node\":" << ToJsonString(task.source_node) << ","
                      << "\"target_node\":" << ToJsonString(task.target_node) << ","
                      << "\"created_at_epoch\":" << task.created_at_epoch << ","
                      << "\"retry_count\":" << task.retry_count << ","
                      << "\"detail\":" << ToJsonString(task.detail) << "}";
        }
        PrintJsonArrayFooter();
        return 0;
    }
    std::cout << "tasks=" << tasks.size() << "\n";
    for (const auto &task : tasks) {
        std::cout << "task " << task.task_id << " domain=" << static_cast<int>(task.domain)
                  << " state=" << static_cast<int>(task.state) << " retry=" << task.retry_count
                  << " detail=" << task.detail << "\n";
    }
    return 0;
}

int RunTaskRetry(int argc, char **argv) {
    if (argc < 4) {
        PrintUsage();
        return 1;
    }
    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const std::string detail = argc >= 5 ? argv[4] : "control plane retry requested";
    const auto status = runtime.control().RetryTask(argv[3], detail);
    if (!status.ok()) {
        std::cerr << "task retry failed: " << status.message << "\n";
        return 2;
    }
    std::cout << "task_retry_ok task=" << argv[3] << "\n";
    return 0;
}

int RunBackups(int argc, char **argv, bool json_output) {
    if (argc < 3) {
        PrintUsage();
        return 1;
    }
    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    std::optional<std::string> node_id;
    if (argc >= 4) {
        node_id = argv[3];
    }
    const auto jobs = runtime.control().ListBackupJobs(node_id);
    if (json_output) {
        PrintJsonArrayHeader("backup_jobs");
        for (std::size_t i = 0; i < jobs.size(); ++i) {
            const auto &job = jobs[i];
            if (i > 0) {
                std::cout << ",";
            }
            std::cout << "{"
                      << "\"job_id\":" << ToJsonString(job.job_id) << ","
                      << "\"node_id\":" << ToJsonString(job.node_id) << ","
                      << "\"source_path\":" << ToJsonString(job.source_path) << ","
                      << "\"destination_label\":" << ToJsonString(job.destination_label) << ","
                      << "\"created_at_epoch\":" << job.created_at_epoch << "}";
        }
        PrintJsonArrayFooter();
        return 0;
    }
    std::cout << "backup_jobs=" << jobs.size() << "\n";
    for (const auto &job : jobs) {
        std::cout << "backup_job " << job.job_id << " " << job.node_id << " " << job.source_path << "\n";
    }
    return 0;
}

int RunShared(int argc, char **argv, bool json_output) {
    if (argc < 5) {
        PrintUsage();
        return 1;
    }
    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const auto entries = runtime.control().ListSharedEntries(argv[3], argv[4]);
    if (json_output) {
        PrintJsonArrayHeader("shared_entries");
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const auto &entry = entries[i];
            if (i > 0) {
                std::cout << ",";
            }
            std::cout << "{"
                      << "\"entry_id\":" << ToJsonString(entry.entry_id) << ","
                      << "\"owner_node_id\":" << ToJsonString(entry.owner_node_id) << ","
                      << "\"file_id\":" << ToJsonString(entry.file_id) << ","
                      << "\"display_name\":" << ToJsonString(entry.display_name) << ","
                      << "\"note\":" << ToJsonString(entry.note) << ","
                      << "\"created_at_epoch\":" << entry.created_at_epoch << ","
                      << "\"expires_at_epoch\":" << entry.expires_at_epoch << ","
                      << "\"expired\":" << ToJsonBool(entry.expired) << ","
                      << "\"delivered\":" << ToJsonBool(entry.delivered) << ","
                      << "\"recipients\":[";
            for (std::size_t j = 0; j < entry.recipients.size(); ++j) {
                if (j > 0) {
                    std::cout << ",";
                }
                const auto &recipient = entry.recipients[j];
                std::cout << "{"
                          << "\"node_id\":" << ToJsonString(recipient.node_id) << ","
                          << "\"state\":" << ToJsonString(SharedRecipientStateName(recipient.state)) << ","
                          << "\"received_path\":" << ToJsonString(recipient.received_path) << ","
                          << "\"error_message\":" << ToJsonString(recipient.error_message) << ","
                          << "\"updated_at_epoch\":" << recipient.updated_at_epoch << "}";
            }
            std::cout << "]}";
        }
        PrintJsonArrayFooter();
        return 0;
    }
    std::cout << "shared_entries=" << entries.size() << "\n";
    for (const auto &entry : entries) {
        std::cout << "shared_entry " << entry.entry_id << " owner=" << entry.owner_node_id << " expired="
                  << entry.expired << " delivered=" << entry.delivered << " recipients=";
        for (std::size_t i = 0; i < entry.recipients.size(); ++i) {
            if (i > 0) {
                std::cout << ",";
            }
            const auto &recipient = entry.recipients[i];
            std::cout << recipient.node_id << ":" << SharedRecipientStateName(recipient.state);
        }
        std::cout << "\n";
    }
    return 0;
}

int RunShareRetry(int argc, char **argv) {
    if (argc < 6) {
        PrintUsage();
        return 1;
    }
    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const auto status = runtime.control().RetrySharedReceive(argv[3], argv[4], argv[5]);
    if (!status.ok()) {
        std::cerr << "share retry failed: " << status.message << "\n";
        return 2;
    }
    std::cout << "share_retry_ok entry=" << argv[3] << " target=" << argv[4] << "\n";
    return 0;
}

int RunRestores(int argc, char **argv, bool json_output) {
    if (argc < 3) {
        PrintUsage();
        return 1;
    }
    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const auto requests = runtime.control().ListRestoreRequests();
    if (json_output) {
        PrintJsonArrayHeader("restore_requests");
        for (std::size_t i = 0; i < requests.size(); ++i) {
            const auto &request = requests[i];
            if (i > 0) {
                std::cout << ",";
            }
            std::cout << "{"
                      << "\"request_id\":" << ToJsonString(request.request_id) << ","
                      << "\"file_id\":" << ToJsonString(request.file_id) << ","
                      << "\"source_node_id\":" << ToJsonString(request.source_node_id) << ","
                      << "\"target_node_id\":" << ToJsonString(request.target_node_id) << ","
                      << "\"destination_path\":" << ToJsonString(request.destination_path) << ","
                      << "\"created_at_epoch\":" << request.created_at_epoch << ","
                      << "\"state\":" << ToJsonString(RestoreRequestStateName(request.state)) << ","
                      << "\"completed_at_epoch\":" << request.completed_at_epoch << ","
                      << "\"error_message\":" << ToJsonString(request.error_message) << "}";
        }
        PrintJsonArrayFooter();
        return 0;
    }
    std::cout << "restore_requests=" << requests.size() << "\n";
    for (const auto &request : requests) {
        std::cout << "restore_request " << request.request_id << " " << request.file_id << " "
                  << request.target_node_id << " " << request.destination_path
                  << " state=" << RestoreRequestStateName(request.state)
                  << " error=" << request.error_message << "\n";
    }
    return 0;
}

int RunRestoreRetry(int argc, char **argv) {
    if (argc < 4) {
        PrintUsage();
        return 1;
    }
    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    const auto status = runtime.control().RetryRestore(argv[3]);
    if (!status.ok()) {
        std::cerr << "restore retry failed: " << status.message << "\n";
        return 2;
    }
    std::cout << "restore_retry_ok request=" << argv[3] << "\n";
    return 0;
}

int RunAudit(int argc, char **argv, bool json_output) {
    if (argc < 3) {
        PrintUsage();
        return 1;
    }
    sync::SqliteMetadataStore metadata_store(argv[2]);
    netdisk::SystemRuntime runtime(metadata_store);
    std::optional<std::string> category;
    if (argc >= 4) {
        category = argv[3];
    }
    const auto events = runtime.control().ListAuditEvents(category);
    if (json_output) {
        PrintJsonArrayHeader("audit_events");
        for (std::size_t i = 0; i < events.size(); ++i) {
            const auto &event = events[i];
            if (i > 0) {
                std::cout << ",";
            }
            std::cout << "{"
                      << "\"event_id\":" << ToJsonString(event.event_id) << ","
                      << "\"category\":" << ToJsonString(event.category) << ","
                      << "\"detail\":" << ToJsonString(event.detail) << ","
                      << "\"created_at_epoch\":" << event.created_at_epoch << "}";
        }
        PrintJsonArrayFooter();
        return 0;
    }
    std::cout << "audit_events=" << events.size() << "\n";
    for (const auto &event : events) {
        std::cout << "audit " << event.category << " " << event.detail << "\n";
    }
    return 0;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    const bool json_output = WantsJson(argc, argv);
    const int effective_argc = json_output ? argc - 1 : argc;
    const std::string command = argv[1];
    if (command == "serve") {
        return RunServe(effective_argc, argv);
    }
    if (command == "submit-backup") {
        return RunSubmitBackup(effective_argc, argv);
    }
    if (command == "submit-restore") {
        return RunSubmitRestore(effective_argc, argv);
    }
    if (command == "submit-share-receive") {
        return RunSubmitShareReceive(effective_argc, argv);
    }
    if (command == "retry-task-remote") {
        return RunRetryTaskRemote(effective_argc, argv);
    }
    if (command == "retry-restore-remote") {
        return RunRetryRestoreRemote(effective_argc, argv);
    }
    if (command == "retry-share-remote") {
        return RunRetryShareRemote(effective_argc, argv);
    }
    if (command == "summary") {
        return RunSummary(effective_argc, argv, json_output);
    }
    if (command == "devices") {
        return RunDevices(effective_argc, argv, json_output);
    }
    if (command == "tasks") {
        return RunTasks(effective_argc, argv, json_output);
    }
    if (command == "task-retry") {
        return RunTaskRetry(effective_argc, argv);
    }
    if (command == "backups") {
        return RunBackups(effective_argc, argv, json_output);
    }
    if (command == "shared") {
        return RunShared(effective_argc, argv, json_output);
    }
    if (command == "share-retry") {
        return RunShareRetry(effective_argc, argv);
    }
    if (command == "restores") {
        return RunRestores(effective_argc, argv, json_output);
    }
    if (command == "restore-retry") {
        return RunRestoreRetry(effective_argc, argv);
    }
    if (command == "audit") {
        return RunAudit(effective_argc, argv, json_output);
    }

    PrintUsage();
    return 1;
}
