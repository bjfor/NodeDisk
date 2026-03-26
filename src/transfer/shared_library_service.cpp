#include "netdisk/transfer/shared_library_service.h"

#include <algorithm>
#include <chrono>
#include <memory>

#include "netdisk/sync/chunker.h"
#include "netdisk/sync/hash.h"

namespace {

sync::AuditEvent MakeTransferAuditEvent(const std::string &detail) {
    sync::AuditEvent event;
    event.category = "transfer.library";
    event.detail = detail;
    return event;
}

sync::FileRecord BuildFileRecord(const std::filesystem::path &path) {
    sync::ChunkerOptions options;
    options.chunk_size = 4 * 1024 * 1024;
    sync::Chunker chunker(options, std::make_unique<sync::Md5Hasher>());
    return chunker.BuildFileRecord(path, path.parent_path());
}

std::uint64_t NowEpoch() {
    return static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

std::optional<std::size_t> FindRecipientIndex(const sync::SharedLibraryEntry &entry, const std::string &node_id) {
    for (std::size_t i = 0; i < entry.recipients.size(); ++i) {
        if (entry.recipients[i].node_id == node_id) {
            return i;
        }
    }
    return std::nullopt;
}

sync::DeviceFileRecord MakeSharedReceiveDeviceFileRecord(const sync::SharedLibraryEntry &entry,
                                                         const std::string &target_node_id,
                                                         const sync::FileRecord &file,
                                                         const std::filesystem::path &target_path) {
    sync::DeviceFileRecord record;
    record.record_id = target_node_id + "|" + target_path.lexically_normal().string();
    record.node_id = target_node_id;
    record.file_id = file.file_id;
    record.relative_path = entry.display_name;
    record.absolute_path = target_path.lexically_normal().string();
    record.source_kind = "share_receive";
    record.source_ref_id = entry.entry_id;
    record.updated_at_epoch = NowEpoch();
    return record;
}

}  // namespace

namespace netdisk {

SharedLibraryService::SharedLibraryService(TaskScheduler &scheduler,
                                           AuditLogger &audit_logger,
                                           sync::MetadataStore &metadata_store,
                                           IntegrityChecker &integrity_checker)
    : scheduler_(scheduler),
      audit_logger_(audit_logger),
      metadata_store_(metadata_store),
      integrity_checker_(integrity_checker) {}

Status SharedLibraryService::ConfigurePolicy(const sync::TransferPolicy &policy) {
    if (policy.policy_id.empty()) {
        return Status::Error(StatusCode::kInvalidArgument, "transfer policy id is required");
    }
    policy_ = policy;
    metadata_store_.UpsertTransferPolicy(policy);
    audit_logger_.Append(MakeTransferAuditEvent("configured transfer policy: " + policy.policy_id));
    return Status::Ok();
}

Status SharedLibraryService::PublishEntry(const sync::SharedLibraryEntry &entry) {
    if (entry.entry_id.empty() || entry.owner_node_id.empty() || entry.display_name.empty()) {
        return Status::Error(StatusCode::kInvalidArgument, "shared entry is missing required fields");
    }
    const auto existing = metadata_store_.GetSharedLibraryEntry(entry.entry_id);
    metadata_store_.UpsertSharedLibraryEntry(entry);
    if (existing.has_value()) {
        audit_logger_.Append(MakeTransferAuditEvent("updated shared entry: " + entry.entry_id));
        return Status::Ok();
    }
    audit_logger_.Append(MakeTransferAuditEvent("published shared entry: " + entry.entry_id));
    return Status::Ok();
}

Status SharedLibraryService::PublishFile(sync::SharedLibraryEntry entry, const std::filesystem::path &library_root) {
    if (entry.entry_id.empty() || entry.owner_node_id.empty() || entry.source_path.empty() || entry.display_name.empty()) {
        return Status::Error(StatusCode::kInvalidArgument, "shared file publish request is missing required fields");
    }
    if (library_root.empty()) {
        return Status::Error(StatusCode::kInvalidArgument, "shared library root is required");
    }

    const std::filesystem::path source_path(entry.source_path);
    if (!std::filesystem::exists(source_path) || !std::filesystem::is_regular_file(source_path)) {
        return Status::Error(StatusCode::kNotFound, "shared source file does not exist");
    }

    sync::FileRecord file;
    try {
        file = BuildFileRecord(source_path);
    } catch (const std::exception &ex) {
        return Status::Error(StatusCode::kInternalError, ex.what());
    }

    const Status verify_status = integrity_checker_.VerifyFile(file, source_path);
    if (!verify_status.ok()) {
        return verify_status;
    }

    std::filesystem::create_directories(library_root);
    std::filesystem::path target_path = library_root / entry.entry_id;
    if (source_path.has_extension()) {
        target_path += source_path.extension().string();
    }

    std::error_code ec;
    std::filesystem::copy_file(source_path, target_path, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        return Status::Error(StatusCode::kInternalError, "failed to copy file into shared library");
    }

    entry.file_id = file.file_id;
    entry.source_path = target_path.lexically_normal().string();
    entry.expired = false;
    metadata_store_.UpsertFile(file);
    return PublishEntry(entry);
}

Status SharedLibraryService::CreateReceiveTask(const std::string &entry_id, const std::string &target_node_id) {
    auto entry = metadata_store_.GetSharedLibraryEntry(entry_id);
    if (!entry.has_value()) {
        return Status::Error(StatusCode::kNotFound, "shared entry not found");
    }
    if (entry->expired) {
        return Status::Error(StatusCode::kPermissionDenied, "shared entry is expired");
    }
    if (const auto recipient_index = FindRecipientIndex(*entry, target_node_id);
        recipient_index.has_value() &&
        entry->recipients[*recipient_index].state == sync::SharedRecipientState::kDelivered) {
        return Status::Error(StatusCode::kAlreadyExists, "entry already delivered to target node");
    }
    sync::ScheduledTaskRecord task;
    task.task_id = entry->entry_id + "->" + target_node_id;
    task.domain = sync::JobDomain::kTransfer;
    task.related_id = entry->entry_id;
    task.source_node = entry->owner_node_id;
    task.target_node = target_node_id;
    task.created_at_epoch = entry->created_at_epoch;
    task.detail = entry->display_name;
    const Status status = scheduler_.ScheduleTask(task);
    if (status.ok()) {
        if (const auto recipient_index = FindRecipientIndex(*entry, target_node_id); recipient_index.has_value()) {
            entry->recipients[*recipient_index].state = sync::SharedRecipientState::kPending;
            entry->recipients[*recipient_index].received_path.clear();
            entry->recipients[*recipient_index].error_message.clear();
            entry->recipients[*recipient_index].updated_at_epoch = NowEpoch();
        } else {
            sync::SharedLibraryRecipient recipient;
            recipient.entry_id = entry->entry_id;
            recipient.node_id = target_node_id;
            recipient.state = sync::SharedRecipientState::kPending;
            recipient.updated_at_epoch = NowEpoch();
            entry->recipients.push_back(recipient);
        }
        metadata_store_.UpsertSharedLibraryEntry(*entry);
        audit_logger_.Append(MakeTransferAuditEvent("scheduled receive task for entry: " + entry->entry_id));
    }
    return status;
}

Status SharedLibraryService::ReceiveEntry(const std::string &entry_id,
                                         const std::string &target_node_id,
                                         const std::filesystem::path &target_root) {
    if (target_root.empty()) {
        return Status::Error(StatusCode::kInvalidArgument, "receive target root is required");
    }

    auto entry = FindEntry(entry_id);
    if (!entry.has_value()) {
        return Status::Error(StatusCode::kNotFound, "shared entry not found");
    }
    if (entry->expired) {
        return Status::Error(StatusCode::kPermissionDenied, "shared entry is expired");
    }

    const Status task_status = CreateReceiveTask(entry_id, target_node_id);
    if (!task_status.ok() && task_status.code != StatusCode::kAlreadyExists) {
        return task_status;
    }
    entry = FindEntry(entry_id);
    if (!entry.has_value()) {
        return Status::Error(StatusCode::kNotFound, "shared entry not found after receive task creation");
    }

    const std::filesystem::path source_path(entry->source_path);
    if (!std::filesystem::exists(source_path) || !std::filesystem::is_regular_file(source_path)) {
        if (const auto recipient_index = FindRecipientIndex(*entry, target_node_id); recipient_index.has_value()) {
            entry->recipients[*recipient_index].state = sync::SharedRecipientState::kFailed;
            entry->recipients[*recipient_index].error_message = "shared library file is missing";
            entry->recipients[*recipient_index].updated_at_epoch = NowEpoch();
            metadata_store_.UpsertSharedLibraryEntry(*entry);
        }
        return Status::Error(StatusCode::kNotFound, "shared library file is missing");
    }

    std::filesystem::create_directories(target_root);
    const std::filesystem::path target_path = target_root / entry->display_name;
    std::error_code ec;
    std::filesystem::copy_file(source_path, target_path, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        if (const auto recipient_index = FindRecipientIndex(*entry, target_node_id); recipient_index.has_value()) {
            entry->recipients[*recipient_index].state = sync::SharedRecipientState::kFailed;
            entry->recipients[*recipient_index].error_message = "failed to materialize shared file";
            entry->recipients[*recipient_index].updated_at_epoch = NowEpoch();
            metadata_store_.UpsertSharedLibraryEntry(*entry);
        }
        return Status::Error(StatusCode::kInternalError, "failed to materialize shared file");
    }

    sync::FileRecord file;
    try {
        file = BuildFileRecord(target_path);
    } catch (const std::exception &ex) {
        return Status::Error(StatusCode::kInternalError, ex.what());
    }

    const Status verify_status = integrity_checker_.VerifyFile(file, target_path);
    if (!verify_status.ok()) {
        if (const auto recipient_index = FindRecipientIndex(*entry, target_node_id); recipient_index.has_value()) {
            entry->recipients[*recipient_index].state = sync::SharedRecipientState::kFailed;
            entry->recipients[*recipient_index].error_message = verify_status.message;
            entry->recipients[*recipient_index].updated_at_epoch = NowEpoch();
            metadata_store_.UpsertSharedLibraryEntry(*entry);
        }
        return verify_status;
    }

    metadata_store_.UpsertFile(file);
    metadata_store_.UpsertDeviceFile(MakeSharedReceiveDeviceFileRecord(*entry, target_node_id, file, target_path));
    const std::string task_id = entry->entry_id + "->" + target_node_id;
    if (task_status.code == StatusCode::kAlreadyExists) {
        audit_logger_.Append(MakeTransferAuditEvent("replayed receive for entry: " + entry->entry_id));
    }
    scheduler_.UpdateTaskState(task_id, sync::JobState::kCompleted, target_path.lexically_normal().string());
    return MarkDelivered(entry_id, target_node_id, target_path.lexically_normal().string());
}

Status SharedLibraryService::RetryReceive(const std::string &entry_id,
                                          const std::string &target_node_id,
                                          const std::filesystem::path &target_root) {
    const auto task_id = entry_id + "->" + target_node_id;
    const auto retry_status = scheduler_.RetryTask(task_id, "manual share receive retry requested");
    if (!retry_status.ok()) {
        return retry_status;
    }

    auto entry = metadata_store_.GetSharedLibraryEntry(entry_id);
    if (entry.has_value()) {
        if (const auto recipient_index = FindRecipientIndex(*entry, target_node_id); recipient_index.has_value()) {
            entry->recipients[*recipient_index].state = sync::SharedRecipientState::kPending;
            entry->recipients[*recipient_index].error_message.clear();
            entry->recipients[*recipient_index].updated_at_epoch = NowEpoch();
            metadata_store_.UpsertSharedLibraryEntry(*entry);
        }
    }

    audit_logger_.Append(MakeTransferAuditEvent("retry receive for entry: " + entry_id));
    return ReceiveEntry(entry_id, target_node_id, target_root);
}

Status SharedLibraryService::MarkDelivered(const std::string &entry_id,
                                          const std::string &target_node_id,
                                          const std::string &received_path) {
    auto entry = metadata_store_.GetSharedLibraryEntry(entry_id);
    if (!entry.has_value()) {
        return Status::Error(StatusCode::kNotFound, "shared entry not found");
    }
    if (const auto recipient_index = FindRecipientIndex(*entry, target_node_id); recipient_index.has_value()) {
        entry->recipients[*recipient_index].state = sync::SharedRecipientState::kDelivered;
        entry->recipients[*recipient_index].received_path = received_path;
        entry->recipients[*recipient_index].error_message.clear();
        entry->recipients[*recipient_index].updated_at_epoch = NowEpoch();
    } else {
        sync::SharedLibraryRecipient recipient;
        recipient.entry_id = entry->entry_id;
        recipient.node_id = target_node_id;
        recipient.state = sync::SharedRecipientState::kDelivered;
        recipient.received_path = received_path;
        recipient.updated_at_epoch = NowEpoch();
        entry->recipients.push_back(recipient);
    }
    entry->delivered = true;
    if (std::find(entry->recipient_nodes.begin(), entry->recipient_nodes.end(), target_node_id) ==
        entry->recipient_nodes.end()) {
        entry->recipient_nodes.push_back(target_node_id);
    }
    metadata_store_.UpsertSharedLibraryEntry(*entry);
    audit_logger_.Append(MakeTransferAuditEvent("marked delivered: " + entry->entry_id));
    return Status::Ok();
}

std::vector<sync::SharedLibraryEntry> SharedLibraryService::ListEntries() const {
    return metadata_store_.ListSharedLibraryEntries();
}

std::vector<sync::SharedLibraryEntry> SharedLibraryService::ListActiveEntries() const {
    std::vector<sync::SharedLibraryEntry> result;
    for (const auto &entry : metadata_store_.ListSharedLibraryEntries()) {
        if (!entry.expired) {
            result.push_back(entry);
        }
    }
    return result;
}

std::vector<sync::SharedLibraryEntry> SharedLibraryService::ListEntriesForNode(const std::string &node_id) const {
    std::vector<sync::SharedLibraryEntry> result;
    for (const auto &entry : metadata_store_.ListSharedLibraryEntries()) {
        bool related_recipient = std::find(entry.recipient_nodes.begin(), entry.recipient_nodes.end(), node_id) !=
                                 entry.recipient_nodes.end();
        if (!related_recipient) {
            related_recipient = FindRecipientIndex(entry, node_id).has_value();
        }
        if (entry.owner_node_id == node_id || related_recipient) {
            result.push_back(entry);
        }
    }
    return result;
}

std::vector<sync::SharedLibraryEntry> SharedLibraryService::ListPendingEntriesForNode(const std::string &node_id) const {
    std::vector<sync::SharedLibraryEntry> result;
    for (const auto &entry : metadata_store_.ListSharedLibraryEntries()) {
        bool delivered_to_node = false;
        bool pending_or_failed_for_node = false;
        for (const auto &recipient : entry.recipients) {
            if (recipient.node_id != node_id) {
                continue;
            }
            if (recipient.state == sync::SharedRecipientState::kDelivered) {
                delivered_to_node = true;
            }
            if (recipient.state == sync::SharedRecipientState::kPending ||
                recipient.state == sync::SharedRecipientState::kFailed) {
                pending_or_failed_for_node = true;
            }
        }
        if (!entry.expired && entry.owner_node_id != node_id && (!delivered_to_node || pending_or_failed_for_node)) {
            result.push_back(entry);
        }
    }
    return result;
}

std::optional<sync::SharedLibraryEntry> SharedLibraryService::FindEntry(const std::string &entry_id) const {
    return metadata_store_.GetSharedLibraryEntry(entry_id);
}

std::size_t SharedLibraryService::CleanupExpired(std::uint64_t now_epoch) {
    std::size_t removed = 0;
    for (auto entry : metadata_store_.ListSharedLibraryEntries()) {
        if (!entry.expired && entry.expires_at_epoch != 0 && entry.expires_at_epoch <= now_epoch) {
            entry.expired = true;
            entry.delivered = true;
            std::error_code ec;
            std::filesystem::remove(entry.source_path, ec);
            metadata_store_.UpsertSharedLibraryEntry(entry);
            ++removed;
        }
    }
    if (removed > 0) {
        audit_logger_.Append(MakeTransferAuditEvent("cleaned expired shared entries: " + std::to_string(removed)));
    }
    return removed;
}

}  // namespace netdisk
