#include "netdisk/sync/hash.h"

#include <cstdio>

extern "C" {
#include "netdisk/common/md5.h"
}

namespace sync {

struct Md5Hasher::Impl {
    MD5_CTX ctx;
};

Md5Hasher::Md5Hasher() : impl_(new Impl) {
    Reset();
}

std::unique_ptr<IHasher> Md5Hasher::Clone() const {
    return std::make_unique<Md5Hasher>();
}

void Md5Hasher::Reset() {
    MD5Init(&impl_->ctx);
}

void Md5Hasher::Update(const std::uint8_t *data, std::size_t size) {
    MD5Update(&impl_->ctx, const_cast<unsigned char *>(reinterpret_cast<const unsigned char *>(data)),
              static_cast<unsigned int>(size));
}

std::string Md5Hasher::FinalHex() {
    unsigned char digest[16];
    MD5_CTX copy = impl_->ctx;
    MD5Final(&copy, digest);

    char hex[33] = {0};
    for (int i = 0; i < 16; ++i) {
        std::snprintf(hex + (i * 2), 3, "%02x", digest[i]);
    }
    return std::string(hex);
}

}  // namespace sync
