// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "bin_reader.h"
#include "find_assets_dir.h"
#include "filesystem.h"

#include <fmt/core.h>
#include <gtest/gtest.h>
#include <xxhash.h>

#include <memory>

using namespace lcevc_dec::utility;

const static filesystem::path kTestAssets{findAssetsDir("src/utility/test/assets")};

TEST(BinReader, NoFile)
{
    auto binReader = createBinReader("does_not_exist.bin");
    EXPECT_EQ(binReader, nullptr);
}

TEST(BinReader, Open)
{
    auto binReader = createBinReader((kTestAssets / "lcevcbin_24frames.bin").string());
    EXPECT_NE(binReader, nullptr);
}

TEST(BinReader, ReadContents)
{
    auto binReader = createBinReader((kTestAssets / "lcevcbin_24frames.bin").string());
    ASSERT_NE(binReader, nullptr);

    static const int64_t ptsReference[20] = {
        133200, 151200, 140400, 136800, 144000, 147600, 158400, 154800, 162000, 180000,
        169200, 165600, 172800, 176400, 187200, 183600, 190800, 208800, 198000, 194400,
    };

    XXH64_hash_t hashReference[20] = {
        0xe84a9127e1e4bbce, 0x67d99d2ca0a09e77, 0xd480073ea8aa5b48, 0x62cf80cdc380991d,
        0xf1ad7659d00092bf, 0x2585d8c39f7f5996, 0x93f7ea0edf4ef520, 0x645b3021e062ffa9,
        0x2ad76017c60620ff, 0x8a9309a247ec237b, 0x78f69a37f95c6c78, 0x190f8ad98b469e28,
        0xa221b445f8e8ea3a, 0x17b2292df95c0682, 0x40f9da5300017315, 0x49543982d7740b82,
        0x801188baaef58819, 0x19bb3212bcc25225, 0xd02a8d88753854ff, 0xf1737151a3c0dbbb,
    };

    for (unsigned int i = 0; i < 20; ++i) {
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;
        std::vector<uint8_t> payload;

        EXPECT_TRUE(binReader->read(decodeIndex, presentationIndex, payload));
        EXPECT_EQ(decodeIndex, i * 3600 + 126000);
        EXPECT_EQ(presentationIndex, ptsReference[i]);
        EXPECT_EQ(XXH64(payload.data(), payload.size(), 0), hashReference[i]);
    }
}

TEST(BinReader, ReadToEOF)
{
    auto binReader = createBinReader((kTestAssets / "lcevcbin_24frames.bin").string());

    EXPECT_NE(binReader, nullptr);

    // Read whole video
    for (unsigned int i = 0; i < 24; ++i) {
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;
        std::vector<uint8_t> payload;

        EXPECT_TRUE(binReader->read(decodeIndex, presentationIndex, payload));
    }

    // Read off end
    {
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;
        std::vector<uint8_t> payload;

        EXPECT_FALSE(binReader->read(decodeIndex, presentationIndex, payload));
    }
}
