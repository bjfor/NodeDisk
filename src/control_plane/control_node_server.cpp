#include "netdisk/control_plane/control_node_server.h"

#include <algorithm>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <sstream>

extern "C" int close(int);

namespace netdisk {

namespace {

std::uint64_t NowEpoch() {
    return static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

std::string TrimCarriageReturn(std::string text) {
    if (!text.empty() && text.back() == '\r') {
        text.pop_back();
    }
    return text;
}

std::string JoinArgs(const std::vector<std::string> &args, std::size_t start_index) {
    std::ostringstream oss;
    for (std::size_t i = start_index; i < args.size(); ++i) {
        if (i > start_index) {
            oss << " ";
        }
        oss << args[i];
    }
    return oss.str();
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

std::string RestoreStateName(sync::RestoreRequestState state) {
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

sync::RestoreRequestState ParseRestoreState(const std::string &text) {
    if (text == "running") {
        return sync::RestoreRequestState::kRunning;
    }
    if (text == "completed") {
        return sync::RestoreRequestState::kCompleted;
    }
    if (text == "failed") {
        return sync::RestoreRequestState::kFailed;
    }
    return sync::RestoreRequestState::kQueued;
}

sync::SharedRecipientState ParseSharedRecipientState(const std::string &text) {
    if (text == "delivered") {
        return sync::SharedRecipientState::kDelivered;
    }
    if (text == "failed") {
        return sync::SharedRecipientState::kFailed;
    }
    return sync::SharedRecipientState::kPending;
}

std::vector<std::string> SplitByTab(const std::string &line) {
    std::vector<std::string> parts;
    std::istringstream iss(line);
    std::string part;
    while (std::getline(iss, part, '\t')) {
        parts.push_back(part);
    }
    return parts;
}

void CloseFd(int fd) {
    if (fd >= 0) {
        ::close(fd);
    }
}

}  // namespace

ControlNodeServer::ControlNodeServer(SystemRuntime &runtime) : runtime_(runtime) {}

Status ControlNodeServer::Serve(const std::string &host,
                                std::uint16_t port,
                                std::uint64_t offline_after_seconds) {
    const int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        return Status::Error(StatusCode::kInternalError, "failed to create listen socket");
    }

    int reuse = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        CloseFd(listen_fd);
        return Status::Error(StatusCode::kInvalidArgument, "invalid listen host");
    }

    if (::bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        CloseFd(listen_fd);
        return Status::Error(StatusCode::kInternalError, std::string("bind failed: ") + std::strerror(errno));
    }
    if (::listen(listen_fd, 16) != 0) {
        CloseFd(listen_fd);
        return Status::Error(StatusCode::kInternalError, std::string("listen failed: ") + std::strerror(errno));
    }

    while (true) {
        const int client_fd = ::accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            CloseFd(listen_fd);
            return Status::Error(StatusCode::kInternalError,
                                 std::string("accept failed: ") + std::strerror(errno));
        }

        const Status status = HandleClient(client_fd, offline_after_seconds);
        CloseFd(client_fd);
        if (!status.ok()) {
            CloseFd(listen_fd);
            return status;
        }
    }
}

Status ControlNodeServer::HandleClient(int client_fd, std::uint64_t offline_after_seconds) {
    std::string line;
    const auto read_status = ReadLine(client_fd, &line);
    if (!read_status.ok()) {
        return read_status;
    }
    const auto request = ParseRequest(line);
    return WriteLine(client_fd, HandleCommand(request, offline_after_seconds));
}

Status ControlNodeServer::ReadLine(int fd, std::string *line) const {
    if (line == nullptr) {
        return Status::Error(StatusCode::kInvalidArgument, "line output is null");
    }
    line->clear();
    char ch = '\0';
    while (true) {
        const ssize_t rc = ::recv(fd, &ch, 1, 0);
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            return Status::Error(StatusCode::kInternalError, "socket recv failed");
        }
        if (ch == '\n') {
            break;
        }
        line->push_back(ch);
    }
    *line = TrimCarriageReturn(*line);
    return Status::Ok();
}

