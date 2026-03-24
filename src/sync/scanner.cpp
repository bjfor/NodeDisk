#include "netdisk/sync/scanner.h"

#include <filesystem>

namespace sync {

Scanner::Scanner(const Chunker &chunker, MetadataStore &store) : chunker_(chunker), store_(store) {}

namespace {

bool IsExcludedPath(const std::filesystem::path &candidate,
                    const std::vector<std::filesystem::path> &excluded_paths) {
    const auto normalized_candidate = candidate.lexically_normal();
    for (const auto &excluded : excluded_paths) {
        if (!excluded.empty() && normalized_candidate == excluded.lexically_normal()) {
            return true;
        }
    }
    return false;
}

}  // namespace

std::vector<FileRecord> Scanner::ScanDirectory(const std::filesystem::path &root_path,
                                               const std::vector<std::filesystem::path> &excluded_paths) {
    std::vector<FileRecord> files;

    if (!std::filesystem::exists(root_path)) {
        return files;
    }

    for (const auto &entry : std::filesystem::recursive_directory_iterator(root_path)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == ".part") {
            continue;
        }
        if (IsExcludedPath(entry.path(), excluded_paths)) {
            continue;
        }

        FileRecord file = chunker_.BuildFileRecord(entry.path(), root_path);
        store_.UpsertFile(file);
        files.push_back(file);
    }

    return files;
}

}  // namespace sync
