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

#include "find_assets_dir.h"

#include <gtest/gtest.h>
#include <LCEVC/utility/base_decoder.h>
#include <LCEVC/utility/bin_reader.h>
#include <xxhash.h>

#include <filesystem>
#include <string>

namespace filesystem = std::filesystem;
using namespace lcevc_dec::utility;

const static filesystem::path kTestAssets{findAssetsDir("src/utility/test/assets")};

TEST(BaseDecoder, CreateH264TS)
{
    std::unique_ptr<BaseDecoder> decoder =
        createBaseDecoderLibAV((kTestAssets / "test_h264.ts").string());
    ASSERT_NE(decoder, nullptr);

    EXPECT_EQ(decoder->description().width, 176);
    EXPECT_EQ(decoder->description().height, 144);
    EXPECT_EQ(decoder->description().colorFormat, LCEVC_I420_8);
}

TEST(BaseDecoder, CreateH264ES)
{
    std::unique_ptr<BaseDecoder> decoder =
        createBaseDecoderLibAV((kTestAssets / "test_h264.es").string());
    EXPECT_TRUE(decoder);
}

TEST(BaseDecoder, BinMatchesLibAV)
{
    XXH64_hash_t hashImage[10] = {
        0xC6862D76E2ACEAF2, 0xA1DA0C7669C7E3BE, 0x4C6A7C469BEB2275, 0x4FFF6F2BAED35B87,
        0x4D074208805EFE99, 0x641173464C237272, 0x6C224942BE90A3E3, 0xA49057812B6D144C,
        0x6BAFE5896269E87F, 0xCCB0F6A969B06F5A,
    };

    std::unique_ptr<BinReader> binReader =
        createBinReader((kTestAssets / "cactus_10frames_lcevc.bin").string());
    EXPECT_TRUE(binReader);

    std::unique_ptr<BaseDecoder> decoder =
        createBaseDecoderLibAV((kTestAssets / "cactus_10frames.ts").string());
    EXPECT_TRUE(decoder);

    // Frame loop - consume data from base
    int frame = 0;

    while (decoder->update()) {
        if (decoder->hasEnhancement()) {
            // Get data from base
            BaseDecoder::Data enhancement = {};

            EXPECT_TRUE(decoder->getEnhancement(enhancement));

            // Get encoded enhancement data from bin file
            int64_t dts = 0;
            int64_t pts = 0;
            std::vector<uint8_t> payload;
            EXPECT_TRUE(binReader->read(dts, pts, payload));
            EXPECT_EQ(enhancement.size, payload.size());

            EXPECT_EQ(XXH64(enhancement.ptr, enhancement.size, 0),
                      XXH64(payload.data(), payload.size(), 0));

            decoder->clearEnhancement();
            EXPECT_FALSE(decoder->hasEnhancement());
        }

        if (decoder->hasImage()) {
            // Get image data from base
            BaseDecoder::Data image = {};

            EXPECT_TRUE(decoder->getImage(image));
            EXPECT_EQ(XXH64(image.ptr, image.size, 0), hashImage[frame]);

            decoder->clearImage();
            EXPECT_FALSE(decoder->hasImage());

            frame++;
        }
    }
}