Status ControlNodeServer::WriteLine(int fd, const std::string &line) const {
    std::string payload = line;
    if (payload.empty() || payload.back() != '\n') {
        payload.push_back('\n');
    }
    std::size_t written = 0;
    while (written < payload.size()) {
        const ssize_t rc = ::send(fd, payload.data() + written, payload.size() - written, 0);
        if (rc <= 0) {
            return Status::Error(StatusCode::kInternalError, "socket send failed");
        }
        written += static_cast<std::size_t>(rc);
    }
    return Status::Ok();
}

ControlNodeServer::ParsedRequest ControlNodeServer::ParseRequest(const std::string &line) const {
    ParsedRequest request;
    std::istringstream iss(line);
    iss >> request.command;
    std::string token;
    while (iss >> token) {
        request.args.push_back(token);
    }
    return request;
}

std::string ControlNodeServer::HandleCommand(const ParsedRequest &request,
                                             std::uint64_t offline_after_seconds) {
    if (request.command == "PING") {
        return "OK PONG";
    }
    if (request.command == "REGISTER") {
        return HandleRegister(request);
    }
    if (request.command == "HEARTBEAT") {
        return HandleHeartbeat(request);
    }
    if (request.command == "SUMMARY") {
        return HandleSummary(offline_after_seconds);
    }
    if (request.command == "PULL_TASKS") {
        return HandlePullTasks(request);
    }
    if (request.command == "SUBMIT_BACKUP") {
        if (request.args.size() < 3) {
            return "ERR submit_backup requires job_id node_id source_path";
        }
        sync::NodeInfo node;
        node.node_id = request.args[1];
        node.device_name = request.args[1];
        node.zt_ip = "pending";
        node.last_seen_epoch = NowEpoch();
        runtime_.devices().RegisterDevice(node);

        sync::BackupPolicy policy;
        policy.policy_id = "remote-policy-" + request.args[1];
        policy.node_id = request.args[1];
        policy.source_path = JoinArgs(request.args, 2);
        policy.schedule_expression = "remote-submit";
        policy.created_at_epoch = NowEpoch();
        runtime_.backup().ConfigurePolicy(policy);

        sync::BackupJob job;
        job.job_id = request.args[0];
        job.node_id = request.args[1];
        job.source_path = JoinArgs(request.args, 2);
        job.destination_label = "remote-submitted";
        job.created_at_epoch = NowEpoch();
        const auto status = runtime_.backup().StartBackup(job);
        return status.ok() ? "OK BACKUP_SUBMITTED" : "ERR " + status.message;
    }
    if (request.command == "SUBMIT_RESTORE") {
        if (request.args.size() < 5) {
            return "ERR submit_restore requires request_id file_id source_node target_node destination_path";
        }
        sync::RestoreRequest restore;
        restore.request_id = request.args[0];
        restore.file_id = request.args[1];
        restore.source_node_id = request.args[2];
        restore.target_node_id = request.args[3];
        restore.destination_path = JoinArgs(request.args, 4);
        restore.created_at_epoch = NowEpoch();
        const auto status = runtime_.recovery().SubmitRestore(restore);
        return status.ok() ? "OK RESTORE_SUBMITTED" : "ERR " + status.message;
    }
    if (request.command == "SUBMIT_SHARE_RECEIVE") {
        if (request.args.size() < 2) {
            return "ERR submit_share_receive requires entry_id target_node_id";
        }
        const auto status = runtime_.shared_library().CreateReceiveTask(request.args[0], request.args[1]);
        return status.ok() ? "OK SHARE_SUBMITTED" : "ERR " + status.message;
    }
    if (request.command == "ACK_TASK") {
        if (request.args.empty()) {
            return "ERR ack_task requires task_id";
        }
        const std::string detail = request.args.size() >= 2 ? JoinArgs(request.args, 1) : "task acknowledged by agent";
        const auto status = runtime_.tasks().UpdateTaskState(request.args[0], sync::JobState::kRunning, detail);
        return status.ok() ? "OK ACKED" : "ERR " + status.message;
    }
    if (request.command == "RETRY_TASK") {
        if (request.args.empty()) {
            return "ERR retry_task requires task_id";
        }
        const std::string detail = request.args.size() >= 2 ? JoinArgs(request.args, 1) : "task requeued via control service";
        const auto status = runtime_.tasks().RetryTask(request.args[0], detail);
        return status.ok() ? "OK TASK_RETRIED" : "ERR " + status.message;
    }
    if (request.command == "RETRY_RESTORE") {
        if (request.args.empty()) {
            return "ERR retry_restore requires request_id";
        }
        const auto status = runtime_.recovery().RetryRestore(request.args[0]);
        return status.ok() ? "OK RESTORE_RETRIED" : "ERR " + status.message;
    }
    if (request.command == "RETRY_SHARE") {
        if (request.args.size() < 2) {
            return "ERR retry_share requires entry_id target_node_id";
        }
        auto entry = runtime_.shared_library().FindEntry(request.args[0]);
        if (!entry.has_value()) {
            return "ERR shared entry not found";
        }
        const auto status = runtime_.shared_library().CreateReceiveTask(entry->entry_id, request.args[1]);
        return status.ok() ? "OK SHARE_RETRIED" : "ERR " + status.message;
    }
    if (request.command == "REPORT_TASK") {
        if (request.args.size() < 2) {
            return "ERR report_task requires task_id state [detail]";
        }
        sync::JobState state = sync::JobState::kFailed;
        if (request.args[1] == "running") {
            state = sync::JobState::kRunning;
        } else if (request.args[1] == "completed") {
            state = sync::JobState::kCompleted;
        } else if (request.args[1] == "queued") {
            state = sync::JobState::kQueued;
        }
        const std::string detail = request.args.size() >= 3 ? JoinArgs(request.args, 2) : "task result reported";
        const auto existing_task = runtime_.tasks().FindTask(request.args[0]);
        const auto status = runtime_.tasks().UpdateTaskState(request.args[0], state, detail);
        if (!status.ok()) {
            return "ERR " + status.message;
        }
        if (existing_task.has_value()) {
            if (existing_task->domain == sync::JobDomain::kRestore) {
                auto restore = runtime_.metadata().GetRestoreRequest(existing_task->task_id);
                if (restore.has_value()) {
                    restore->state = state == sync::JobState::kCompleted
                                         ? sync::RestoreRequestState::kCompleted
                                         : state == sync::JobState::kRunning ? sync::RestoreRequestState::kRunning
                                                                             : sync::RestoreRequestState::kFailed;
                    restore->completed_at_epoch = state == sync::JobState::kCompleted || state == sync::JobState::kFailed
                                                      ? NowEpoch()
                                                      : 0;
                    restore->error_message = state == sync::JobState::kFailed ? detail : "";
                    runtime_.metadata().UpsertRestoreRequest(*restore);
                }
            } else if (existing_task->domain == sync::JobDomain::kTransfer) {
                auto entry = runtime_.metadata().GetSharedLibraryEntry(existing_task->related_id);
                if (entry.has_value()) {
                    bool updated = false;
                    for (auto &recipient : entry->recipients) {
                        if (recipient.node_id != existing_task->target_node) {
                            continue;
                        }
                        recipient.state = state == sync::JobState::kCompleted
                                              ? sync::SharedRecipientState::kDelivered
                                              : state == sync::JobState::kRunning ? sync::SharedRecipientState::kPending
                                                                                  : sync::SharedRecipientState::kFailed;
                        recipient.updated_at_epoch = NowEpoch();
                        if (state == sync::JobState::kCompleted) {
                            recipient.received_path = detail;
                            recipient.error_message.clear();
                        } else if (state == sync::JobState::kFailed) {
                            recipient.error_message = detail;
                        }
                        updated = true;
                        break;
                    }
                    if (!updated) {
                        sync::SharedLibraryRecipient recipient;
                        recipient.entry_id = entry->entry_id;
                        recipient.node_id = existing_task->target_node;
                        recipient.state = ParseSharedRecipientState(state == sync::JobState::kCompleted
                                                                       ? "delivered"
                                                                       : state == sync::JobState::kFailed ? "failed"
                                                                                                          : "pending");
                        recipient.updated_at_epoch = NowEpoch();
                        if (state == sync::JobState::kCompleted) {
                            recipient.received_path = detail;
                        } else if (state == sync::JobState::kFailed) {
                            recipient.error_message = detail;
                        }
                        entry->recipients.push_back(recipient);
                    }
                    entry->delivered = std::all_of(entry->recipients.begin(),
                                                   entry->recipients.end(),
                                                   [](const sync::SharedLibraryRecipient &recipient) {
                                                       return recipient.state == sync::SharedRecipientState::kDelivered;
                                                   });
                    runtime_.metadata().UpsertSharedLibraryEntry(*entry);
                }
            }
        }
        return "OK REPORTED";
    }
    if (request.command == "GET_RESTORE") {
        if (request.args.empty()) {
            return "ERR get_restore requires request_id";
        }
        const auto restore = runtime_.recovery().FindRestoreRequest(request.args[0]);
        if (!restore.has_value()) {
            return "ERR restore request not found";
        }
        const auto stored = runtime_.file_index().FindStoredObject(restore->file_id);
        if (!stored.has_value()) {
            return "ERR stored object not found";
        }
        const auto file = runtime_.file_index().FindFile(restore->file_id);
        if (!file.has_value()) {
            return "ERR file metadata not found";
        }
        std::ostringstream oss;
        oss << "OK RESTORE\t"
            << restore->request_id << "\t" << restore->file_id << "\t" << restore->source_node_id << "\t"
            << restore->target_node_id << "\t" << restore->destination_path << "\t"
            << RestoreStateName(restore->state) << "\t" << restore->completed_at_epoch << "\t"
            << restore->error_message << "\t" << stored->backend_name << "\t" << stored->storage_key << "\t"
            << stored->access_url << "\t" << stored->source_path << "\t" << stored->stored_at_epoch << "\t"
            << file->relative_path << "\t" << file->absolute_path << "\t" << file->size << "\t"
            << file->modified_time_epoch << "\t" << file->mode << "\t" << file->root_hash;
        return oss.str();
    }
    if (request.command == "GET_SHARED_ENTRY") {
        if (request.args.empty()) {
            return "ERR get_shared_entry requires entry_id";
        }
        const auto entry = runtime_.shared_library().FindEntry(request.args[0]);
        if (!entry.has_value()) {
            return "ERR shared entry not found";
        }
        std::ostringstream oss;
        oss << "OK SHARED\t" << entry->entry_id << "\t" << entry->owner_node_id << "\t" << entry->source_path << "\t"
            << entry->file_id << "\t" << entry->display_name << "\t" << entry->note << "\t"
            << entry->created_at_epoch << "\t" << entry->expires_at_epoch << "\t" << (entry->expired ? 1 : 0);
        return oss.str();
    }
    return "ERR unsupported command";
}

