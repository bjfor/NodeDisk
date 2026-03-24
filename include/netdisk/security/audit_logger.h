#ifndef NETDISK_SECURITY_AUDIT_LOGGER_H
#define NETDISK_SECURITY_AUDIT_LOGGER_H

#include <string>
#include <vector>

#include "netdisk/core/types.h"

namespace netdisk {

class AuditLogger {
public:
    void Append(const sync::AuditEvent &event);
    std::vector<sync::AuditEvent> ListEvents() const;
    std::vector<sync::AuditEvent> ListEventsByCategory(const std::string &category) const;

private:
    std::vector<sync::AuditEvent> events_;
};

}  // namespace netdisk

#endif
