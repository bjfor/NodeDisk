#ifndef NETDISK_CONTROL_PLANE_TASK_SCHEDULER_H
#define NETDISK_CONTROL_PLANE_TASK_SCHEDULER_H

#include <optional>
#include <vector>

#include "netdisk/common/status.h"
#include "netdisk/core/types.h"

namespace netdisk {

class TaskScheduler {
public:
    Status ScheduleTask(const sync::ScheduledTaskRecord &task);
    Status UpdateTaskState(const std::string &task_id, sync::JobState state, std::string detail);
    std::optional<sync::ScheduledTaskRecord> FindTask(const std::string &task_id) const;
    std::vector<sync::ScheduledTaskRecord> ListTasks() const;
    std::vector<sync::ScheduledTaskRecord> ListTasksByDomain(sync::JobDomain domain) const;
    std::vector<sync::ScheduledTaskRecord> ListTasksForNode(const std::string &node_id) const;

private:
    std::vector<sync::ScheduledTaskRecord> tasks_;
};

}  // namespace netdisk

#endif
