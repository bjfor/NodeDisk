#include "netdisk/security/auth_service.h"

#include <chrono>

namespace netdisk {

Status AuthService::RegisterIdentity(const sync::AuthIdentity &identity) {
    if (identity.user_id.empty() || identity.username.empty() || identity.device_id.empty()) {
        return Status::Error(StatusCode::kInvalidArgument, "identity is missing required fields");
    }
    for (auto &current : identities_) {
        if (current.user_id == identity.user_id) {
            current = identity;
            return Status::Ok();
        }
    }
    identities_.push_back(identity);
    return Status::Ok();
}

Status AuthService::UpsertSession(const sync::AuthSession &session) {
    if (session.session_id.empty() || session.user_id.empty() || session.device_id.empty() ||
        session.access_token.empty()) {
        return Status::Error(StatusCode::kInvalidArgument, "session is missing required fields");
    }
    for (auto &current : sessions_) {
        if (current.session_id == session.session_id) {
            current = session;
            return Status::Ok();
        }
    }
    sessions_.push_back(session);
    return Status::Ok();
}

std::optional<sync::AuthIdentity> AuthService::FindIdentityByUser(const std::string &user_id) const {
    for (const auto &identity : identities_) {
        if (identity.user_id == user_id) {
            return identity;
        }
    }
    return std::nullopt;
}

std::optional<sync::AuthIdentity> AuthService::FindIdentityByDevice(const std::string &device_id) const {
    for (const auto &identity : identities_) {
        if (identity.device_id == device_id) {
            return identity;
        }
    }
    return std::nullopt;
}

std::optional<sync::AuthSession> AuthService::ValidateToken(const std::string &access_token) const {
    const auto now = static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    for (const auto &session : sessions_) {
        if (session.access_token == access_token && (session.expires_at_epoch == 0 || session.expires_at_epoch >= now)) {
            return session;
        }
    }
    return std::nullopt;
}

Status AuthService::RevokeSession(const std::string &session_id) {
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
        if (it->session_id == session_id) {
            sessions_.erase(it);
            return Status::Ok();
        }
    }
    return Status::Error(StatusCode::kNotFound, "session not found");
}

}  // namespace netdisk
