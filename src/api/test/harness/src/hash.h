// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
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
