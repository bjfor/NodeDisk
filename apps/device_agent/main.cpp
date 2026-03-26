#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "netdisk/backup/backup_service.h"
#include "netdisk/control_plane/device_agent_service.h"
#include "netdisk/control_plane/system_runtime.h"
#include "netdisk/metadata/metadata_store.h"
#include "netdisk/storage/storage_backend.h"

namespace {

std::atomic<bool> g_stop_requested{false};

std::uint64_t NowEpoch() {
    return static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

void PrintUsage() {
    std::cout
        << "usage:\n"
        << "  device_agent ping <host> <port>\n"
        << "  device_agent register <host> <port> <node_id> <device_name> <zt_ip>\n"
        << "  device_agent heartbeat <host> <port> <node_id>\n"
        << "  device_agent poll <host> <port> <node_id>\n"
        << "  device_agent bootstrap <host> <port> <node_id> <device_name> <zt_ip>\n"
        << "  device_agent run-once <local_db_path> <host> <port> <node_id> <device_name> <zt_ip> <backend_name> [backend_arg]\n"
        << "  device_agent serve <local_db_path> <host> <port> <node_id> <device_name> <zt_ip> <backend_name> [backend_arg] [poll_interval_seconds] [max_cycles]\n";
}

std::uint16_t ParsePort(const char *text) {
    return static_cast<std::uint16_t>(std::stoul(text));
}

int PrintTasks(const std::vector<sync::ScheduledTaskRecord> &tasks) {
    std::cout << "tasks=" << tasks.size() << "\n";
    for (const auto &task : tasks) {
        std::cout << "task " << task.task_id << " domain=" << static_cast<int>(task.domain)
                  << " state=" << static_cast<int>(task.state) << " source=" << task.source_node
                  << " target=" << task.target_node << " detail=" << task.detail << "\n";
    }
    return 0;
}

sync::BackupJob MakeBackupJob(const sync::ScheduledTaskRecord &task, const std::string &node_id) {
    sync::BackupJob job;
    job.job_id = task.task_id;
    job.node_id = node_id;
    job.source_path = task.detail;
    job.destination_label = "agent-exec";
    job.created_at_epoch = task.created_at_epoch;
    return job;
}

std::filesystem::path DefaultMaterializeRoot(const std::string &node_id) {
    return std::filesystem::temp_directory_path() / ("nodedisk_agent_materialized_" + node_id);
}

void HandleStopSignal(int) {
    g_stop_requested.store(true);
}

struct AgentRunResult {
    std::size_t completed = 0;
    std::size_t failed = 0;
};

AgentRunResult ExecuteTasks(sync::SqliteMetadataStore &local_store,
                            netdisk::SystemRuntime &local_runtime,
                            netdisk::ControlNodeClient &client,
                            netdisk::DeviceAgentService &agent,
                            const sync::NodeInfo &node,
                            const std::string &backend_name,
                            const std::string &backend_arg,
                            const std::vector<sync::ScheduledTaskRecord> &tasks) {
    AgentRunResult result;
    for (const auto &task : tasks) {
        auto status = agent.AcknowledgeTask(task.task_id, "accepted by device_agent");
        if (!status.ok()) {
            ++result.failed;
            continue;
        }

        if (task.domain == sync::JobDomain::kBackup) {
            sync::BackupPolicy policy;
            policy.policy_id = "agent-policy-" + node.node_id;
            policy.node_id = node.node_id;
            policy.source_path = task.detail;
            policy.created_at_epoch = NowEpoch();
            local_runtime.backup().ConfigurePolicy(policy);

            auto backend = sync::CreateStorageBackend(backend_name, backend_arg);
            const auto exec_status = local_runtime.backup().ExecuteBackup(MakeBackupJob(task, node.node_id), *backend);
            if (exec_status.ok()) {
                agent.ReportTaskResult(task.task_id, sync::JobState::kCompleted, task.detail);
                ++result.completed;
            } else {
                agent.ReportTaskResult(task.task_id, sync::JobState::kFailed, exec_status.message);
                ++result.failed;
            }
            continue;
        }

        if (task.domain == sync::JobDomain::kRestore) {
            sync::RestoreRequest request;
            sync::StoredObjectRecord stored;
            sync::FileRecord file;
            status = client.FetchRestoreBundle(task.task_id, &request, &stored, &file);
            if (!status.ok()) {
                agent.ReportTaskResult(task.task_id, sync::JobState::kFailed, status.message);
                ++result.failed;
                continue;
            }
            local_store.UpsertFile(file);
            local_store.UpsertStoredObject(stored);
            local_store.UpsertRestoreRequest(request);
            const auto exec_status = local_runtime.recovery().ExecuteRestore(request.request_id);
            if (exec_status.ok()) {
                agent.ReportTaskResult(task.task_id, sync::JobState::kCompleted, request.destination_path);
                ++result.completed;
            } else {
                agent.ReportTaskResult(task.task_id, sync::JobState::kFailed, exec_status.message);
                ++result.failed;
            }
            continue;
        }

        if (task.domain == sync::JobDomain::kTransfer) {
            sync::SharedLibraryEntry entry;
            status = client.FetchSharedEntry(task.related_id, &entry);
            if (!status.ok()) {
                agent.ReportTaskResult(task.task_id, sync::JobState::kFailed, status.message);
                ++result.failed;
                continue;
            }
            local_store.UpsertSharedLibraryEntry(entry);
            const auto exec_status =
                local_runtime.shared_library().ReceiveEntry(entry.entry_id, node.node_id, DefaultMaterializeRoot(node.node_id));
            if (exec_status.ok()) {
                const auto received_path =
                    (DefaultMaterializeRoot(node.node_id) / entry.display_name).lexically_normal().string();
                agent.ReportTaskResult(task.task_id, sync::JobState::kCompleted, received_path);
                ++result.completed;
            } else {
                agent.ReportTaskResult(task.task_id, sync::JobState::kFailed, exec_status.message);
                ++result.failed;
            }
            continue;
        }

        agent.ReportTaskResult(task.task_id,
                               sync::JobState::kFailed,
                               "unsupported task domain in device_agent skeleton");
        ++result.failed;
    }
    return result;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    const std::string command = argv[1];
    if (command == "ping") {
        if (argc < 4) {
            PrintUsage();
            return 1;
        }
        netdisk::ControlNodeClient client(argv[2], ParsePort(argv[3]));
        const auto status = client.Ping();
        if (!status.ok()) {
            std::cerr << "ping failed: " << status.message << "\n";
            return 1;
        }
        std::cout << "pong\n";
        return 0;
    }

    if (command == "register") {
        if (argc < 7) {
            PrintUsage();
            return 1;
        }
        netdisk::ControlNodeClient client(argv[2], ParsePort(argv[3]));
        netdisk::DeviceAgentService agent(client);
        sync::NodeInfo node{argv[4], argv[5], argv[6], NowEpoch()};
        const auto status = agent.Register(node);
        if (!status.ok()) {
            std::cerr << "register failed: " << status.message << "\n";
            return 1;
        }
        std::cout << "registered " << node.node_id << "\n";
        return 0;
    }

    if (command == "heartbeat") {
        if (argc < 5) {
            PrintUsage();
            return 1;
        }
        netdisk::ControlNodeClient client(argv[2], ParsePort(argv[3]));
        netdisk::DeviceAgentService agent(client);
        const auto status = agent.Heartbeat(argv[4], NowEpoch());
        if (!status.ok()) {
            std::cerr << "heartbeat failed: " << status.message << "\n";
            return 1;
        }
        std::cout << "heartbeat " << argv[4] << "\n";
        return 0;
    }

    if (command == "poll") {
        if (argc < 5) {
            PrintUsage();
            return 1;
        }
        netdisk::ControlNodeClient client(argv[2], ParsePort(argv[3]));
        netdisk::DeviceAgentService agent(client);
        std::vector<sync::ScheduledTaskRecord> tasks;
        const auto status = agent.PollTasks(argv[4], &tasks);
        if (!status.ok()) {
            std::cerr << "poll failed: " << status.message << "\n";
            return 1;
        }
        return PrintTasks(tasks);
    }

    if (command == "bootstrap") {
        if (argc < 7) {
            PrintUsage();
            return 1;
        }
        netdisk::ControlNodeClient client(argv[2], ParsePort(argv[3]));
        netdisk::DeviceAgentService agent(client);
        sync::NodeInfo node{argv[4], argv[5], argv[6], NowEpoch()};
        std::vector<sync::ScheduledTaskRecord> tasks;
        const auto status = agent.RunBootstrap(node, node.last_seen_epoch, &tasks);
        if (!status.ok()) {
            std::cerr << "bootstrap failed: " << status.message << "\n";
            return 1;
        }
        std::cout << "bootstrapped " << node.node_id << "\n";
        return PrintTasks(tasks);
    }

    if (command == "run-once") {
        if (argc < 9) {
            PrintUsage();
            return 1;
        }
        sync::SqliteMetadataStore local_store(argv[2]);
        netdisk::SystemRuntime local_runtime(local_store);
        netdisk::ControlNodeClient client(argv[3], ParsePort(argv[4]));
        netdisk::DeviceAgentService agent(client);
        sync::NodeInfo node{argv[5], argv[6], argv[7], NowEpoch()};
        const std::string backend_name = argv[8];
        const std::string backend_arg = argc >= 10 ? argv[9] : "";

        std::vector<sync::ScheduledTaskRecord> tasks;
        auto status = agent.RunBootstrap(node, node.last_seen_epoch, &tasks);
        if (!status.ok()) {
            std::cerr << "run-once bootstrap failed: " << status.message << "\n";
            return 1;
        }

        const auto result =
            ExecuteTasks(local_store, local_runtime, client, agent, node, backend_name, backend_arg, tasks);
        std::cout << "run_once_ok completed=" << result.completed << " failed=" << result.failed << "\n";
        return 0;
    }

    if (command == "serve") {
        if (argc < 9) {
            PrintUsage();
            return 1;
        }
        sync::SqliteMetadataStore local_store(argv[2]);
        netdisk::SystemRuntime local_runtime(local_store);
        netdisk::ControlNodeClient client(argv[3], ParsePort(argv[4]));
        netdisk::DeviceAgentService agent(client);
        sync::NodeInfo node{argv[5], argv[6], argv[7], NowEpoch()};
        const std::string backend_name = argv[8];
        const std::string backend_arg = argc >= 10 ? argv[9] : "";
        const std::uint64_t poll_interval_seconds = argc >= 11 ? std::stoull(argv[10]) : 5;
        const std::uint64_t max_cycles = argc >= 12 ? std::stoull(argv[11]) : 0;
        std::signal(SIGINT, HandleStopSignal);
        std::signal(SIGTERM, HandleStopSignal);

        std::uint64_t cycles = 0;
        std::size_t total_completed = 0;
        std::size_t total_failed = 0;
        std::uint64_t consecutive_bootstrap_failures = 0;
        while (!g_stop_requested.load() && (max_cycles == 0 || cycles < max_cycles)) {
            node.last_seen_epoch = NowEpoch();
            std::vector<sync::ScheduledTaskRecord> tasks;
            auto status = agent.RunBootstrap(node, node.last_seen_epoch, &tasks);
            if (!status.ok()) {
                std::cerr << "serve bootstrap failed: " << status.message << "\n";
                ++consecutive_bootstrap_failures;
            } else {
                consecutive_bootstrap_failures = 0;
                const auto result =
                    ExecuteTasks(local_store, local_runtime, client, agent, node, backend_name, backend_arg, tasks);
                total_completed += result.completed;
                total_failed += result.failed;
                std::cout << "cycle=" << (cycles + 1) << " tasks=" << tasks.size() << " completed=" << result.completed
                          << " failed=" << result.failed << "\n";
            }
            ++cycles;
            if (g_stop_requested.load() || (max_cycles != 0 && cycles >= max_cycles)) {
                break;
            }
            const std::uint64_t backoff_seconds =
                consecutive_bootstrap_failures == 0
                    ? poll_interval_seconds
                    : std::min<std::uint64_t>(poll_interval_seconds * (1ULL << std::min<std::uint64_t>(consecutive_bootstrap_failures, 4ULL)),
                                              60ULL);
            std::this_thread::sleep_for(std::chrono::seconds(backoff_seconds));
        }

        std::cout << "serve_ok cycles=" << cycles << " completed=" << total_completed
                  << " failed=" << total_failed << " stopped=" << (g_stop_requested.load() ? 1 : 0) << "\n";
        return 0;
    }

    PrintUsage();
    return 1;
}
