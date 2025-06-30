/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

// Hashing utility classes - wrappers around xxHash and MD5, along with a factory function
//
#include "hash.h"
//
#include <LCEVC/utility/check.h>
#include <LCEVC/utility/md5.h>
#include <LCEVC/utility/picture_lock.h>
//
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <xxhash.h>
//
#include <memory>

// ------------------------------------------------------------------------------------------------

using namespace lcevc_dec::utility;

// - Hash and child classes -----------------------------------------------------------------------

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

// - Hashes ---------------------------------------------------------------------------------------

Hashes::~Hashes()
{
    if (m_base && m_high && m_fileOut) {
        try {
            fmt::print(*m_fileOut, "{{\n    \"base\":\"{}\",\n    \"high\":\"{}\"\n}}\n",
                       m_base->hexDigest(), m_high->hexDigest());
        } catch (const std::exception&) {
            fmt::print(stderr, "Failed to write out hashes\n");
        }
    }
}

int Hashes::initBaseAndHigh(const std::string_view outputHashFilename)
{
    m_fileOut = std::make_unique<std::ofstream>(outputHashFilename.data());
    if (!m_fileOut->good()) {
        fmt::print("Could not open output hash {}\n", outputHashFilename);
        return EXIT_FAILURE;
    }

    m_base = createHash(m_hashType);
    m_high = createHash(m_hashType);
    return EXIT_SUCCESS;
}

void Hashes::initOpl(const std::string_view outputOplFile)
{
    m_oplFileOut = std::make_unique<std::ofstream>(outputOplFile.data());
    fmt::print(*m_oplFileOut, "PicOrderCntVal,pic_width_max,pic_height_max,md5_y,md5_u,md5_v\n");
}

void Hashes::updateBase(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle basePicture)
{
    if (m_base) {
        m_base->updatePicture(decoder, basePicture);
    }
}

void Hashes::updateHigh(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle highPicture)
{
    if (m_high) {
        m_high->updatePicture(decoder, highPicture);
    }
}

void Hashes::updateOpl(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle decodedPicture,
                       uint32_t frameCount, uint32_t width, uint32_t height)
{
    if (m_oplFileOut) {
        uint32_t planeCount{0};
        VN_LCEVC_CHECK(LCEVC_GetPicturePlaneCount(decoder, decodedPicture, &planeCount));
        fmt::print(*m_oplFileOut, "{},{},{}", frameCount, width, height);
        for (uint32_t plane = 0; plane < planeCount; ++plane) {
            auto hash = createHash(m_hashType);
            hash->updatePlane(decoder, decodedPicture, plane);
            fmt::print(*m_oplFileOut, ",{}", hash->hexDigest());
        }
        fmt::print(*m_oplFileOut, "\n");
    }
}
