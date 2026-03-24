#include "netdisk/storage/integrity_checker.h"

#include <filesystem>

#include "netdisk/sync/chunker.h"
#include "netdisk/sync/hash.h"

namespace netdisk {

Status IntegrityChecker::VerifyFile(const sync::FileRecord &file, const std::filesystem::path &path) const {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return Status::Error(StatusCode::kNotFound, "file path does not exist");
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return Status::Error(StatusCode::kInternalError, "failed to stat file");
    }
    if (size != file.size) {
        return Status::Error(StatusCode::kInvalidArgument, "file size does not match metadata");
    }

    sync::ChunkerOptions options;
    sync::Chunker chunker(options, std::make_unique<sync::Md5Hasher>());
    const auto rebuilt = chunker.BuildFileRecord(path, path.parent_path());
    if (rebuilt.root_hash != file.root_hash) {
        return Status::Error(StatusCode::kInvalidArgument, "file hash does not match metadata");
    }
    return Status::Ok();
}

}  // namespace netdisk