std::string ControlNodeServer::HandleRegister(const ParsedRequest &request) const {
    if (request.args.size() < 4) {
        return "ERR register requires node_id device_name zt_ip epoch";
    }
    sync::NodeInfo node;
    node.node_id = request.args[0];
    node.device_name = request.args[1];
    node.zt_ip = request.args[2];
    node.last_seen_epoch = static_cast<std::uint64_t>(std::stoull(request.args[3]));
    const auto status = runtime_.devices().RegisterDevice(node);
    return status.ok() ? "OK REGISTERED" : "ERR " + status.message;
}

std::string ControlNodeServer::HandleHeartbeat(const ParsedRequest &request) const {
    if (request.args.size() < 2) {
        return "ERR heartbeat requires node_id epoch";
    }
    const auto status =
        runtime_.devices().RecordHeartbeat(request.args[0], static_cast<std::uint64_t>(std::stoull(request.args[1])));
    return status.ok() ? "OK HEARTBEAT" : "ERR " + status.message;
}

std::string ControlNodeServer::HandleSummary(std::uint64_t offline_after_seconds) const {
    const auto summary = runtime_.control().BuildSummary(NowEpoch(), offline_after_seconds);
    std::ostringstream oss;
    oss << "OK SUMMARY "
        << "devices=" << summary.devices << " "
        << "online_devices=" << summary.online_devices << " "
        << "queued_tasks=" << summary.queued_tasks << " "
        << "failed_tasks=" << summary.failed_tasks << " "
        << "shared_entries=" << summary.shared_entries << " "
        << "restore_requests=" << summary.restore_requests << " "
        << "audit_events=" << summary.audit_events;
    return oss.str();
}

