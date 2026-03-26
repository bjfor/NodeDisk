#include "netdisk/security/node_network_service.h"

namespace netdisk {

Status NodeNetworkService::UpsertPeer(const sync::OverlayPeer &peer) {
    for (auto &current : peers_) {
        if (current.node_id == peer.node_id) {
            current = peer;
            return Status::Ok();
        }
    }
    peers_.push_back(peer);
    return Status::Ok();
}

void NodeNetworkService::RefreshPeersFromDevices(const std::vector<sync::NodeInfo> &devices,
                                                 std::uint64_t now_epoch,
                                                 std::uint64_t offline_after_seconds) {
    for (const auto &device : devices) {
        sync::OverlayPeer peer;
        peer.node_id = device.node_id;
        peer.virtual_ip = device.zt_ip;
        peer.last_seen_epoch = device.last_seen_epoch;
        peer.online = device.last_seen_epoch != 0 && now_epoch >= device.last_seen_epoch &&
                      (now_epoch - device.last_seen_epoch) <= offline_after_seconds;
        UpsertPeer(peer);
    }
}

Status NodeNetworkService::MarkPeerOffline(const std::string &node_id) {
    for (auto &peer : peers_) {
        if (peer.node_id == node_id) {
            peer.online = false;
            return Status::Ok();
        }
    }
    return Status::Error(StatusCode::kNotFound, "peer not found");
}

std::optional<sync::OverlayPeer> NodeNetworkService::FindPeer(const std::string &node_id) const {
    for (const auto &peer : peers_) {
        if (peer.node_id == node_id) {
            return peer;
        }
    }
    return std::nullopt;
}

std::vector<sync::OverlayPeer> NodeNetworkService::ListPeers() const {
    return peers_;
}

std::vector<sync::OverlayPeer> NodeNetworkService::ListOnlinePeers() const {
    std::vector<sync::OverlayPeer> result;
    for (const auto &peer : peers_) {
        if (peer.online) {
            result.push_back(peer);
        }
    }
    return result;
}

}  // namespace netdisk
