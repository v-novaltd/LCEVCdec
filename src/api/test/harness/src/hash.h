/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

// Abstract base class and factory for hash mechanisms.
//
#ifndef VN_LCEVC_TEST_HARNESS_HASH_H
#define VN_LCEVC_TEST_HARNESS_HASH_H

#include "LCEVC/lcevc_dec.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

// Hashing utility class
//
class Hash
{
public:
    Hash() = default;

    Hash(const Hash&) = delete;
    Hash& operator=(const Hash&) = delete;
    Hash(Hash&&) = delete;
    Hash& operator=(Hash&&) = delete;

    virtual ~Hash() = 0;
    virtual void update(const uint8_t* data, uint32_t size) = 0;
    virtual std::string hexDigest() = 0;

    void updatePicture(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture);
    void updatePlane(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture, uint32_t plane);
};

std::unique_ptr<Hash> createHash(std::string_view name);

// Helper class to encompass all hashes for a given test run
class Hashes
{
public:
    explicit Hashes(const std::string_view hashType)
        : m_hashType(hashType)
    {}
    ~Hashes();

    int initBaseAndHigh(std::string_view outputHashFilename);
    void initOpl(std::string_view outputOplFile);

    void updateBase(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle basePicture);
    void updateHigh(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle highPicture);
    void updateOpl(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle decodedPicture,
                   uint32_t frameCount, uint32_t width, uint32_t height);

private:
    const std::string_view m_hashType;
    std::unique_ptr<Hash> m_base;
    std::unique_ptr<Hash> m_high;
    std::unique_ptr<std::ofstream> m_fileOut;
    std::unique_ptr<std::ofstream> m_oplFileOut;
};

#endif
