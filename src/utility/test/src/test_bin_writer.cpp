// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "bin_reader.h"
#include "bin_writer.h"
#include "filesystem.h"
#include "find_assets_dir.h"

#include <fmt/core.h>
#include <gtest/gtest.h>
#include <xxhash.h>

#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>

using namespace lcevc_dec::utility;

const static filesystem::path kTestAssets{findAssetsDir("src/utility/test/assets")};

TEST(BinWriter, DuplicateFile)
{
    const filesystem::path testFile = kTestAssets / "lcevcbin_24frames.bin";

    // Transcrive BIN file into a new output

    auto output = std::make_unique<std::ostringstream>();

    auto binReader = createBinReader(testFile.string());
    ASSERT_NE(binReader, nullptr);

    auto binWriter = createBinWriter(std::move(output));
    ASSERT_NE(binWriter, nullptr);

    for (unsigned int i = 0; i < 24; ++i) {
        int64_t decodeIndex = 0;
        int64_t presentationIndex = 0;
        std::vector<uint8_t> payload;
        EXPECT_TRUE(binReader->read(decodeIndex, presentationIndex, payload));
        EXPECT_TRUE(binWriter->write(decodeIndex, presentationIndex, payload.data(),
                                     static_cast<uint32_t>(payload.size())));
    }

    // Check output string matches original file

    // Fetch file into vector
    const uintmax_t fileSize = filesystem::file_size(testFile);
    std::vector<char> fileContents(fileSize);
    std::ifstream input(testFile, std::ios::binary);
    EXPECT_TRUE(!!input.read(fileContents.data(), fileContents.size()));

    // Dig string out of binwriter's stream
    auto* oss = dynamic_cast<std::ostringstream*>(binWriter->stream());
    EXPECT_NE(oss, nullptr);
    std::string str = oss->str();

    EXPECT_EQ(fileContents.size(), str.size());
    EXPECT_TRUE(!memcmp(fileContents.data(), str.data(), fileContents.size()));
}
