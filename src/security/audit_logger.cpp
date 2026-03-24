#include "netdisk/security/audit_logger.h"

#include <chrono>

namespace netdisk {

void AuditLogger::Append(const sync::AuditEvent &event) {
    sync::AuditEvent stored = event;
    if (stored.created_at_epoch == 0) {
        stored.created_at_epoch =
            static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }
    events_.push_back(stored);
}

std::vector<sync::AuditEvent> AuditLogger::ListEvents() const {
    return events_;
}

std::vector<sync::AuditEvent> AuditLogger::ListEventsByCategory(const std::string &category) const {
    std::vector<sync::AuditEvent> result;
    for (const auto &event : events_) {
        if (event.category == category) {
            result.push_back(event);
        }
    }
    return result;
}

}  // namespace netdisk