std::string ControlNodeServer::HandlePullTasks(const ParsedRequest &request) const {
    if (request.args.empty()) {
        return "ERR pull_tasks requires node_id";
    }

    std::vector<sync::ScheduledTaskRecord> queued;
    for (const auto &task : runtime_.tasks().ListTasksForNode(request.args[0])) {
        if (task.state == sync::JobState::kQueued) {
            queued.push_back(task);
        }
    }

    std::ostringstream oss;
    oss << "OK TASKS " << queued.size();
    for (const auto &task : queued) {
        std::string detail = task.detail;
        for (char &ch : detail) {
            if (ch == '\t' || ch == '\n' || ch == '\r') {
                ch = ' ';
            }
        }
        oss << "\nTASK\t" << task.task_id << "\t" << JobDomainName(task.domain) << "\t" << JobStateName(task.state)
            << "\t" << task.related_id << "\t" << task.source_node << "\t" << task.target_node << "\t"
            << task.created_at_epoch << "\t" << task.retry_count << "\t" << detail;
    }
    oss << "\nEND";
    return oss.str();
}

ControlNodeClient::ControlNodeClient(std::string host, std::uint16_t port) : host_(std::move(host)), port_(port) {}

Status ControlNodeClient::Ping() const {
    std::vector<std::string> lines;
    const auto status = SendCommand("PING", &lines);
    if (!status.ok()) {
        return status;
    }
    return (!lines.empty() && lines.front() == "OK PONG")
               ? Status::Ok()
               : Status::Error(StatusCode::kInternalError, "unexpected ping response");
}

