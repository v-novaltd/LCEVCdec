// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "LCEVC/utility/extract.h"

#include <gtest/gtest.h>

#include <array>
#include <memory>

extern "C" uint32_t extractEnhancementFromNAL(uint8_t* data, uint32_t dataSize,
                                              LCEVC_CodecType codecType, LCEVC_NALFormat nalFormat,
                                              uint8_t* outputData, uint32_t outputCapacity,
                                              uint32_t* outputSize, bool remove, uint32_t* newDataSize);

TEST(Extract, ExtractEmpty)
{
    std::array<uint8_t, 100> output = {0};
    uint32_t outputSize = 0;

    std::vector<uint8_t> nalu;

    EXPECT_EQ(extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                        LCEVC_CodecType_H264, LCEVC_NALFormat_AnnexB, output.data(),
                                        static_cast<uint32_t>(output.size()), &outputSize, false, nullptr),
              0);
}

TEST(Extract, ExtractH264_SEI)
{
    std::array<uint8_t, 100> output = {0};
    uint32_t outputSize = 0;

    // Simple NAL
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x01, 'p',  'a', 'y',
                                     'l',  'o',  'a',  'd',  0x00, 0x00, 0x01};
        EXPECT_EQ(extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                            LCEVC_CodecType_H264, LCEVC_NALFormat_AnnexB,
                                            output.data(), static_cast<uint32_t>(output.size()),
                                            &outputSize, false, nullptr),
                  0);
    }
    // LCEVC SEI
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x01, 0x06, 0x04, 0x0b, 0xb4, 0x00, 0x50, 0x00,
                                     'p',  'a',  'y',  'l',  'o',  'a',  'd',  0x00, 0x00, 0x01};
        EXPECT_EQ(extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                            LCEVC_CodecType_H264, LCEVC_NALFormat_AnnexB,
                                            output.data(), static_cast<uint32_t>(output.size()),
                                            &outputSize, false, nullptr),
                  7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
    // LCEVC SEI 4 byte start
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x06, 0x04, 0x0b,
                                     0xb4, 0x00, 0x50, 0x00, 'p',  'a',  'y',
                                     'l',  'o',  'a',  'd',  0x00, 0x00, 0x01};
        EXPECT_EQ(extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                            LCEVC_CodecType_H264, LCEVC_NALFormat_AnnexB,
                                            output.data(), static_cast<uint32_t>(output.size()),
                                            &outputSize, false, nullptr),
                  7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
    // LCEVC SEI + prefix
    {
        std::vector<uint8_t> nalu = {0xaa, 0x55, 0x00, 0x00, 0x01, 0x06, 0x04, 0x0b,
                                     0xb4, 0x00, 0x50, 0x00, 'p',  'a',  'y',  'l',
                                     'o',  'a',  'd',  0x00, 0x00, 0x01};
        EXPECT_EQ(extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                            LCEVC_CodecType_H264, LCEVC_NALFormat_AnnexB,
                                            output.data(), static_cast<uint32_t>(output.size()),
                                            &outputSize, false, nullptr),
                  7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
    // LCEVC SEI long
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x06, 0x04, 0x0b, 0xb4,
                                     0x00, 0x50, 0x00, 'p',  'a',  'y',  'l',  'o',
                                     'a',  'd',  0xab, 0x00, 0x00, 0x00, 0x01};
        EXPECT_EQ(extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                            LCEVC_CodecType_H264, LCEVC_NALFormat_AnnexB,
                                            output.data(), static_cast<uint32_t>(output.size()),
                                            &outputSize, false, nullptr),
                  7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
    // LCEVC SEI no following start code
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x06, 0x04, 0x0b, 0xb4, 0x00,
                                     0x50, 0x00, 'p',  'a',  'y',  'l',  'o',  'a',  'd'};
        EXPECT_EQ(extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                            LCEVC_CodecType_H264, LCEVC_NALFormat_AnnexB,
                                            output.data(), static_cast<uint32_t>(output.size()),
                                            &outputSize, false, nullptr),
                  7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
    // LCEVC SEI w/ star code emulation prevention
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x01, 0x06, 0x04, 0x0e, 0xb4, 0x00,
                                     0x50, 0x00, 'p',  'a',  'y',  0x00, 0x00, 0x03,
                                     0x01, 'l',  'o',  'a',  'd',  0x00, 0x00, 0x01};
        EXPECT_EQ(extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                            LCEVC_CodecType_H264, LCEVC_NALFormat_AnnexB,
                                            output.data(), static_cast<uint32_t>(output.size()),
                                            &outputSize, false, nullptr),
                  10);
        EXPECT_EQ(memcmp(output.data(), "pay\x00\x00\x01load", 10), 0);
        EXPECT_EQ(outputSize, 10);
    }
}

TEST(Extract, ExtractH264_Remove_SEI)
{
    std::array<uint8_t, 100> output = {0};
    uint32_t outputSize = 0;

    // LCEVC SEI long
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x06, 0x04, 0x0b, 0xb4,
                                     0x00, 0x50, 0x00, 'p',  'a',  'y',  'l',  'o',
                                     'a',  'd',  0xab, 0x00, 0x00, 0x00, 0x01};
        EXPECT_EQ(extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                            LCEVC_CodecType_H264, LCEVC_NALFormat_AnnexB,
                                            output.data(), static_cast<uint32_t>(output.size()),
                                            &outputSize, true, nullptr),
                  7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
}
