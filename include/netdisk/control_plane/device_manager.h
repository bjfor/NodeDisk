#ifndef NETDISK_CONTROL_PLANE_DEVICE_MANAGER_H
#define NETDISK_CONTROL_PLANE_DEVICE_MANAGER_H

#include <optional>
#include <vector>

#include "netdisk/common/status.h"
#include "netdisk/core/types.h"
#include "netdisk/metadata/metadata_store.h"

namespace netdisk {

class DeviceManager {
public:
    explicit DeviceManager(sync::MetadataStore &store);

    Status RegisterDevice(const sync::NodeInfo &node);
    Status RecordHeartbeat(const std::string &node_id, std::uint64_t epoch_seconds);
    std::optional<sync::NodeInfo> FindDevice(const std::string &node_id) const;
    std::vector<sync::NodeInfo> ListDevices() const;
    std::vector<sync::NodeInfo> ListOnlineDevices(std::uint64_t now_epoch, std::uint64_t offline_after_seconds) const;
    bool IsDeviceOnline(const std::string &node_id,
                        std::uint64_t now_epoch,
                        std::uint64_t offline_after_seconds) const;

private:
    sync::MetadataStore &store_;
};

}  // namespace netdisk

#endif
