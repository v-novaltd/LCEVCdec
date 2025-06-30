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

#include <gtest/gtest.h>
#include <LCEVC/utility/extract.h>

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
    // Bad output store size
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

    // Non LCEVC NAL
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
    // LCEVC SEI Annex B prefix 3-byte
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
    // LCEVC SEI Annex B prefix 4-byte
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x06, 0x04, 0x0b, 0xb4,
                                     0x00, 0x50, 0x00, 'p',  'a',  'y',  'l',  'o',
                                     'a',  'd',  0x00, 0x00, 0x00, 0x01};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
    // LCEVC SEI Length prefix 4-byte
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x0e, 0x06, 0x04, 0x0b, 0xb4, 0x00,
                                     0x50, 0x00, 'p',  'a',  'y',  'l',  'o',  'a',  'd'};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_LengthPrefix,
                                                  LCEVC_CodecType_H264, output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
    // LCEVC SEI Length prefix 4-byte, following empty NAL
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x0e, 0x06, 0x04, 0x0b, 0xb4,
                                     0x00, 0x50, 0x00, 'p',  'a',  'y',  'l',  'o',
                                     'a',  'd',  0x00, 0x00, 0x00, 0x00};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_LengthPrefix,
                                                  LCEVC_CodecType_H264, output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
    // LCEVC SEI Annex B prefix 3-byte, offset
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
    // LCEVC SEI Annex B prefix 4-byte, tail
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
    // LCEVC SEI Annex B prefix 4-byte, no following start code
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
    // LCEVC SEI Annex B prefix 3-byte, w/ start code emulation prevention
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

    // LCEVC SEI Annex B prefix 4-byte, tail
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x06, 0x04, 0x0b, 0xb4,
                                     0x00, 0x50, 0x00, 'p',  'a',  'y',  'l',  'o',
                                     'a',  'd',  0xab, 0x00, 0x00, 0x00, 0x01};
        EXPECT_EQ(LCEVC_extractAndRemoveEnhancementFromNAL(
                      nalu.data(), static_cast<uint32_t>(nalu.size()), LCEVC_NALFormat_AnnexB,
                      LCEVC_CodecType_H264, output.data(), static_cast<uint32_t>(output.size()),
                      &outputSize, nullptr, nullptr),
                  1);
        EXPECT_EQ(outputSize, 7);
        EXPECT_EQ(memcmp(output.data(), "payload", 7), 0);
    }
}

TEST(Extract, ExtractH264_InterleavedNAL)
{
    std::array<uint8_t, 100> output = {0};
    uint32_t outputSize = 0;

    // LCEVC NAL Annex B prefix 3-byte
    {
        std::vector<uint8_t> nalu = {
            0x00, 0x00, 0x01, // start_code_prefix_one_3bytes
            0x79, // LCEVC: nal_unit_header: forbidden_zero_bit = 0b0, forbidden_one_bit = 0b1, nal_unit_type = (28) 0b11100, reserved_flag = 0b1
                  //  H264: nal_unit_header: forbidden_zero_bit = 0b0, nal_ref_idc = 0b11, nal_unit_type = (25) 0b11001
            'p', 'a', 'y', 'l', 'o', 'a', 'd', 0x00, 0x00, 0x01};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 11);
        EXPECT_EQ(memcmp(output.data(), "\x00\x00\x01\x79payload", 11), 0);
    }
    // LCEVC NAL Annex B prefix 3-byte, no following start code
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x01, 0x79, 'p', 'a', 'y', 'l', 'o', 'a', 'd'};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 11);
        EXPECT_EQ(memcmp(output.data(), "\x00\x00\x01\x79payload", 11), 0);
    }
    // LCEVC NAL Annex B prefix 4-byte
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, // start_code_prefix_one_4bytes
                                     0x79, 'p',  'a',  'y',  'l',  'o',
                                     'a',  'd',  0x00, 0x00, 0x00, 0x01};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 12);
        EXPECT_EQ(memcmp(output.data(), "\x00\x00\x00\x01\x79payload", 12), 0);
    }
    // LCEVC NAL Annex B prefix 4-byte, no following start code
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x79, 'p',
                                     'a',  'y',  'l',  'o',  'a',  'd'};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H264,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 12);
        EXPECT_EQ(memcmp(output.data(), "\x00\x00\x00\x01\x79payload", 12), 0);
    }
    // LCEVC NAL Length prefix 4-byte
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x08, // length_prefix
                                     0x79, 'p',  'a',  'y',  'l', 'o', 'a', 'd'};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_LengthPrefix,
                                                  LCEVC_CodecType_H264, output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 12);
        EXPECT_EQ(memcmp(output.data(), "\x00\x00\x00\x01\x79payload", 12), 0);
    }
    // LCEVC NAL Length prefix 4-byte, following empty NAL
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x08, 0x79, 'p',  'a',  'y',
                                     'l',  'o',  'a',  'd',  0x00, 0x00, 0x00, 0x00};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_LengthPrefix,
                                                  LCEVC_CodecType_H264, output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 12);
        EXPECT_EQ(memcmp(output.data(), "\x00\x00\x00\x01\x79payload", 12), 0);
    }
}

