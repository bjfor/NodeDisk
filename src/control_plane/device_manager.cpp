#include "netdisk/control_plane/device_manager.h"

namespace netdisk {

DeviceManager::DeviceManager(sync::MetadataStore &store) : store_(store) {}

Status DeviceManager::RegisterDevice(const sync::NodeInfo &node) {
    store_.UpsertNode(node);
    return Status::Ok();
}

Status DeviceManager::RecordHeartbeat(const std::string &node_id, std::uint64_t epoch_seconds) {
    auto node = store_.GetNode(node_id);
    if (!node.has_value()) {
        return Status::Error(StatusCode::kNotFound, "device not found");
    }
    node->last_seen_epoch = epoch_seconds;
    store_.UpsertNode(*node);
    return Status::Ok();
}

std::optional<sync::NodeInfo> DeviceManager::FindDevice(const std::string &node_id) const {
    return store_.GetNode(node_id);
}

std::vector<sync::NodeInfo> DeviceManager::ListDevices() const {
    return store_.ListNodes();
}

std::vector<sync::NodeInfo> DeviceManager::ListOnlineDevices(std::uint64_t now_epoch,
                                                             std::uint64_t offline_after_seconds) const {
    std::vector<sync::NodeInfo> result;
    for (const auto &node : store_.ListNodes()) {
        if (node.last_seen_epoch != 0 && now_epoch >= node.last_seen_epoch &&
            (now_epoch - node.last_seen_epoch) <= offline_after_seconds) {
            result.push_back(node);
        }
    }
    return result;
}

bool DeviceManager::IsDeviceOnline(const std::string &node_id,
                                   std::uint64_t now_epoch,
                                   std::uint64_t offline_after_seconds) const {
    const auto node = store_.GetNode(node_id);
    if (!node.has_value() || node->last_seen_epoch == 0 || now_epoch < node->last_seen_epoch) {
        return false;
    }
    return (now_epoch - node->last_seen_epoch) <= offline_after_seconds;
}

}  // namespace netdisk
