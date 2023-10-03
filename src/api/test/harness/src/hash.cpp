// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Hashing utility classes - wrappers around xxHash and MD5, along with a factory function
//
#include "hash.h"
//
#include "LCEVC/utility/md5.h"
#include "LCEVC/utility/picture_lock.h"
//
#include <fmt/core.h>
#include <xxhash.h>
//
#include <memory>
#include <string>
#include <vector>

using namespace lcevc_dec::utility;

//
Hash::~Hash() = default;

class HashXxHash final : public Hash
{
public:
    HashXxHash()
        : m_state(XXH64_createState())
    {
        XXH64_reset(m_state, 0);
    }
    ~HashXxHash() final { XXH64_freeState(m_state); }

    HashXxHash(const HashXxHash&) = delete;
    HashXxHash& operator=(const HashXxHash&) = delete;
    HashXxHash(HashXxHash&&) = delete;
    HashXxHash& operator=(HashXxHash&&) = delete;

    void update(const uint8_t* data, uint32_t size) final { XXH64_update(m_state, data, size); }

    std::string hexDigest() final { return fmt::format("{:016x}", XXH64_digest(m_state)); }

private:
    XXH64_state_t* m_state;
};

class HashMd5 final : public Hash
{
public:
    HashMd5() = default;
    ~HashMd5() final = default;

    HashMd5(const HashMd5&) = delete;
    HashMd5& operator=(const HashMd5&) = delete;
    HashMd5(HashMd5&&) = delete;
    HashMd5& operator=(HashMd5&&) = delete;

    void update(const uint8_t* data, uint32_t size) final { m_md5.update(data, size); }

    std::string hexDigest() final { return m_md5.hexDigest(); }

private:
    MD5 m_md5;
};

// Accumulate a hash of active parts of a picture
void Hash::updatePicture(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture)
{
    PictureLock lock(decoder, picture, LCEVC_Access_Read);

    for (uint32_t plane = 0; plane < lock.numPlanes(); ++plane) {
        for (uint32_t row = 0; row < lock.height(plane); ++row) {
            update(lock.rowData<uint8_t>(plane, row), lock.rowSize(plane));
        }
    }
}

// Accumulate a hash of a single plane of a picture
void Hash::updatePlane(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture, uint32_t plane)
{
    PictureLock lock(decoder, picture, LCEVC_Access_Read);
    assert(plane < lock.numPlanes());
    for (uint32_t row = 0; row < lock.height(plane); ++row) {
        update(lock.rowData<uint8_t>(plane, row), lock.rowSize(plane));
    }
}

std::unique_ptr<Hash> createHash(std::string_view name)
{
    if (name == "xxhash") {
        return std::make_unique<HashXxHash>();
    }
    if (name == "md5") {
        return std::make_unique<HashMd5>();
    }
    fmt::print("Unknown hash: {}\n", name);
    return nullptr;
}
