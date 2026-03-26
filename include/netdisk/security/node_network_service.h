#ifndef NETDISK_SECURITY_NODE_NETWORK_SERVICE_H
#define NETDISK_SECURITY_NODE_NETWORK_SERVICE_H

#include <optional>
#include <vector>

#include "netdisk/common/status.h"
#include "netdisk/core/types.h"

namespace netdisk {

class NodeNetworkService {
public:
    Status UpsertPeer(const sync::OverlayPeer &peer);
    void RefreshPeersFromDevices(const std::vector<sync::NodeInfo> &devices,
                                 std::uint64_t now_epoch,
                                 std::uint64_t offline_after_seconds);
    Status MarkPeerOffline(const std::string &node_id);
    std::optional<sync::OverlayPeer> FindPeer(const std::string &node_id) const;
    std::vector<sync::OverlayPeer> ListPeers() const;
    std::vector<sync::OverlayPeer> ListOnlinePeers() const;

private:
    std::vector<sync::OverlayPeer> peers_;
};

}  // namespace netdisk

#endif
