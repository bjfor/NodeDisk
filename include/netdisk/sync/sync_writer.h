#ifndef SYNC_SYNC_WRITER_H
#define SYNC_SYNC_WRITER_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "netdisk/sync/hash.h"

namespace sync {

class SyncWriter {
public:
    explicit SyncWriter(std::unique_ptr<IHasher> hasher);

    std::filesystem::path Begin(const std::filesystem::path &final_path);
    std::filesystem::path Resume(const std::filesystem::path &final_path);
    void Append(const std::vector<std::uint8_t> &bytes);
    bool VerifyAndCommit(const std::string &expected_root_hash);
    void Abort();
    std::uint64_t BytesWritten() const;

private:
    std::unique_ptr<IHasher> hasher_;
    std::filesystem::path final_path_;
    std::filesystem::path temp_path_;
    bool active_ = false;
    std::uint64_t bytes_written_ = 0;
};

}  // namespace sync

#endif
