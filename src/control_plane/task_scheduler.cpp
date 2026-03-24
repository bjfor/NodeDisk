#include "netdisk/control_plane/task_scheduler.h"

namespace netdisk {

Status TaskScheduler::ScheduleTask(const sync::ScheduledTaskRecord &task) {
    for (auto &current : tasks_) {
        if (current.task_id == task.task_id) {
            current = task;
            return Status::Ok();
        }
    }
    tasks_.push_back(task);
    return Status::Ok();
}

Status TaskScheduler::UpdateTaskState(const std::string &task_id, sync::JobState state, std::string detail) {
    for (auto &task : tasks_) {
        if (task.task_id == task_id) {
            task.state = state;
            task.detail = std::move(detail);
            return Status::Ok();
        }
    }
    return Status::Error(StatusCode::kNotFound, "task not found");
}

std::optional<sync::ScheduledTaskRecord> TaskScheduler::FindTask(const std::string &task_id) const {
    for (const auto &task : tasks_) {
        if (task.task_id == task_id) {
            return task;
        }
    }
    return std::nullopt;
}

std::vector<sync::ScheduledTaskRecord> TaskScheduler::ListTasks() const {
    return tasks_;
}

std::vector<sync::ScheduledTaskRecord> TaskScheduler::ListTasksByDomain(sync::JobDomain domain) const {
    std::vector<sync::ScheduledTaskRecord> result;
    for (const auto &task : tasks_) {
        if (task.domain == domain) {
            result.push_back(task);
        }
    }
    return result;
}

std::vector<sync::ScheduledTaskRecord> TaskScheduler::ListTasksForNode(const std::string &node_id) const {
    std::vector<sync::ScheduledTaskRecord> result;
    for (const auto &task : tasks_) {
        if (task.source_node == node_id || task.target_node == node_id) {
            result.push_back(task);
        }
    }
    return result;
}

}  // namespace netdisk
