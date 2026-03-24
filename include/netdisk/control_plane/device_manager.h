#ifndef NETDISK_CONTROL_PLANE_DEVICE_MANAGER_H
#define NETDISK_CONTROL_PLANE_DEVICE_MANAGER_H

#include <optional>
#include <vector>

#include "netdisk/common/status.h"
#include "netdisk/core/types.h"

namespace netdisk {

class DeviceManager {
public:
    Status RegisterDevice(const sync::NodeInfo &node);
    Status RecordHeartbeat(const std::string &node_id, std::uint64_t epoch_seconds);
    std::optional<sync::NodeInfo> FindDevice(const std::string &node_id) const;
    std::vector<sync::NodeInfo> ListDevices() const;

private:
    std::vector<sync::NodeInfo> devices_;
};

}  // namespace netdisk

#endif
