#include "netdisk/storage/storage_backend.h"

#include <ctime>
#include <filesystem>
#include <stdexcept>

extern "C" {
#include "netdisk/storage/fdfs_api.h"
}

namespace sync {

namespace {

std::uint64_t CurrentEpoch() {
    return static_cast<std::uint64_t>(std::time(nullptr));
}

class NullStorageBackend : public IStorageBackend {
public:
    std::string Name() const override {
        return "none";
    }

    StoredObjectRecord StoreFile(const FileRecord &file,
                                 const std::filesystem::path &local_path) override {
        StoredObjectRecord record;
        record.file_id = file.file_id;
        record.backend_name = Name();
        record.storage_key = file.file_id;
        record.source_path = local_path.lexically_normal().string();
        record.stored_at_epoch = CurrentEpoch();
        return record;
    }

    void RestoreFile(const StoredObjectRecord &record,
                     const std::filesystem::path &target_path) override {
        if (record.source_path.empty()) {
            throw std::runtime_error("none backend cannot restore without source path");
        }
        std::filesystem::create_directories(target_path.parent_path());
        std::filesystem::copy_file(record.source_path, target_path, std::filesystem::copy_options::overwrite_existing);
    }
};

class MirrorStorageBackend : public IStorageBackend {
public:
    explicit MirrorStorageBackend(std::filesystem::path root) : root_(std::move(root)) {}

    std::string Name() const override {
        return "mirror";
    }

    StoredObjectRecord StoreFile(const FileRecord &file,
                                 const std::filesystem::path &local_path) override {
        if (root_.empty()) {
            throw std::runtime_error("mirror backend requires a destination root");
        }

        const std::filesystem::path target_dir = root_ / file.file_id.substr(0, 2);
        std::filesystem::create_directories(target_dir);

        std::filesystem::path target_path = target_dir / file.file_id;
        const std::filesystem::path ext = local_path.extension();
        if (!ext.empty()) {
            target_path += ext.string();
        }

        std::filesystem::copy_file(local_path,
                                   target_path,
                                   std::filesystem::copy_options::overwrite_existing);

        StoredObjectRecord record;
        record.file_id = file.file_id;
        record.backend_name = Name();
        record.storage_key = target_path.lexically_normal().string();
        record.access_url = target_path.lexically_normal().string();
        record.source_path = local_path.lexically_normal().string();
        record.stored_at_epoch = CurrentEpoch();
        return record;
    }

    void RestoreFile(const StoredObjectRecord &record,
                     const std::filesystem::path &target_path) override {
        if (record.storage_key.empty()) {
            throw std::runtime_error("mirror backend restore requires storage key");
        }
        std::filesystem::create_directories(target_path.parent_path());
        std::filesystem::copy_file(record.storage_key, target_path, std::filesystem::copy_options::overwrite_existing);
    }

private:
    std::filesystem::path root_;
};

class LegacyFastDfsStorageBackend : public IStorageBackend {
public:
    std::string Name() const override {
        return "fastdfs";
    }

    StoredObjectRecord StoreFile(const FileRecord &file,
                                 const std::filesystem::path &local_path) override {
        char fileid[512] = {0};
        char file_url[512] = {0};

        if (fdfs_upload_file(local_path.c_str(), fileid) != 0) {
            throw std::runtime_error("failed to upload file into FastDFS: " + local_path.string());
        }
        if (fdfs_make_file_url(fileid, file_url) != 0) {
            throw std::runtime_error("failed to build FastDFS file url for: " + std::string(fileid));
        }

        StoredObjectRecord record;
        record.file_id = file.file_id;
        record.backend_name = Name();
        record.storage_key = fileid;
        record.access_url = file_url;
        record.source_path = local_path.lexically_normal().string();
        record.stored_at_epoch = CurrentEpoch();
        return record;
    }

    void RestoreFile(const StoredObjectRecord &record,
                     const std::filesystem::path &target_path) override {
        if (record.storage_key.empty()) {
            throw std::runtime_error("fastdfs restore requires storage key");
        }

        std::filesystem::create_directories(target_path.parent_path());
        if (fdfs_download_file(record.storage_key.c_str(), target_path.lexically_normal().string().c_str()) != 0) {
            throw std::runtime_error("failed to restore file from FastDFS: " + record.storage_key);
        }
    }
};

}  // namespace

std::unique_ptr<IStorageBackend> CreateStorageBackend(const std::string &backend_name,
                                                      const std::string &backend_arg) {
    if (backend_name.empty() || backend_name == "none") {
        return std::make_unique<NullStorageBackend>();
    }
    if (backend_name == "mirror") {
        return std::make_unique<MirrorStorageBackend>(backend_arg);
    }
    if (backend_name == "fastdfs") {
        return std::make_unique<LegacyFastDfsStorageBackend>();
    }

    throw std::runtime_error("unknown storage backend: " + backend_name);
}

}  // namespace sync