TEST(Extract, ExtractH265_InterleavedNAL)
{
    std::array<uint8_t, 100> output = {0};
    uint32_t outputSize = 0;

    // LCEVC NAL Annex B prefix 3-byte
    {
        std::vector<uint8_t> nalu = {
            0x00, 0x00, 0x01,
            0x79, // LCEVC: nal_unit_header: forbidden_zero_bit = 0b0, forbidden_one_bit = 0b1, nal_unit_type = (28) 0b11100, reserved_flag = 0b1
                  //  H265: nal_unit_header: forbidden_zero_bit = nal_unit_type = (60) 0b111100, nuh_layer_id msb (1) = 0b1
            'p', 'a', 'y', 'l', 'o', 'a', 'd', 0x00, 0x00, 0x01};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H265,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 11);
        EXPECT_EQ(memcmp(output.data(), "\x00\x00\x01\x79payload", 11), 0);
    }
    // LCEVC NAL Annex B prefix 3-byte, no following start code
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x01, 0x79, 'p', 'a', 'y', 'l', 'o', 'a', 'd'};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H265,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 11);
        EXPECT_EQ(memcmp(output.data(), "\x00\x00\x01\x79payload", 11), 0);
    }
    // LCEVC NAL Annex B prefix 4-byte
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x79, 'p',  'a',  'y',
                                     'l',  'o',  'a',  'd',  0x00, 0x00, 0x00, 0x01};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H265,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 12);
        EXPECT_EQ(memcmp(output.data(), "\x00\x00\x00\x01\x79payload", 12), 0);
    }
    // LCEVC NAL Annex B prefix 4-byte, no following start code
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01, 0x79, 'p',
                                     'a',  'y',  'l',  'o',  'a',  'd'};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_AnnexB, LCEVC_CodecType_H265,
                                                  output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 12);
        EXPECT_EQ(memcmp(output.data(), "\x00\x00\x00\x01\x79payload", 12), 0);
    }
    // LCEVC NAL Length prefix 4-byte
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x08, 0x79, 'p',
                                     'a',  'y',  'l',  'o',  'a',  'd'};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_LengthPrefix,
                                                  LCEVC_CodecType_H265, output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 12);
        EXPECT_EQ(memcmp(output.data(), "\x00\x00\x00\x01\x79payload", 12), 0);
    }
    // LCEVC NAL Length prefix 4-byte, following empty NAL
    {
        std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x08, 0x79, 'p',  'a',  'y',
                                     'l',  'o',  'a',  'd',  0x00, 0x00, 0x00, 0x00};
        EXPECT_EQ(LCEVC_extractEnhancementFromNAL(nalu.data(), static_cast<uint32_t>(nalu.size()),
                                                  LCEVC_NALFormat_LengthPrefix,
                                                  LCEVC_CodecType_H265, output.data(),
                                                  static_cast<uint32_t>(output.size()), &outputSize),
                  1);
        EXPECT_EQ(outputSize, 12);
        EXPECT_EQ(memcmp(output.data(), "\x00\x00\x00\x01\x79payload", 12), 0);
    }
}
