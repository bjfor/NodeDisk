#include "netdisk/control_plane/device_agent_service.h"

namespace netdisk {

DeviceAgentService::DeviceAgentService(ControlNodeClient &client) : client_(client) {}

Status DeviceAgentService::Register(const sync::NodeInfo &node) const {
    return client_.RegisterNode(node);
}

Status DeviceAgentService::Heartbeat(const std::string &node_id, std::uint64_t epoch_seconds) const {
    return client_.SendHeartbeat(node_id, epoch_seconds);
}

Status DeviceAgentService::PollTasks(const std::string &node_id,
                                     std::vector<sync::ScheduledTaskRecord> *tasks) const {
    return client_.PullTasks(node_id, tasks);
}

Status DeviceAgentService::AcknowledgeTask(const std::string &task_id, const std::string &detail) const {
    return client_.AcknowledgeTask(task_id, detail);
}

Status DeviceAgentService::ReportTaskResult(const std::string &task_id,
                                            sync::JobState state,
                                            const std::string &detail) const {
    return client_.ReportTaskResult(task_id, state, detail);
}

Status DeviceAgentService::RunBootstrap(const sync::NodeInfo &node,
                                        std::uint64_t epoch_seconds,
                                        std::vector<sync::ScheduledTaskRecord> *tasks) const {
    auto status = Register(node);
    if (!status.ok()) {
        return status;
    }
    status = Heartbeat(node.node_id, epoch_seconds);
    if (!status.ok()) {
        return status;
    }
    return PollTasks(node.node_id, tasks);
}

}  // namespace netdisk
