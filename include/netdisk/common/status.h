#ifndef NETDISK_COMMON_STATUS_H
#define NETDISK_COMMON_STATUS_H

#include <string>

namespace netdisk {

enum class StatusCode {
    kOk = 0,
    kNotFound,
    kInvalidArgument,
    kAlreadyExists,
    kPermissionDenied,
    kInternalError,
};

struct Status {
    StatusCode code = StatusCode::kOk;
    std::string message;

    bool ok() const {
        return code == StatusCode::kOk;
    }

    static Status Ok() {
        return {};
    }

    static Status Error(StatusCode code_value, std::string text) {
        Status status;
        status.code = code_value;
        status.message = std::move(text);
        return status;
    }
};

}  // namespace netdisk

#endif
