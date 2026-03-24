#include "netdisk/sync/sync_writer.h"

#include <array>
#include <fstream>
#include <stdexcept>

namespace sync {

SyncWriter::SyncWriter(std::unique_ptr<IHasher> hasher) : hasher_(std::move(hasher)) {}

std::filesystem::path SyncWriter::Begin(const std::filesystem::path &final_path) {
    final_path_ = final_path;
    temp_path_ = final_path_;
    temp_path_ += ".part";

    std::filesystem::create_directories(final_path_.parent_path());

    std::ofstream output(temp_path_, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to create temp sync file: " + temp_path_.string());
    }

    hasher_->Reset();
    active_ = true;
    bytes_written_ = 0;
    return temp_path_;
}

std::filesystem::path SyncWriter::Resume(const std::filesystem::path &final_path) {
    final_path_ = final_path;
    temp_path_ = final_path_;
    temp_path_ += ".part";

    std::filesystem::create_directories(final_path_.parent_path());
    hasher_->Reset();
    bytes_written_ = 0;

    if (!std::filesystem::exists(temp_path_)) {
        return Begin(final_path_);
    }

    std::ifstream input(temp_path_, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to resume temp sync file: " + temp_path_.string());
    }

    std::array<char, 1024 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto read_count = static_cast<std::size_t>(input.gcount());
        if (read_count == 0) {
            break;
        }
        hasher_->Update(reinterpret_cast<const std::uint8_t *>(buffer.data()), read_count);
        bytes_written_ += read_count;
    }

    active_ = true;
    return temp_path_;
}

void SyncWriter::Append(const std::vector<std::uint8_t> &bytes) {
    if (!active_) {
        throw std::runtime_error("sync writer not active");
    }

    std::ofstream output(temp_path_, std::ios::binary | std::ios::app);
    if (!output) {
        throw std::runtime_error("failed to append temp sync file: " + temp_path_.string());
    }

    output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output.good()) {
        throw std::runtime_error("failed to write temp sync file: " + temp_path_.string());
    }

    if (!bytes.empty()) {
        hasher_->Update(bytes.data(), bytes.size());
        bytes_written_ += bytes.size();
    }
}

bool SyncWriter::VerifyAndCommit(const std::string &expected_root_hash) {
    if (!active_) {
        return false;
    }

    if (hasher_->FinalHex() != expected_root_hash) {
        Abort();
        return false;
    }

    std::error_code ec;
    std::filesystem::rename(temp_path_, final_path_, ec);
    if (ec) {
        Abort();
        return false;
    }

    active_ = false;
    temp_path_.clear();
    bytes_written_ = 0;
    return true;
}

void SyncWriter::Abort() {
    if (!temp_path_.empty()) {
        std::error_code ec;
        std::filesystem::remove(temp_path_, ec);
    }
    active_ = false;
    temp_path_.clear();
    bytes_written_ = 0;
}

std::uint64_t SyncWriter::BytesWritten() const {
    return bytes_written_;
}

}  // namespace sync
