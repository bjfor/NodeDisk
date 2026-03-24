#ifndef SYNC_HASH_H
#define SYNC_HASH_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace sync {

class IHasher {
public:
    virtual ~IHasher() = default;
    virtual std::unique_ptr<IHasher> Clone() const = 0;
    virtual void Reset() = 0;
    virtual void Update(const std::uint8_t *data, std::size_t size) = 0;
    virtual std::string FinalHex() = 0;
};

// Phase 1 keeps hashing abstract so we can swap MD5 for BLAKE3 later
// without changing the chunker, scanner, or sync writer contracts.
class Md5Hasher : public IHasher {
public:
    Md5Hasher();
    std::unique_ptr<IHasher> Clone() const override;
    void Reset() override;
    void Update(const std::uint8_t *data, std::size_t size) override;
    std::string FinalHex() override;

private:
    struct Impl;
    Impl *impl_;
};

}  // namespace sync

#endif
