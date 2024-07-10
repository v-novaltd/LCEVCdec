/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

// Abstract base class and factory for hash mechanisms.
//
#ifndef VN_LCEVC_TEST_HARNESS_HASH_H
#define VN_LCEVC_TEST_HARNESS_HASH_H

#include "LCEVC/lcevc_dec.h"

#include <memory>
#include <string>

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

#endif