Status ControlNodeClient::RegisterNode(const sync::NodeInfo &node) const {
    std::ostringstream oss;
    oss << "REGISTER " << node.node_id << " " << node.device_name << " " << node.zt_ip << " " << node.last_seen_epoch;
    std::vector<std::string> lines;
    const auto status = SendCommand(oss.str(), &lines);
    if (!status.ok()) {
        return status;
    }
    return (!lines.empty() && lines.front() == "OK REGISTERED")
               ? Status::Ok()
               : Status::Error(StatusCode::kInternalError, lines.empty() ? "register failed" : lines.front());
}

Status ControlNodeClient::SendHeartbeat(const std::string &node_id, std::uint64_t epoch_seconds) const {
    std::ostringstream oss;
    oss << "HEARTBEAT " << node_id << " " << epoch_seconds;
    std::vector<std::string> lines;
    const auto status = SendCommand(oss.str(), &lines);
    if (!status.ok()) {
        return status;
    }
    return (!lines.empty() && lines.front() == "OK HEARTBEAT")
               ? Status::Ok()
               : Status::Error(StatusCode::kInternalError, lines.empty() ? "heartbeat failed" : lines.front());
}

Status ControlNodeClient::FetchSummary(std::uint64_t offline_after_seconds, std::string *summary_line) const {
    std::ostringstream oss;
    oss << "SUMMARY " << offline_after_seconds;
    std::vector<std::string> lines;
    const auto status = SendCommand(oss.str(), &lines);
    if (!status.ok()) {
        return status;
    }
    if (lines.empty()) {
        return Status::Error(StatusCode::kInternalError, "missing summary response");
    }
    if (summary_line != nullptr) {
        *summary_line = lines.front();
    }
    return Status::Ok();
}

