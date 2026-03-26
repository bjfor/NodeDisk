#ifndef NETDISK_SECURITY_AUDIT_LOGGER_H
#define NETDISK_SECURITY_AUDIT_LOGGER_H

#include <string>

#include "netdisk/core/types.h"
#include "netdisk/metadata/metadata_store.h"

namespace netdisk {

class AuditLogger {
public:
    explicit AuditLogger(sync::MetadataStore &metadata_store);

    void Append(const sync::AuditEvent &event);
    std::vector<sync::AuditEvent> ListEvents() const;
    std::vector<sync::AuditEvent> ListEventsByCategory(const std::string &category) const;

private:
    sync::MetadataStore &metadata_store_;
};

}  // namespace netdisk

#endif
