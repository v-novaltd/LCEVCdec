// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "LCEVC/utility/md5.h"

#include "filesystem.h"
#include "string_utils.h"
#include "find_assets_dir.h"

#include <fmt/core.h>
#include <gtest/gtest.h>

#include <fstream>

using namespace lcevc_dec::utility;

const static filesystem::path kTestAssets{findAssetsDir("src/utility/test/assets")};

// MD5("") = d41d8cd98f00b204e9800998ecf8427e
TEST(MD5, Empty)
{
	MD5 md5;
	EXPECT_EQ(md5.hexDigest(), "d41d8cd98f00b204e9800998ecf8427e");

	uint8_t digest[16]{};
	md5.digest(digest);

	const uint8_t refDigest[16] = { 0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04, 0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e  };
	EXPECT_FALSE(memcmp(digest, refDigest, sizeof(refDigest)));
}

// MD5("The quick brown fox jumps over the lazy dog") = 9e107d9d372bb6826bd81d3542a419d6
TEST(MD5, Example1)
{
	unsigned char const data[] = "The quick brown fox jumps over the lazy dog";

	MD5 md5;
	md5.update(static_cast<const uint8_t *>(data), sizeof(data)-1);

	EXPECT_EQ(md5.hexDigest(), "9e107d9d372bb6826bd81d3542a419d6");
}

// MD5("The quick brown fox jumps over the lazy dog.") = e4d909c290d0fb1ca068ffaddf22cbd0
TEST(MD5, Example2)
{
	uint8_t const data[] = "The quick brown fox jumps over the lazy dog.";

	MD5 md5;
	md5.update(static_cast<const uint8_t *>(data), sizeof(data)-1);

	EXPECT_EQ(md5.hexDigest(), "e4d909c290d0fb1ca068ffaddf22cbd0");

}

// >>> import hashlib
// >>> hashlib.md5(bytearray(map( lambda x: x % 256, range(0,1000)))).hexdigest()
// 'cbecbdb0fdd5cec1e242493b6008cc79'
//
TEST(MD5, Blocks)
{
	uint8_t data[1000];
	for(uint32_t i =0; i < sizeof(data); ++i) {
		data[i] = i;
	}

	MD5 md5;
	md5.update(static_cast<const uint8_t *>(data), sizeof(data));

	EXPECT_EQ(md5.hexDigest(), "cbecbdb0fdd5cec1e242493b6008cc79");
}

// $ md5sum src/api/utility/test/assets/cactus_10frames.ts
// 87b94aae08cf5867fec8060c8e452f71  src/api/utility/test/assets/cactus_10frames.ts
// $
//
TEST(MD5, File)
{
	const std::string filePath{(kTestAssets / "cactus_10frames.ts").string()};
	const uint32_t fileSize = static_cast<uint32_t>(filesystem::file_size(filePath));
	std::vector<uint8_t> fileData(fileSize);

	std::ifstream file(filePath);
	ASSERT_TRUE(file.good());

	file.read(static_cast<char *>(static_cast<void *>(fileData.data())), fileSize);
	ASSERT_TRUE(file.good());

	MD5 md5;
	md5.update(fileData.data(), static_cast<uint32_t>(fileData.size()));

	EXPECT_EQ(md5.hexDigest(), "87b94aae08cf5867fec8060c8e452f71");
}