Status ControlNodeClient::PullTasks(const std::string &node_id, std::vector<sync::ScheduledTaskRecord> *tasks) const {
    if (tasks == nullptr) {
        return Status::Error(StatusCode::kInvalidArgument, "tasks output is null");
    }
    tasks->clear();

    std::vector<std::string> lines;
    const auto status = SendCommand("PULL_TASKS " + node_id, &lines);
    if (!status.ok()) {
        return status;
    }
    if (lines.empty() || lines.front().rfind("OK TASKS ", 0) != 0) {
        return Status::Error(StatusCode::kInternalError, lines.empty() ? "pull tasks failed" : lines.front());
    }

    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (lines[i] == "END") {
            break;
        }
        if (lines[i].rfind("TASK\t", 0) != 0) {
            continue;
        }
        std::vector<std::string> parts;
        std::istringstream iss(lines[i]);
        std::string part;
        while (std::getline(iss, part, '\t')) {
            parts.push_back(part);
        }
        if (parts.size() < 9) {
            continue;
        }

        sync::ScheduledTaskRecord task;
        task.task_id = parts[1];
        task.domain = parts[2] == "backup" ? sync::JobDomain::kBackup
                    : parts[2] == "transfer" ? sync::JobDomain::kTransfer
                                             : sync::JobDomain::kRestore;
        task.state = parts[3] == "queued"     ? sync::JobState::kQueued
                   : parts[3] == "running"    ? sync::JobState::kRunning
                   : parts[3] == "completed"  ? sync::JobState::kCompleted
                                              : sync::JobState::kFailed;
        task.related_id = parts[4];
        task.source_node = parts[5];
        task.target_node = parts[6];
        task.created_at_epoch = static_cast<std::uint64_t>(std::stoull(parts[7]));
        task.retry_count = static_cast<std::uint32_t>(std::stoul(parts[8]));
        if (parts.size() >= 10) {
            task.detail = parts[9];
        }
        tasks->push_back(task);
    }
    return Status::Ok();
}

Status ControlNodeClient::SubmitBackupJob(const sync::BackupJob &job) const {
    std::vector<std::string> lines;
    const auto status = SendCommand("SUBMIT_BACKUP " + job.job_id + " " + job.node_id + " " + job.source_path, &lines);
    if (!status.ok()) {
        return status;
    }
    return (!lines.empty() && lines.front() == "OK BACKUP_SUBMITTED")
               ? Status::Ok()
               : Status::Error(StatusCode::kInternalError, lines.empty() ? "submit backup failed" : lines.front());
}

Status ControlNodeClient::SubmitRestoreRequest(const sync::RestoreRequest &request) const {
    std::vector<std::string> lines;
    const auto status = SendCommand("SUBMIT_RESTORE " + request.request_id + " " + request.file_id + " " +
                                        request.source_node_id + " " + request.target_node_id + " " +
                                        request.destination_path,
                                    &lines);
    if (!status.ok()) {
        return status;
    }
    return (!lines.empty() && lines.front() == "OK RESTORE_SUBMITTED")
               ? Status::Ok()
               : Status::Error(StatusCode::kInternalError, lines.empty() ? "submit restore failed" : lines.front());
}

Status ControlNodeClient::SubmitShareReceive(const std::string &entry_id, const std::string &target_node_id) const {
    std::vector<std::string> lines;
    const auto status = SendCommand("SUBMIT_SHARE_RECEIVE " + entry_id + " " + target_node_id, &lines);
    if (!status.ok()) {
        return status;
    }
    return (!lines.empty() && lines.front() == "OK SHARE_SUBMITTED")
               ? Status::Ok()
               : Status::Error(StatusCode::kInternalError, lines.empty() ? "submit share receive failed" : lines.front());
}

