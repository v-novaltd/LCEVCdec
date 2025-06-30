/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#include "string_format.h"

#include <gtest/gtest.h>
#include <LCEVC/common/diagnostics.h>

class FormatParseTest : public ::testing::Test
{
protected:
    LdcFormatParser parser;
    LdcFormatElement element;

    void SetUp() override
    {
        memset(&parser, 0, sizeof(parser));
        memset(&element, 0, sizeof(element));
    }

    // Helper to convert format element to string (not null-terminated)
    std::string ElementString(const LdcFormatElement& el)
    {
        return std::string(el.start, el.length);
    }
};

TEST_F(FormatParseTest, PlainTextOnly)
{
    ldcFormatParseInitialise(&parser, "LCEVC FTW!");
    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "LCEVC FTW!");
    EXPECT_EQ(element.type, LdcDiagArgNone);
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);
    EXPECT_FALSE(ldcFormatParseNext(&parser, &element));
}

TEST_F(FormatParseTest, MultipleSpecifiersMixedWithText)
{
    ldcFormatParseInitialise(&parser, "int:%d, string:%s, hex:%08x");

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "int:");
    EXPECT_EQ(element.type, LdcDiagArgNone);
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "%d");
    EXPECT_EQ(element.type, LdcDiagArgInt32);
    EXPECT_EQ(element.argumentCount, 1);
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), ", string:");
    EXPECT_EQ(element.type, LdcDiagArgNone);
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "%s");
    EXPECT_EQ(element.type, LdcDiagArgConstCharPtr);
    EXPECT_EQ(element.argumentCount, 1);
    EXPECT_EQ(element.argumentIndex, 1);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), ", hex:");
    EXPECT_EQ(element.type, LdcDiagArgNone);
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "%08x");
    EXPECT_EQ(element.type, LdcDiagArgUInt32);
    EXPECT_EQ(element.argumentCount, 1);
    EXPECT_EQ(element.argumentIndex, 2);

    EXPECT_FALSE(ldcFormatParseNext(&parser, &element));
}

TEST_F(FormatParseTest, PointerAndChar)
{
    ldcFormatParseInitialise(&parser, "pointer:%p, char:%c\n");

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "pointer:");
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "%p");
    EXPECT_EQ(element.type, LdcDiagArgConstVoidPtr);
    EXPECT_EQ(element.argumentCount, 1);
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), ", char:");
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "%c");
    EXPECT_EQ(element.type, LdcDiagArgChar);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "\n");
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);

    EXPECT_FALSE(ldcFormatParseNext(&parser, &element));
}

TEST_F(FormatParseTest, LiteralPercent)
{
    ldcFormatParseInitialise(&parser, "We are 100%% done\r");

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "We are 100");
    EXPECT_EQ(element.type, LdcDiagArgNone);
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "%");
    EXPECT_EQ(element.type, LdcDiagArgNone);
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), " done\r");
    EXPECT_FALSE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);
}

TEST_F(FormatParseTest, SignedLengths)
{
    ldcFormatParseInitialise(&parser, "%hhd %hd %d %ld %lld %zd");

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(element.type, LdcDiagArgInt8);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(element.type, LdcDiagArgInt16);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(element.type, LdcDiagArgInt32);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    if (sizeof(long) == 4) {
        EXPECT_EQ(element.type, LdcDiagArgInt32);
    } else if (sizeof(long) == 8) {
        EXPECT_EQ(element.type, LdcDiagArgInt64);
    } else {
        // Unexpected size for long!
        FAIL();
    }
    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(element.type, LdcDiagArgInt64);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    if (sizeof(size_t) == 4) {
        EXPECT_EQ(element.type, LdcDiagArgInt32);
    } else if (sizeof(size_t) == 8) {
        EXPECT_EQ(element.type, LdcDiagArgInt64);
    } else {
        // Unexpected size for size_t!
        FAIL();
    }

    ASSERT_FALSE(ldcFormatParseNext(&parser, &element));
}

