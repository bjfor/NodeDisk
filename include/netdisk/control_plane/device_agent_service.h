#ifndef NETDISK_CONTROL_PLANE_DEVICE_AGENT_SERVICE_H
#define NETDISK_CONTROL_PLANE_DEVICE_AGENT_SERVICE_H

#include <cstdint>
#include <vector>

#include "netdisk/common/status.h"
#include "netdisk/control_plane/control_node_server.h"

namespace netdisk {

class DeviceAgentService {
public:
    explicit DeviceAgentService(ControlNodeClient &client);

    Status Register(const sync::NodeInfo &node) const;
    Status Heartbeat(const std::string &node_id, std::uint64_t epoch_seconds) const;
    Status PollTasks(const std::string &node_id, std::vector<sync::ScheduledTaskRecord> *tasks) const;
    Status AcknowledgeTask(const std::string &task_id, const std::string &detail) const;
    Status ReportTaskResult(const std::string &task_id,
                            sync::JobState state,
                            const std::string &detail) const;
    Status RunBootstrap(const sync::NodeInfo &node,
                        std::uint64_t epoch_seconds,
                        std::vector<sync::ScheduledTaskRecord> *tasks) const;

private:
    ControlNodeClient &client_;
};

}  // namespace netdisk

#endif
