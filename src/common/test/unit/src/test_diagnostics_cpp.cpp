/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#include <fmt/core.h>
#include <gtest/gtest.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/log.h>

#include <climits>

extern "C" bool LCEVC_DiagHandlerOStream(void* user, const LdcDiagSite* site,
                                         const LdcDiagRecord* record, const LdcDiagValue* values);

class DiagnosticsTest : public testing::Test
{
public:
    DiagnosticsTest()
    { //
        ldcDiagnosticsHandlerPush(LCEVC_DiagHandlerOStream, &m_output);
        get();
    }

    ~DiagnosticsTest() override
    {
        //
        get();
        ldcDiagnosticsHandlerPop(LCEVC_DiagHandlerOStream, NULL);
    }

    DiagnosticsTest(const DiagnosticsTest&) = delete;
    DiagnosticsTest(DiagnosticsTest&&) = delete;
    void operator=(const DiagnosticsTest&) = delete;
    void operator=(DiagnosticsTest&&) = delete;

    // Clear current output string
    void clear() { m_output.str(""); }

    // Get current output string
    std::string output()
    {
        ldcDiagnosticsFlush();
        return m_output.str();
    }

    // Get current log string, and clear for next test
    std::string get()
    {
        ldcDiagnosticsFlush();
        std::string r = m_output.str();
        m_output.str("");
        return r;
    }

private:
    std::ostringstream m_output;
};

TEST_F(DiagnosticsTest, LogFormats)
{
    VNLogInfo("Test %c", 'a');
    EXPECT_EQ(get(), "Info: Test a\n");
    VNLogInfo("Test %d", 42);
    EXPECT_EQ(get(), "Info: Test 42\n");
    VNLogInfo("Test %d", 0);
    EXPECT_EQ(get(), "Info: Test 0\n");
    VNLogInfo("Test %d", INT_MAX);
    EXPECT_EQ(get(), "Info: Test 2147483647\n");
    VNLogInfo("Test %d", INT_MIN);
    EXPECT_EQ(get(), "Info: Test -2147483648\n");

    VNLogInfo("Test %u", 9769876);
    EXPECT_EQ(get(), "Info: Test 9769876\n");
    VNLogInfo("Test %u", 0);
    EXPECT_EQ(get(), "Info: Test 0\n");
    //    VNLogInfo("Test %u", UINT_MAX);
    //    EXPECT_EQ(get(), "Info: Test \n");

    VNLogInfo("Test %s", "string");
    EXPECT_EQ(get(), "Info: Test string\n");
}

TEST_F(DiagnosticsTest, LogFormatTypes)
{
#ifdef VN_SDK_LOG_ENABLE_INFO
    char c = 40;
    uint8_t u8 = 41;
    int8_t i8 = 42;
    uint16_t u16 = 43;
    int16_t i16 = 44;
    uint32_t u32 = 45;
    int32_t i32 = 46;
    uint64_t u64 = 47;
    int64_t i64 = 48;
#endif

    VNLogInfo("char %u %d %hu %hd %lu %ld", c, c, c, c, c, c);
    EXPECT_EQ(get(), "Info: char 40 40 40 40 40 40\n");
    VNLogInfo("u8 %u %d %hu %hd %lu %ld", u8, u8, u8, u8, u8, u8);
    EXPECT_EQ(get(), "Info: u8 41 41 41 41 41 41\n");
    VNLogInfo("i8 %u %d %hu %hd %lu %ld", i8, i8, i8, i8, i8, i8);
    EXPECT_EQ(get(), "Info: i8 42 42 42 42 42 42\n");
    VNLogInfo("u16 %u %d %hu %hd %lu %ld", u16, u16, u16, u16, u16, u16);
    EXPECT_EQ(get(), "Info: u16 43 43 43 43 43 43\n");
    VNLogInfo("i16 %u %d %hu %hd %lu %ld", i16, i16, i16, i16, i16, i16);
    EXPECT_EQ(get(), "Info: i16 44 44 44 44 44 44\n");
    VNLogInfo("u32 %u %d %hu %hd %lu %ld", u32, u32, u32, u32, u32, u32);
    EXPECT_EQ(get(), "Info: u32 45 45 45 45 45 45\n");
    VNLogInfo("i32 %u %d %hu %hd %lu %ld", i32, i32, i32, i32, i32, i32);
    EXPECT_EQ(get(), "Info: i32 46 46 46 46 46 46\n");
    VNLogInfo("u64 %u %d %hu %hd %lu %ld", u64, u64, u64, u64, u64, u64);
    EXPECT_EQ(get(), "Info: u64 47 47 47 47 47 47\n");
    VNLogInfo("i64 %u %d %hu %hd %lu %ld", i64, i64, i64, i64, i64, i64);
    EXPECT_EQ(get(), "Info: i64 48 48 48 48 48 48\n");
}

TEST_F(DiagnosticsTest, LogLevels)
{
    //    VNFatal("Test %s", "string");
    VNLogError("Level %d", 42);
    EXPECT_EQ(get(), "Error: Level 42\n");
    VNLogWarning("Level %d", 42);
    EXPECT_EQ(get(), "Warning: Level 42\n");
    VNLogInfo("Level %d", 42);
    EXPECT_EQ(get(), "Info: Level 42\n");
    VNLogDebug("Level %d", 42);
    EXPECT_EQ(get(), "Debug: Level 42\n");
    VNLogVerbose("Level %d", 42);
    EXPECT_EQ(get(), "Verbose: Level 42\n");
}