Status ControlNodeClient::AcknowledgeTask(const std::string &task_id, const std::string &detail) const {
    std::vector<std::string> lines;
    const auto status = SendCommand("ACK_TASK " + task_id + " " + detail, &lines);
    if (!status.ok()) {
        return status;
    }
    return (!lines.empty() && lines.front() == "OK ACKED")
               ? Status::Ok()
               : Status::Error(StatusCode::kInternalError, lines.empty() ? "ack failed" : lines.front());
}

Status ControlNodeClient::ReportTaskResult(const std::string &task_id,
                                           sync::JobState state,
                                           const std::string &detail) const {
    std::string state_name = "failed";
    if (state == sync::JobState::kQueued) {
        state_name = "queued";
    } else if (state == sync::JobState::kRunning) {
        state_name = "running";
    } else if (state == sync::JobState::kCompleted) {
        state_name = "completed";
    }
    std::vector<std::string> lines;
    const auto status = SendCommand("REPORT_TASK " + task_id + " " + state_name + " " + detail, &lines);
    if (!status.ok()) {
        return status;
    }
    return (!lines.empty() && lines.front() == "OK REPORTED")
               ? Status::Ok()
               : Status::Error(StatusCode::kInternalError, lines.empty() ? "report failed" : lines.front());
}

Status ControlNodeClient::RetryTask(const std::string &task_id, const std::string &detail) const {
    std::vector<std::string> lines;
    const auto status = SendCommand("RETRY_TASK " + task_id + " " + detail, &lines);
    if (!status.ok()) {
        return status;
    }
    return (!lines.empty() && lines.front() == "OK TASK_RETRIED")
               ? Status::Ok()
               : Status::Error(StatusCode::kInternalError, lines.empty() ? "retry task failed" : lines.front());
}

Status ControlNodeClient::RetryRestore(const std::string &request_id) const {
    std::vector<std::string> lines;
    const auto status = SendCommand("RETRY_RESTORE " + request_id, &lines);
    if (!status.ok()) {
        return status;
    }
    return (!lines.empty() && lines.front() == "OK RESTORE_RETRIED")
               ? Status::Ok()
               : Status::Error(StatusCode::kInternalError, lines.empty() ? "retry restore failed" : lines.front());
}

Status ControlNodeClient::RetryShareReceive(const std::string &entry_id, const std::string &target_node_id) const {
    std::vector<std::string> lines;
    const auto status = SendCommand("RETRY_SHARE " + entry_id + " " + target_node_id, &lines);
    if (!status.ok()) {
        return status;
    }
    return (!lines.empty() && lines.front() == "OK SHARE_RETRIED")
               ? Status::Ok()
               : Status::Error(StatusCode::kInternalError, lines.empty() ? "retry share failed" : lines.front());
}

Status ControlNodeClient::FetchRestoreBundle(const std::string &request_id,
                                             sync::RestoreRequest *request,
                                             sync::StoredObjectRecord *stored,
                                             sync::FileRecord *file) const {
    if (request == nullptr || stored == nullptr || file == nullptr) {
        return Status::Error(StatusCode::kInvalidArgument, "restore bundle output is null");
    }
    std::vector<std::string> lines;
    const auto status = SendCommand("GET_RESTORE " + request_id, &lines);
    if (!status.ok()) {
        return status;
    }
    if (lines.empty() || lines.front().rfind("OK RESTORE\t", 0) != 0) {
        return Status::Error(StatusCode::kInternalError, lines.empty() ? "missing restore bundle" : lines.front());
    }

    const auto parts = SplitByTab(lines.front());
    if (parts.size() < 20) {
        return Status::Error(StatusCode::kInternalError, "restore bundle is incomplete");
    }

    request->request_id = parts[1];
    request->file_id = parts[2];
    request->source_node_id = parts[3];
    request->target_node_id = parts[4];
    request->destination_path = parts[5];
    request->state = ParseRestoreState(parts[6]);
    request->completed_at_epoch = static_cast<std::uint64_t>(std::stoull(parts[7]));
    request->error_message = parts[8];

    stored->file_id = parts[2];
    stored->backend_name = parts[9];
    stored->storage_key = parts[10];
    stored->access_url = parts[11];
    stored->source_path = parts[12];
    stored->stored_at_epoch = static_cast<std::uint64_t>(std::stoull(parts[13]));

    file->file_id = parts[2];
    file->relative_path = parts[14];
    file->absolute_path = parts[15];
    file->size = static_cast<std::uint64_t>(std::stoull(parts[16]));
    file->modified_time_epoch = static_cast<std::uint64_t>(std::stoull(parts[17]));
    file->mode = static_cast<std::uint32_t>(std::stoul(parts[18]));
    file->root_hash = parts[19];
    file->status = sync::FileStatus::kReady;
    return Status::Ok();
}

