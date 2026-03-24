#include "netdisk/control_plane/device_manager.h"

namespace netdisk {

Status DeviceManager::RegisterDevice(const sync::NodeInfo &node) {
    for (auto &current : devices_) {
        if (current.node_id == node.node_id) {
            current = node;
            return Status::Ok();
        }
    }
    devices_.push_back(node);
    return Status::Ok();
}

Status DeviceManager::RecordHeartbeat(const std::string &node_id, std::uint64_t epoch_seconds) {
    for (auto &node : devices_) {
        if (node.node_id == node_id) {
            node.last_seen_epoch = epoch_seconds;
            return Status::Ok();
        }
    }
    return Status::Error(StatusCode::kNotFound, "device not found");
}

std::optional<sync::NodeInfo> DeviceManager::FindDevice(const std::string &node_id) const {
    for (const auto &node : devices_) {
        if (node.node_id == node_id) {
            return node;
        }
    }
    return std::nullopt;
}

std::vector<sync::NodeInfo> DeviceManager::ListDevices() const {
    return devices_;
}

}  // namespace netdisk
