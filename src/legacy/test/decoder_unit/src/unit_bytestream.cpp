/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

#include <gtest/gtest.h>
#include <LCEVC/utility/byte_order.h>

#include <limits>

extern "C"
{
#include "common/bytestream.h"
}

// -----------------------------------------------------------------------------

bool operator==(const ByteStream_t& a, const ByteStream_t& b)
{
    return memcmp(&a, &b, sizeof(ByteStream_t)) == 0;
}

// -----------------------------------------------------------------------------

TEST(ByteStream, Initialize)
{
    uint8_t data;
    uint8_t data2;

    // Initialize stream with some dummy info.
    ByteStream_t stream = {};
    stream.size = 21;
    stream.data = &data2;
    stream.offset = 33;

    const ByteStream_t baselineStream = stream;

    // Null data is an error, struct is not modified.
    EXPECT_EQ(bytestreamInitialise(&stream, nullptr, 50), -1);
    EXPECT_EQ(stream, baselineStream);

    // Zero length is an error, struct is not modified.
    EXPECT_EQ(bytestreamInitialise(&stream, &data, 0), -1);
    EXPECT_EQ(stream, baselineStream);

    // Valid input.
    EXPECT_EQ(bytestreamInitialise(&stream, &data, 1), 0);
    EXPECT_EQ(stream.size, 1);
    EXPECT_EQ(stream.data, &data);
    EXPECT_EQ(stream.offset, 0);
}

// Performs several tests on the bytestreamReadXYZ functions to ensure they:
//    1. Read data correctly.
//    2. Fail appropriately.
//    3. Are non-modifying under error.
template <typename T, typename Fn>
void ByteStreamTestReadT(Fn readFunction)
{
    constexpr T kTestValue = 30;
    ByteStream_t stream = {};

    // Stream expects values to be in big-endian order.
    T data = lcevc_dec::utility::ByteOrder<T>::ToNetwork(kTestValue);
    T result;

    EXPECT_EQ(bytestreamInitialise(&stream, (const uint8_t*)&data, sizeof(data)), 0);

    // Read as success
    EXPECT_EQ(readFunction(&stream, &result), 0);

    // Value read is as expected.
    EXPECT_EQ(result, kTestValue);

    const ByteStream_t preFailureStream = stream;

    // Expecting nothing more to read.
    EXPECT_EQ(bytestreamRemaining(&stream), 0);

    // Expecting stream reading offset to be after read amount.
    EXPECT_EQ(stream.offset, sizeof(T));

    // Read as failure.
    EXPECT_EQ(readFunction(&stream, &result), -1);

    // Expecting failure to not modify stream state in any way.
    EXPECT_EQ(stream, preFailureStream);
}

TEST(ByteStream, ReadU64) { ByteStreamTestReadT<uint64_t>(&ldlBytestreamReadU64); }

TEST(ByteStream, ReadU32) { ByteStreamTestReadT<uint32_t>(&ldlBytestreamReadU32); }

TEST(ByteStream, ReadU16) { ByteStreamTestReadT<uint16_t>(&ldlBytestreamReadU16); }

TEST(ByteStream, ReadU8) { ByteStreamTestReadT<uint8_t>(&ldlBytestreamReadU8); }

std::vector<uint8_t> GenerateMultibyte(uint64_t value, uint32_t numBytes)
{
    std::vector<uint8_t> data(numBytes, 0);

    // MultiByte is in a big-endian like order.
    uint64_t encodeValue = value << ((numBytes - 1) * 7);

    for (uint32_t i = 0; i < numBytes; ++i) {
        auto valueData = static_cast<uint8_t>(encodeValue & 0x7f);
        encodeValue >>= 7;

        // Always generate overflow bit up to last desired byte.
        if (i < (numBytes - 1)) {
            valueData |= 0x80;
        }

        data[i] = valueData;
    }

    return data;
}

