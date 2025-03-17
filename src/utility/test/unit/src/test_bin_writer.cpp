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

#include "bin_writer.h"
#include "find_assets_dir.h"
#include "LCEVC/utility/bin_reader.h"

#include <fmt/core.h>
#include <gtest/gtest.h>
#include <xxhash.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace filesystem = std::filesystem;
using namespace lcevc_dec::utility;

const static filesystem::path kTestAssets{findAssetsDir("src/utility/test/assets")};

TEST(BinWriter, DuplicateFile)
{
    const filesystem::path testFile = kTestAssets / "lcevcbin_24frames.bin";

    // Transcribe BIN file into a new output

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