TEST_F(FormatParseTest, UnsignedLengths)
{
    ldcFormatParseInitialise(&parser, "%hhu %hx %u %lo %llX %zu");

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(element.type, LdcDiagArgUInt8);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(element.type, LdcDiagArgUInt16);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(element.type, LdcDiagArgUInt32);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    if (sizeof(unsigned long) == 4) {
        EXPECT_EQ(element.type, LdcDiagArgUInt32);
    } else if (sizeof(long) == 8) {
        EXPECT_EQ(element.type, LdcDiagArgUInt64);
    } else {
        // Unexpected size for unsigned long!
        FAIL();
    }
    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(element.type, LdcDiagArgUInt64);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));

    if (sizeof(size_t) == 4) {
        EXPECT_EQ(element.type, LdcDiagArgUInt32);
    } else if (sizeof(size_t) == 8) {
        EXPECT_EQ(element.type, LdcDiagArgUInt64);
    } else {
        // Unexpected size for size_t!
        FAIL();
    }
    ASSERT_FALSE(ldcFormatParseNext(&parser, &element));
}

TEST_F(FormatParseTest, WidthAndPrecisionWithStars)
{
    ldcFormatParseInitialise(&parser, "Floats f:%*.*f e:%10.*e G:%*.3G Unsigned:%*u\t\n");

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "Floats f:");
    EXPECT_EQ(element.type, LdcDiagArgNone);
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "%*.*f");
    EXPECT_EQ(element.type, LdcDiagArgFloat64);
    EXPECT_EQ(element.argumentCount, 3); // width + precision + value
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), " e:");
    EXPECT_EQ(element.type, LdcDiagArgNone);
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "%10.*e");
    EXPECT_EQ(element.type, LdcDiagArgFloat64);
    EXPECT_EQ(element.argumentCount, 2); // precision + value
    EXPECT_EQ(element.argumentIndex, 3);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), " G:");
    EXPECT_EQ(element.type, LdcDiagArgNone);
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "%*.3G");
    EXPECT_EQ(element.type, LdcDiagArgFloat64);
    EXPECT_EQ(element.argumentCount, 2); // width + value
    EXPECT_EQ(element.argumentIndex, 5);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), " Unsigned:");
    EXPECT_EQ(element.type, LdcDiagArgNone);
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "%*u");
    EXPECT_EQ(element.type, LdcDiagArgUInt32);
    EXPECT_EQ(element.argumentCount, 2); // width + value
    EXPECT_EQ(element.argumentIndex, 7);

    ASSERT_TRUE(ldcFormatParseNext(&parser, &element));
    EXPECT_EQ(ElementString(element), "\t\n");
    EXPECT_EQ(element.type, LdcDiagArgNone);
    EXPECT_EQ(element.argumentCount, 0);
    EXPECT_EQ(element.argumentIndex, 0);

    EXPECT_FALSE(ldcFormatParseNext(&parser, &element));
}

TEST(LdcFormatTest, FormatIntegers)
{
    char buffer[128];
    const char* format = "int:%d,%+i,%-10i unsigned:%u hex:%x,%04X,%#010x\n";

    LdcDiagArg types[] = {LdcDiagArgInt32,  LdcDiagArgInt32,  LdcDiagArgInt32, LdcDiagArgUInt32,
                          LdcDiagArgUInt32, LdcDiagArgUInt32, LdcDiagArgUInt32};

    LdcDiagValue values[7];
    values[0].valueInt32 = -42;
    values[1].valueUInt32 = 213456;
    values[2].valueUInt32 = 808;
    values[3].valueUInt32 = 42;
    values[4].valueUInt32 = 0x2A;
    values[5].valueUInt32 = 0xDAD;
    values[6].valueUInt32 = 0xFACE;

    size_t written = ldcFormat(buffer, sizeof(buffer), format, types, values, 7);
    EXPECT_EQ(written, 62);
    EXPECT_EQ(std::string(buffer),
              "int:-42,+213456,808        unsigned:42 hex:2a,0DAD,0x0000face\n");
}

TEST(LdcFormatTest, FormatFloat)
{
    char buffer[128];
    const char* format = "float:%.2f %10.2e %g";

    LdcDiagArg types[3] = {LdcDiagArgFloat64, LdcDiagArgFloat64, LdcDiagArgFloat64};

    LdcDiagValue values[3];
    values[0].valueFloat64 = 3.14159;
    values[1].valueFloat64 = 10e6;
    values[2].valueFloat64 = 34456.7;

    size_t written = ldcFormat(buffer, sizeof(buffer), format, types, values, 3);
    EXPECT_EQ(written, 29);
    EXPECT_EQ(std::string(buffer), "float:3.14   1.00e+07 34456.7");
}