TEST(ByteStream, ReadMultiByteValid)
{
    constexpr uint32_t kTestRange = 16;
    constexpr uint32_t kMultiByteMaxBytes = 10;
    constexpr uint64_t kValue = 30;

    for (uint32_t i = 1; i < kTestRange; ++i) {
        // 63-bits for 9-bytes, 10th byte will be 1.
        const uint64_t testValue = (i == kMultiByteMaxBytes) ? 1 : kValue;

        const auto data = GenerateMultibyte(testValue, i);
        EXPECT_EQ(data.size(), i);

        ByteStream_t stream = {};
        EXPECT_EQ(bytestreamInitialise(&stream, data.data(), i), 0);

        uint64_t value = 0;

        if (i <= kMultiByteMaxBytes) {
            EXPECT_EQ(ldlBytestreamReadMultiByte(&stream, &value), 0);
            EXPECT_EQ(value, testValue);
        } else {
            EXPECT_EQ(ldlBytestreamReadMultiByte(&stream, &value), -1);
        }
    }
}

TEST(ByteStream, ReadMultiByteCorruption)
{
    uint64_t value = 0;
    ByteStream_t stream = {};

    // Invalidate offset and ensure read catches it.
    stream.offset = 5;
    EXPECT_EQ(ldlBytestreamReadMultiByte(&stream, &value), -1);

    // Read corrupted data.
    constexpr uint32_t kInvalidCount = 6;

    auto data = GenerateMultibyte(50, 8);
    data[kInvalidCount - 1] |= 0x80; // Set overflow bit.
    EXPECT_EQ(bytestreamInitialise(&stream, data.data(), kInvalidCount), 0);
    EXPECT_EQ(ldlBytestreamReadMultiByte(&stream, &value), -1);
}

TEST(ByteStream, SeekBothValidAndInvalid)
{
    constexpr uint32_t kLength = 30;
    constexpr uint8_t kValue = 5;
    std::vector<uint8_t> data(kLength * 2, kValue);

    ByteStream_t stream = {};
    EXPECT_EQ(bytestreamInitialise(&stream, data.data(), kLength * 2), 0);

    // Jump over kLength reads that are valid.
    EXPECT_EQ(ldlBytestreamSeek(&stream, kLength), 0);

    const ByteStream_t preFailureStream = stream;

    // Try to seek past the end of the data.
    EXPECT_EQ(ldlBytestreamSeek(&stream, kLength + 1), -1);
    EXPECT_EQ(stream, preFailureStream);

    constexpr uint32_t kMaxU32 = std::numeric_limits<uint32_t>::max();

    // Try to overflow seek where result will be exactly as current offset.
    EXPECT_EQ(ldlBytestreamSeek(&stream, kMaxU32), -1);
    EXPECT_EQ(stream, preFailureStream);

    // Try to seek and overflow where the resulting offset will be less than
    // current offset.
    EXPECT_EQ(ldlBytestreamSeek(&stream, kMaxU32 - 5), -1);
    EXPECT_EQ(stream, preFailureStream);
}

TEST(ByteStream, RemainingDataAndCurrent)
{
    constexpr uint32_t kLength = 30;
    constexpr uint8_t kValue = 5;
    std::vector<uint8_t> data(kLength, kValue);

    ByteStream_t stream = {};
    EXPECT_EQ(bytestreamInitialise(&stream, data.data(), kLength), 0);
    EXPECT_EQ(ldlBytestreamCurrent(&stream), data.data());

    // Perform expected reads and check remaining decrements accordingly.
    for (uint32_t i = 0; i < kLength; ++i) {
        uint8_t value = 0;
        EXPECT_EQ(ldlBytestreamReadU8(&stream, &value), 0);
        const auto remaining = static_cast<int32_t>(bytestreamRemaining(&stream));
        EXPECT_EQ(remaining, (kLength - (i + 1)));

        const uint8_t* expectedData = remaining ? (data.data() + i + 1) : nullptr;
        EXPECT_EQ(ldlBytestreamCurrent(&stream), expectedData);
        EXPECT_EQ(value, kValue);
    }

    // Expect no more data.
    EXPECT_EQ(bytestreamRemaining(&stream), 0);

    // Invalidate offset and check it is caught.
    stream.offset = kLength + 1;
    EXPECT_EQ(bytestreamRemaining(&stream), 0);
    EXPECT_EQ(ldlBytestreamCurrent(&stream), nullptr);
}

TEST(ByteStream, Size)
{
    ByteStream_t stream = {};
    uint32_t data = 0;

    EXPECT_EQ(bytestreamInitialise(&stream, (const uint8_t*)&data, sizeof(data)), 0);
    EXPECT_EQ(ldlByteStreamGetSize(&stream), sizeof(data));
}

// -----------------------------------------------------------------------------
