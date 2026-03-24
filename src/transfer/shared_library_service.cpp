#include "netdisk/transfer/shared_library_service.h"

#include <algorithm>
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
    audit_logger_.Append(MakeTransferAuditEvent("configured transfer policy: " + policy.policy_id));
    return Status::Ok();
}

Status SharedLibraryService::PublishEntry(const sync::SharedLibraryEntry &entry) {
    if (entry.entry_id.empty() || entry.owner_node_id.empty() || entry.display_name.empty()) {
        return Status::Error(StatusCode::kInvalidArgument, "shared entry is missing required fields");
    }
    for (auto &current : entries_) {
        if (current.entry_id == entry.entry_id) {
            current = entry;
            audit_logger_.Append(MakeTransferAuditEvent("updated shared entry: " + entry.entry_id));
            return Status::Ok();
        }
    }
    entries_.push_back(entry);
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
    metadata_store_.UpsertFile(file);
    return PublishEntry(entry);
}

Status SharedLibraryService::CreateReceiveTask(const std::string &entry_id, const std::string &target_node_id) {
    for (const auto &entry : entries_) {
        if (entry.entry_id == entry_id) {
            if (entry.delivered &&
                std::find(entry.recipient_nodes.begin(), entry.recipient_nodes.end(), target_node_id) !=
                    entry.recipient_nodes.end()) {
                return Status::Error(StatusCode::kAlreadyExists, "entry already delivered to target node");
            }
            sync::ScheduledTaskRecord task;
            task.task_id = entry.entry_id + "->" + target_node_id;
            task.domain = sync::JobDomain::kTransfer;
            task.related_id = entry.entry_id;
            task.source_node = entry.owner_node_id;
            task.target_node = target_node_id;
            task.detail = entry.display_name;
            const Status status = scheduler_.ScheduleTask(task);
            if (status.ok()) {
                audit_logger_.Append(MakeTransferAuditEvent("scheduled receive task for entry: " + entry.entry_id));
            }
            return status;
        }
    }
    return Status::Error(StatusCode::kNotFound, "shared entry not found");
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

    const Status task_status = CreateReceiveTask(entry_id, target_node_id);
    if (!task_status.ok() && task_status.code != StatusCode::kAlreadyExists) {
        return task_status;
    }

    const std::filesystem::path source_path(entry->source_path);
    if (!std::filesystem::exists(source_path) || !std::filesystem::is_regular_file(source_path)) {
        return Status::Error(StatusCode::kNotFound, "shared library file is missing");
    }

    std::filesystem::create_directories(target_root);
    const std::filesystem::path target_path = target_root / entry->display_name;
    std::error_code ec;
    std::filesystem::copy_file(source_path, target_path, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
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
        return verify_status;
    }

    metadata_store_.UpsertFile(file);
    const std::string task_id = entry->entry_id + "->" + target_node_id;
    scheduler_.UpdateTaskState(task_id, sync::JobState::kCompleted, target_path.lexically_normal().string());
    return MarkDelivered(entry_id, target_node_id);
}

Status SharedLibraryService::MarkDelivered(const std::string &entry_id, const std::string &target_node_id) {
    for (auto &entry : entries_) {
        if (entry.entry_id == entry_id) {
            entry.delivered = true;
            if (std::find(entry.recipient_nodes.begin(), entry.recipient_nodes.end(), target_node_id) ==
                entry.recipient_nodes.end()) {
                entry.recipient_nodes.push_back(target_node_id);
            }
            audit_logger_.Append(MakeTransferAuditEvent("marked delivered: " + entry.entry_id));
            return Status::Ok();
        }
    }
    return Status::Error(StatusCode::kNotFound, "shared entry not found");
}

std::vector<sync::SharedLibraryEntry> SharedLibraryService::ListEntries() const {
    return entries_;
}

std::vector<sync::SharedLibraryEntry> SharedLibraryService::ListEntriesForNode(const std::string &node_id) const {
    std::vector<sync::SharedLibraryEntry> result;
    for (const auto &entry : entries_) {
        if (entry.owner_node_id == node_id ||
            std::find(entry.recipient_nodes.begin(), entry.recipient_nodes.end(), node_id) !=
                entry.recipient_nodes.end()) {
            result.push_back(entry);
        }
    }
    return result;
}

std::optional<sync::SharedLibraryEntry> SharedLibraryService::FindEntry(const std::string &entry_id) const {
    for (const auto &entry : entries_) {
        if (entry.entry_id == entry_id) {
            return entry;
        }
    }
    return std::nullopt;
}

std::size_t SharedLibraryService::CleanupExpired(std::uint64_t now_epoch) {
    const auto old_size = entries_.size();
    entries_.erase(std::remove_if(entries_.begin(),
                                  entries_.end(),
                                  [now_epoch](const sync::SharedLibraryEntry &entry) {
                                      return entry.expires_at_epoch != 0 && entry.expires_at_epoch <= now_epoch;
                                  }),
                  entries_.end());
    const auto removed = old_size - entries_.size();
    if (removed > 0) {
        audit_logger_.Append(MakeTransferAuditEvent("cleaned expired shared entries: " + std::to_string(removed)));
    }
    return removed;
}

}  // namespace netdisk