TEST(LdcFormatTest, FormatStringAndChar)
{
    char buffer[128];
    const char* format = "string:%s,'%10.8s' char:%c";

    LdcDiagArg types[3] = {LdcDiagArgConstCharPtr, LdcDiagArgConstCharPtr, LdcDiagArgChar};

    LdcDiagValue values[3];
    values[0].valueConstCharPtr = "Testing";
    values[1].valueConstCharPtr = "ALongStringLongLong";
    values[2].valueChar = 'x';

    size_t written = ldcFormat(buffer, sizeof(buffer), format, types, values, 3);
    EXPECT_EQ(written, 34);
    EXPECT_EQ(std::string(buffer), "string:Testing,'  ALongStr' char:x");
}

TEST(LdcFormatTest, FormatPointer)
{
    char buffer[128];
    int dummy = 42;
    const char* format = "pointer:%p";

    LdcDiagArg types[1] = {LdcDiagArgConstVoidPtr};

    LdcDiagValue values[1];
    values[0].valueConstVoidPtr = &dummy;

    size_t written = ldcFormat(buffer, sizeof(buffer), format, types, values, 1);
    std::string output = std::string(buffer);
    EXPECT_EQ(written, output.length());

    snprintf(buffer, sizeof(buffer), format, &dummy);
    EXPECT_EQ(std::string(buffer), output);
}

TEST(LdcFormatTest, FormatWithLiteralPercent)
{
    char buffer[128];
    const char* format = "Progress: 100%% done";

    LdcDiagArg types[1] = {};
    LdcDiagValue values[1] = {};

    size_t written = ldcFormat(buffer, sizeof(buffer), format, types, values, 0);
    EXPECT_EQ(written, 19);
    EXPECT_EQ(std::string(buffer), "Progress: 100% done");
}

TEST(LdcFormatTest, FormatWithWidthAndPrecisionStar)
{
    char buffer[128];
    const char* format = "value:%*.*f";

    LdcDiagArg types[3] = {
        LdcDiagArgInt32,  // width
        LdcDiagArgInt32,  // precision
        LdcDiagArgFloat64 // value
    };

    LdcDiagValue values[3];
    values[0].valueInt32 = 8;
    values[1].valueInt32 = 3;
    values[2].valueFloat64 = 1.234567;

    size_t written = ldcFormat(buffer, sizeof(buffer), format, types, values, 3);
    EXPECT_EQ(written, 14);
    EXPECT_EQ(std::string(buffer), "value:   1.235");
}

TEST(LdcFormatTest, FormatWithWidthStar)
{
    char buffer[128];
    const char* format = "value:%*.2f";

    LdcDiagArg types[2] = {
        LdcDiagArgInt32,  // width
        LdcDiagArgFloat64 // value
    };

    LdcDiagValue values[2];
    values[0].valueInt32 = 8;
    values[1].valueFloat64 = 1.234567;

    size_t written = ldcFormat(buffer, sizeof(buffer), format, types, values, 2);
    EXPECT_EQ(written, 14);
    EXPECT_EQ(std::string(buffer), "value:    1.23");
}

TEST(LdcFormatTest, FormatWithPrecisionStar)
{
    char buffer[128];
    const char* format = "value: %6.*f";

    LdcDiagArg types[2] = {
        LdcDiagArgInt32,  // precision
        LdcDiagArgFloat64 // value
    };

    LdcDiagValue values[2];
    values[0].valueInt32 = 4;
    values[1].valueFloat64 = 1.234567;

    size_t written = ldcFormat(buffer, sizeof(buffer), format, types, values, 3);
    EXPECT_EQ(written, 13);
    EXPECT_EQ(std::string(buffer), "value: 1.2346");
}

TEST(LdcFormatTest, FormatTruncatedOutput)
{
    char buffer[10];
    const char* format = "Long string: %s";

    LdcDiagArg types[1] = {LdcDiagArgConstCharPtr};

    LdcDiagValue values[1];
    values[0].valueConstCharPtr = "abcdefghijklmnop";

    size_t written = ldcFormat(buffer, sizeof(buffer), format, types, values, 1);
    EXPECT_LT(written, sizeof(buffer)); // Truncated
    EXPECT_EQ(buffer[written], '\0');   // Null terminated
}
