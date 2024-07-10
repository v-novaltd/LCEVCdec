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

#include "LCEVC/utility/extract.h"

#include <gtest/gtest.h>

#include <array>
#include <memory>

TEST(Extract, ExtractFailures)
{
    std::array<uint8_t, 100> output = {0};
    uint32_t outputSize = 0;

    // Have some LCEVC data
    std::vector<uint8_t> nalu = {0x00, 0x00, 0x01, 0x06, 0x04, 0x0b, 0xb4, 0x00, 0x50, 0x00,
                                 'p',  'a',  'y',  'l',  'o',  'a',  'd',  0x00, 0x00, 0x01};
    // No output store
    {
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264, nullptr,
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  -1);
    }
    // bad output store size
    {
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(), 0, &outputSize),
                  -1);
    }
    // No output size store
    {
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), nullptr),
                  -1);
    }
}

TEST(Extract, ExtractEmpty)
{
    std::array<uint8_t, 100> output = {0};
    uint32_t outputSize = 0;

    // Empty NAL
    {
        std::vector<uint8_t> nalu;
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  0);
        EXPECT_EQ(outputSize, 0);
    }
    // No NAL data
    {
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nullptr, 0, LCEVC_NALFormat_AnnexB,
                                                  LCEVC_CodecType_H264, output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  0);
        EXPECT_EQ(outputSize, 0);
    }
    // 0 length NAL
    {
        std::vector<uint8_t> nalu = {0, 0, 0, 0};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), 0, LCEVC_NALFormat_AnnexB,
                                                  LCEVC_CodecType_H264, output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  0);
        EXPECT_EQ(outputSize, 0);
    }
}

TEST(Extract, ExtractH264_SEI)
{
    std::array<uint8_t, 100> output = {0};
    uint32_t outputSize = 0;

    // Simple NAL
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x01, 'p',  'a', 'y',
                                     'l',  'o',  'a',  'd',  0x00, 0x00, 0x01};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  0);
        EXPECT_EQ(outputSize, 0);
    }
    // NOTE: the minimum payload size for LCEVC is 8 bytes NOT the 7 bytes used here
    // LCEVC SEI
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x01, 0x06, 0x04, 0x0b, 0xb4, 0x00, 0x50, 0x00,
                                     'p',  'a',  'y',  'l',  'o',  'a',  'd',  0x00, 0x00, 0x01};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
    // LCEVC SEI 4 byte start
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x06, 0x04, 0x0b,
                                     0xb4, 0x00, 0x50, 0x00, 'p',  'a',  'y',
                                     'l',  'o',  'a',  'd',  0x00, 0x00, 0x01};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
    // LCEVC SEI + prefix
    {
        std::vector<uint8_t> nalu = {0xaa, 0x55, 0x00, 0x00, 0x01, 0x06, 0x04, 0x0b,
                                     0xb4, 0x00, 0x50, 0x00, 'p',  'a',  'y',  'l',
                                     'o',  'a',  'd',  0x00, 0x00, 0x01};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
    // LCEVC SEI long
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x06, 0x04, 0x0b, 0xb4,
                                     0x00, 0x50, 0x00, 'p',  'a',  'y',  'l',  'o',
                                     'a',  'd',  0xab, 0x00, 0x00, 0x00, 0x01};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
    // LCEVC SEI no following start code
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x06, 0x04, 0x0b, 0xb4, 0x00,
                                     0x50, 0x00, 'p',  'a',  'y',  'l',  'o',  'a',  'd'};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
    // LCEVC SEI w/ star code emulation prevention
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x01, 0x06, 0x04, 0x0e, 0xb4, 0x00,
                                     0x50, 0x00, 'p',  'a',  'y',  0x00, 0x00, 0x03,
                                     0x01, 'l',  'o',  'a',  'd',  0x00, 0x00, 0x01};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
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
        EXPECT_EQ(LCEVC_extractAndRemoveEnhancementFromNAL(
                      nalu.data(), static_cast<uint32_t>(nalu.size()), LCEVC_NALFormat_AnnexB,
                      LCEVC_CodecType_H264, nullptr, output.data(),
                      static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
}
