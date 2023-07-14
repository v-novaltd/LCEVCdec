/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include <gtest/gtest.h>

extern "C"
{
#include "common/bitstream.h"
}

// -----------------------------------------------------------------------------

bool operator==(const BitStream_t& a, const BitStream_t& b)
{
    return memcmp(&a, &b, sizeof(BitStream_t)) == 0;
}

// -----------------------------------------------------------------------------

TEST(BitStream, Initialize)
{
    BitStream_t stream = {};
    stream.nextBit = 5;
    stream.word = 30;

    uint8_t data;

    const BitStream_t baselineStream = stream;

    // Null data is an error, struct is not modified.
    EXPECT_EQ(bitstreamInitialise(&stream, nullptr, 50), -1);
    EXPECT_EQ(stream, baselineStream);

    // Zero length is an error, struct is not modified.
    EXPECT_EQ(bitstreamInitialise(&stream, &data, 0), -1);
    EXPECT_EQ(stream, baselineStream);

    // Valid input.
    EXPECT_EQ(bitstreamInitialise(&stream, &data, 1), 0);
}

TEST(BitStream, ReadBit)
{
    constexpr uint32_t numBytes = 8;
    constexpr uint32_t numBits = numBytes * 8;
    constexpr uint8_t data[numBytes] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};

    BitStream_t stream = {};

    EXPECT_EQ(bitstreamInitialise(&stream, data, numBytes), 0);

    // Try to read without passing in output parameter.
    EXPECT_EQ(bitstreamReadBit(&stream, nullptr), -1);

    // Read each nibble and ensure it is as expected, this will also perform
    // a new word load after 32nd bit to be read in.
    uint32_t nibble = 0;
    for (uint8_t i = 0; i < numBits; ++i) {
        if (i && ((i % 4) == 0)) {
            const uint8_t expected = (i - 1) >> 2;
            EXPECT_EQ(nibble, expected);
            nibble = 0;
        }

        uint8_t bit = 0;
        EXPECT_EQ(bitstreamReadBit(&stream, &bit), 0);
        nibble <<= 1;
        nibble |= bit;
    }

    // Try to read past end of stream.
    uint8_t bit;
    EXPECT_EQ(bitstreamReadBit(&stream, &bit), -1);
}

// -----------------------------------------------------------------------------