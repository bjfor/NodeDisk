#ifndef NETDISK_CONTROL_PLANE_CONTROL_NODE_SERVER_H
#define NETDISK_CONTROL_PLANE_CONTROL_NODE_SERVER_H

#include <cstdint>
#include <string>
#include <vector>

#include "netdisk/common/status.h"
#include "netdisk/control_plane/system_runtime.h"
#include "netdisk/core/types.h"

namespace netdisk {

class ControlNodeServer {
public:
    explicit ControlNodeServer(SystemRuntime &runtime);

    Status Serve(const std::string &host,
                 std::uint16_t port,
                 std::uint64_t offline_after_seconds);

private:
    struct ParsedRequest {
        std::string command;
        std::vector<std::string> args;
    };

    SystemRuntime &runtime_;

    Status HandleClient(int client_fd, std::uint64_t offline_after_seconds);
    Status ReadLine(int fd, std::string *line) const;
    Status WriteLine(int fd, const std::string &line) const;
    ParsedRequest ParseRequest(const std::string &line) const;
    std::string HandleCommand(const ParsedRequest &request,
                              std::uint64_t offline_after_seconds);
    std::string HandleRegister(const ParsedRequest &request) const;
    std::string HandleHeartbeat(const ParsedRequest &request) const;
    std::string HandleSummary(std::uint64_t offline_after_seconds) const;
    std::string HandlePullTasks(const ParsedRequest &request) const;
};

class ControlNodeClient {
public:
    ControlNodeClient(std::string host, std::uint16_t port);

    Status Ping() const;
    Status RegisterNode(const sync::NodeInfo &node) const;
    Status SendHeartbeat(const std::string &node_id, std::uint64_t epoch_seconds) const;
    Status FetchSummary(std::uint64_t offline_after_seconds, std::string *summary_line) const;
    Status PullTasks(const std::string &node_id, std::vector<sync::ScheduledTaskRecord> *tasks) const;
    Status SubmitBackupJob(const sync::BackupJob &job) const;
    Status SubmitRestoreRequest(const sync::RestoreRequest &request) const;
    Status SubmitShareReceive(const std::string &entry_id, const std::string &target_node_id) const;
    Status AcknowledgeTask(const std::string &task_id, const std::string &detail) const;
    Status ReportTaskResult(const std::string &task_id,
                            sync::JobState state,
                            const std::string &detail) const;
    Status RetryTask(const std::string &task_id, const std::string &detail) const;
    Status RetryRestore(const std::string &request_id) const;
    Status RetryShareReceive(const std::string &entry_id, const std::string &target_node_id) const;
    Status FetchRestoreBundle(const std::string &request_id,
                              sync::RestoreRequest *request,
                              sync::StoredObjectRecord *stored,
                              sync::FileRecord *file) const;
    Status FetchSharedEntry(const std::string &entry_id, sync::SharedLibraryEntry *entry) const;

private:
    std::string host_;
    std::uint16_t port_;

    Status SendCommand(const std::string &command, std::vector<std::string> *response_lines) const;
};

}  // namespace netdisk

#endif