Status ControlNodeClient::FetchSharedEntry(const std::string &entry_id, sync::SharedLibraryEntry *entry) const {
    if (entry == nullptr) {
        return Status::Error(StatusCode::kInvalidArgument, "shared entry output is null");
    }
    std::vector<std::string> lines;
    const auto status = SendCommand("GET_SHARED_ENTRY " + entry_id, &lines);
    if (!status.ok()) {
        return status;
    }
    if (lines.empty() || lines.front().rfind("OK SHARED\t", 0) != 0) {
        return Status::Error(StatusCode::kInternalError, lines.empty() ? "missing shared entry" : lines.front());
    }

    const auto parts = SplitByTab(lines.front());
    if (parts.size() < 10) {
        return Status::Error(StatusCode::kInternalError, "shared entry is incomplete");
    }

    entry->entry_id = parts[1];
    entry->owner_node_id = parts[2];
    entry->source_path = parts[3];
    entry->file_id = parts[4];
    entry->display_name = parts[5];
    entry->note = parts[6];
    entry->created_at_epoch = static_cast<std::uint64_t>(std::stoull(parts[7]));
    entry->expires_at_epoch = static_cast<std::uint64_t>(std::stoull(parts[8]));
    entry->expired = parts[9] == "1";
    return Status::Ok();
}

Status ControlNodeClient::SendCommand(const std::string &command, std::vector<std::string> *response_lines) const {
    if (response_lines == nullptr) {
        return Status::Error(StatusCode::kInvalidArgument, "response_lines is null");
    }
    response_lines->clear();

    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return Status::Error(StatusCode::kInternalError, "failed to create client socket");
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
        CloseFd(fd);
        return Status::Error(StatusCode::kInvalidArgument, "invalid control host");
    }
    if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        CloseFd(fd);
        return Status::Error(StatusCode::kInternalError, std::string("connect failed: ") + std::strerror(errno));
    }

    const std::string payload = command + "\n";
    std::size_t written = 0;
    while (written < payload.size()) {
        const ssize_t rc = ::send(fd, payload.data() + written, payload.size() - written, 0);
        if (rc <= 0) {
            CloseFd(fd);
            return Status::Error(StatusCode::kInternalError, "socket send failed");
        }
        written += static_cast<std::size_t>(rc);
    }

    std::string all;
    char buffer[1024];
    while (true) {
        const ssize_t rc = ::recv(fd, buffer, sizeof(buffer), 0);
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            CloseFd(fd);
            return Status::Error(StatusCode::kInternalError, "socket recv failed");
        }
        all.append(buffer, static_cast<std::size_t>(rc));
    }
    CloseFd(fd);

    std::istringstream iss(all);
    std::string line;
    while (std::getline(iss, line)) {
        response_lines->push_back(TrimCarriageReturn(line));
    }
    if (!response_lines->empty() && response_lines->front().rfind("ERR ", 0) == 0) {
        return Status::Error(StatusCode::kInternalError, response_lines->front().substr(4));
    }
    return Status::Ok();
}

}  // namespace netdisk