TEST_F(DiagnosticsTest, LogNumArguments)
{
    VNLogInfo("No argument");
    EXPECT_EQ(get(), "Info: No argument\n");
    VNLogInfo("1 argument: %d", 1);
    EXPECT_EQ(get(), "Info: 1 argument: 1\n");
    VNLogInfo("2 arguments: %d %d", 1, 2);
    EXPECT_EQ(get(), "Info: 2 arguments: 1 2\n");
    VNLogInfo("3 arguments: %d %d %d", 1, 2, 3);
    EXPECT_EQ(get(), "Info: 3 arguments: 1 2 3\n");
    VNLogInfo("4 arguments: %d %d %d %d", 1, 2, 3, 4);
    EXPECT_EQ(get(), "Info: 4 arguments: 1 2 3 4\n");
    VNLogInfo("5 arguments: %d %d %d %d %d", 1, 2, 3, 4, 5);
    EXPECT_EQ(get(), "Info: 5 arguments: 1 2 3 4 5\n");
    VNLogInfo("6 arguments: %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6);
    EXPECT_EQ(get(), "Info: 6 arguments: 1 2 3 4 5 6\n");
    VNLogInfo("7 arguments: %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7);
    EXPECT_EQ(get(), "Info: 7 arguments: 1 2 3 4 5 6 7\n");
    VNLogInfo("8 arguments: %d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7, 8);
    EXPECT_EQ(get(), "Info: 8 arguments: 1 2 3 4 5 6 7 8\n");
    VNLogInfo("9 arguments: %d %d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7, 8, 9);
    EXPECT_EQ(get(), "Info: 9 arguments: 1 2 3 4 5 6 7 8 9\n");
    VNLogInfo("10 arguments: %d %d %d %d %d %d %d %d %d %d", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    EXPECT_EQ(get(), "Info: 10 arguments: 1 2 3 4 5 6 7 8 9 10\n");
}

TEST_F(DiagnosticsTest, Formatted)
{
    VNLogF(LdcLogLevelError, "Formatted %d", 34);
    EXPECT_EQ(get(), "Error (formatted): Formatted 34\n");
}

static void function2()
{
    VNTraceScoped();
    VNLogInfo("Function 2");
}

static void function1()
{
    VNTraceScoped();

    VNLogInfo("Function 1");
    function2();
}

TEST_F(DiagnosticsTest, Scoped)
{
    function1();

    EXPECT_EQ(
        get(),
        "Trace: {\"ph\":\"B\", \"ts\":0.000, \"pid\":1, \"tid\":2, \"name\":\"function1\"},\n\n"
        "Info: Function 1\n"
        "Trace: {\"ph\":\"B\", \"ts\":0.000, \"pid\":1, \"tid\":2, \"name\":\"function2\"},\n\n"
        "Info: Function 2\n"
        "Trace: {\"ph\":\"E\", \"ts\":0.000, \"pid\":1, \"tid\":2},\n\n"
        "Trace: {\"ph\":\"E\", \"ts\":0.000, \"pid\":1, \"tid\":2},\n\n");
}

TEST_F(DiagnosticsTest, Metrics)
{
    int32_t i32 = 44;
    uint32_t u32 = 45;
    int64_t i64 = 46;
    uint64_t u64 = 47;
    float f32 = 3.1f;
    float f64 = 8.0;

    VNMetricInt32("i32", i32);
    VNMetricUInt32("u32", u32);
    VNMetricInt64("i64", i64);
    VNMetricUInt64("u64", u64);
    VNMetricFloat32("f32", f32);
    VNMetricFloat64("f64", f64);

    EXPECT_EQ(get(), "Metric: {\"ph\":\"C\", \"ts\":0.000, \"pid\":1, \"tid\":2, \"name\":\"i32\", "
                     "\"args\": { \"value\": 44}},\n\n"
                     "Metric: {\"ph\":\"C\", \"ts\":0.000, \"pid\":1, \"tid\":2, \"name\":\"u32\", "
                     "\"args\": { \"value\": 45}},\n\n"
                     "Metric: {\"ph\":\"C\", \"ts\":0.000, \"pid\":1, \"tid\":2, \"name\":\"i64\", "
                     "\"args\": { \"value\": 46}},\n\n"
                     "Metric: {\"ph\":\"C\", \"ts\":0.000, \"pid\":1, \"tid\":2, \"name\":\"u64\", "
                     "\"args\": { \"value\": 47}},\n\n"
                     "Metric: {\"ph\":\"C\", \"ts\":0.000, \"pid\":1, \"tid\":2, \"name\":\"f32\", "
                     "\"args\": { \"value\": 3.1}},\n\n"
                     "Metric: {\"ph\":\"C\", \"ts\":0.000, \"pid\":1, \"tid\":2, \"name\":\"f64\", "
                     "\"args\": { \"value\": 8}},\n\n");
}

extern "C" int diagnosticsTestCLog();
TEST_F(DiagnosticsTest, TestCLog) { EXPECT_TRUE(diagnosticsTestCLog()); }

extern "C" int diagnosticsTestCScope();
TEST_F(DiagnosticsTest, TestCScoped) { EXPECT_TRUE(diagnosticsTestCScope()); }

extern "C" int diagnosticsTestCMetrics();
TEST_F(DiagnosticsTest, TestCMetrics) { EXPECT_TRUE(diagnosticsTestCMetrics()); }
