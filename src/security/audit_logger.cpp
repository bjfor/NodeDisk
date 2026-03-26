#include "netdisk/security/audit_logger.h"

#include <atomic>
#include <chrono>

namespace netdisk {

namespace {

std::string MakeFallbackEventId(std::uint64_t created_at_epoch) {
    static std::atomic<std::uint64_t> counter{0};
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    return "audit-" + std::to_string(created_at_epoch) + "-" + std::to_string(now_ns) + "-" +
           std::to_string(++counter);
}

}  // namespace

AuditLogger::AuditLogger(sync::MetadataStore &metadata_store) : metadata_store_(metadata_store) {}

void AuditLogger::Append(const sync::AuditEvent &event) {
    sync::AuditEvent stored = event;
    if (stored.created_at_epoch == 0) {
        stored.created_at_epoch =
            static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }
    if (stored.event_id.empty()) {
        stored.event_id = MakeFallbackEventId(stored.created_at_epoch);
    }
    metadata_store_.AppendAuditEvent(stored);
}

std::vector<sync::AuditEvent> AuditLogger::ListEvents() const {
    return metadata_store_.ListAuditEvents();
}

std::vector<sync::AuditEvent> AuditLogger::ListEventsByCategory(const std::string &category) const {
    return metadata_store_.ListAuditEventsByCategory(category);
}

}  // namespace netdisk
