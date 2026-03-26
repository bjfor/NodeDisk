#include "netdisk/control_plane/task_scheduler.h"

namespace netdisk {

TaskScheduler::TaskScheduler(sync::MetadataStore &store) : store_(store) {}

Status TaskScheduler::ScheduleTask(const sync::ScheduledTaskRecord &task) {
    store_.UpsertScheduledTask(task);
    return Status::Ok();
}

Status TaskScheduler::UpdateTaskState(const std::string &task_id, sync::JobState state, std::string detail) {
    auto task = store_.GetScheduledTaskRecord(task_id);
    if (!task.has_value()) {
        return Status::Error(StatusCode::kNotFound, "task not found");
    }
    task->state = state;
    task->detail = std::move(detail);
    store_.UpsertScheduledTask(*task);
    return Status::Ok();
}

Status TaskScheduler::RetryTask(const std::string &task_id, std::string detail) {
    auto task = store_.GetScheduledTaskRecord(task_id);
    if (!task.has_value()) {
        return Status::Error(StatusCode::kNotFound, "task not found");
    }
    task->state = sync::JobState::kQueued;
    ++task->retry_count;
    task->detail = std::move(detail);
    store_.UpsertScheduledTask(*task);
    return Status::Ok();
}

std::optional<sync::ScheduledTaskRecord> TaskScheduler::FindTask(const std::string &task_id) const {
    return store_.GetScheduledTaskRecord(task_id);
}

std::vector<sync::ScheduledTaskRecord> TaskScheduler::ListTasks() const {
    return store_.ListScheduledTasks();
}

std::vector<sync::ScheduledTaskRecord> TaskScheduler::ListTasksByDomain(sync::JobDomain domain) const {
    std::vector<sync::ScheduledTaskRecord> result;
    for (const auto &task : store_.ListScheduledTasks()) {
        if (task.domain == domain) {
            result.push_back(task);
        }
    }
    return result;
}

std::vector<sync::ScheduledTaskRecord> TaskScheduler::ListTasksForNode(const std::string &node_id) const {
    std::vector<sync::ScheduledTaskRecord> result;
    for (const auto &task : store_.ListScheduledTasks()) {
        if (task.source_node == node_id || task.target_node == node_id) {
            result.push_back(task);
        }
    }
    return result;
}

std::vector<sync::ScheduledTaskRecord> TaskScheduler::ListTasksByState(sync::JobState state) const {
    return store_.ListScheduledTasksByState(state);
}

std::vector<sync::ScheduledTaskRecord> TaskScheduler::ListRetryableTasks() const {
    return store_.ListScheduledTasksByState(sync::JobState::kFailed);
}

}  // namespace netdisk
