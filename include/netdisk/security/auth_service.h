#ifndef NETDISK_SECURITY_AUTH_SERVICE_H
#define NETDISK_SECURITY_AUTH_SERVICE_H

#include <optional>
#include <vector>

#include "netdisk/common/status.h"
#include "netdisk/core/types.h"

namespace netdisk {

class AuthService {
public:
    Status RegisterIdentity(const sync::AuthIdentity &identity);
    Status UpsertSession(const sync::AuthSession &session);
    std::optional<sync::AuthIdentity> FindIdentityByUser(const std::string &user_id) const;
    std::optional<sync::AuthIdentity> FindIdentityByDevice(const std::string &device_id) const;
    std::optional<sync::AuthSession> ValidateToken(const std::string &access_token) const;
    Status RevokeSession(const std::string &session_id);

private:
    std::vector<sync::AuthIdentity> identities_;
    std::vector<sync::AuthSession> sessions_;
};

}  // namespace netdisk

#endif
