// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "LCEVC/utility/byte_order.h"

#include <gtest/gtest.h>

TEST(ByteOrder, ReadLittleEndian)
{
    {
        std::istringstream s("xxx");
        uint32_t v = 0;
        EXPECT_FALSE(lcevc_dec::utility::readLittleEndian(s, v));
    }
    {
        std::istringstream s("");
        uint32_t v = 0;
        EXPECT_FALSE(lcevc_dec::utility::readLittleEndian(s, v));
    }

    {
        std::istringstream s("0123");
        uint32_t v = 0;
        EXPECT_TRUE(lcevc_dec::utility::readLittleEndian(s, v) && v == 0x33323130);
    }

    {
        std::istringstream s("01234567");
        uint32_t v1 = 0;
        uint32_t v2 = 0;
        EXPECT_TRUE(lcevc_dec::utility::readLittleEndian(s, v1) && v1 == 0x33323130);
        EXPECT_TRUE(lcevc_dec::utility::readLittleEndian(s, v2) && v2 == 0x37363534);
    }

    {
        std::istringstream s("01234567");
        uint8_t v1 = 0;
        uint16_t v2 = 0;
        uint32_t v3 = 0;
        uint8_t v4 = 0;
        EXPECT_TRUE(lcevc_dec::utility::readLittleEndian(s, v1) && v1 == 0x30);
        EXPECT_TRUE(lcevc_dec::utility::readLittleEndian(s, v2) && v2 == 0x3231);
        EXPECT_TRUE(lcevc_dec::utility::readLittleEndian(s, v3) && v3 == 0x36353433);
        EXPECT_TRUE(lcevc_dec::utility::readLittleEndian(s, v4) && v4 == 0x37);
    }
}
TEST(ByteOrder, ReadBigEndian)
{
    {
        std::istringstream s("xxx");
        uint32_t v = 0;
        EXPECT_FALSE(lcevc_dec::utility::readBigEndian(s, v));
    }
    {
        std::istringstream s("");
        uint32_t v = 0;
        EXPECT_FALSE(lcevc_dec::utility::readBigEndian(s, v));
    }

    {
        std::istringstream s("0123");
        uint32_t v = 0;
        EXPECT_TRUE(lcevc_dec::utility::readBigEndian(s, v) && v == 0x30313233);
    }

    {
        std::istringstream s("01234567");
        uint32_t v1 = 0;
        uint32_t v2 = 0;
        EXPECT_TRUE(lcevc_dec::utility::readBigEndian(s, v1) && v1 == 0x30313233);
        EXPECT_TRUE(lcevc_dec::utility::readBigEndian(s, v2) && v2 == 0x34353637);
    }

    {
        std::istringstream s("01234567");
        uint8_t v1 = 0;
        uint16_t v2 = 0;
        uint32_t v3 = 0;
        uint8_t v4 = 0;
        EXPECT_TRUE(lcevc_dec::utility::readBigEndian(s, v1) && v1 == 0x30);
        EXPECT_TRUE(lcevc_dec::utility::readBigEndian(s, v2) && v2 == 0x3132);
        EXPECT_TRUE(lcevc_dec::utility::readBigEndian(s, v3) && v3 == 0x33343536);
        EXPECT_TRUE(lcevc_dec::utility::readBigEndian(s, v4) && v4 == 0x37);
    }
}