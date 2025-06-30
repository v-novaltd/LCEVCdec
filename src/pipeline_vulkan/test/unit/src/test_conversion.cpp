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

#include "test_utility.h"

#include <buffer_vulkan.h>
#include <fmt/core.h>
#include <gtest/gtest.h>
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/cpp_tools.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/pipeline/event_sink.h>
#include <LCEVC/pipeline/pipeline.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pipeline_vulkan/create_pipeline.h>
#include <picture_vulkan.h>
#include <pipeline_vulkan.h>

#include <memory>
#include <vector>

using namespace lcevc_dec::pipeline;
using namespace lcevc_dec::pipeline_vulkan;

class PipelineVulkanConversionFixture : public testing::Test
{
public:
    PipelineVulkanConversionFixture() noexcept {};

    std::unique_ptr<Pipeline> mPipeline;

    void SetUp() override
    {
        buildPipeline();

        auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());
        if (!pipeline) {
            GTEST_SKIP() << "Skipping test due to lack of Vulkan support";
        }
    }

    void TearDown() override {}

    void buildPipeline()
    {
        auto* pipelineBuilder =
            new lcevc_dec::pipeline_vulkan::PipelineBuilderVulkan(ldcMemoryAllocatorMalloc());
        ASSERT_TRUE(pipelineBuilder);

        EventSink* eventSink = nullptr;
        mPipeline = pipelineBuilder->finish(eventSink);

        delete pipelineBuilder;
    }

    template <typename T>
    void testConversionFromGenerated(uint32_t width, uint32_t height, LdpColorFormat srcColor,
                                     LdpColorFormat dstColor, bool toInternal,
                                     const std::string& expectedHash) const
    {
        const auto data = vulkan_test_util::generateYUV420FromFixedSeed<T>(width, height);
        testConversion(data, width, height, srcColor, dstColor, toInternal, expectedHash);
    }

    void testConversionWithAsset(const std::string& filename, uint32_t width, uint32_t height,
                                 LdpColorFormat srcColor, LdpColorFormat dstColor, bool toInternal,
                                 const std::string& expectedHash) const
    {
        const auto data = vulkan_test_util::readRaw(filename.c_str());
        testConversion(data, width, height, srcColor, dstColor, toInternal, expectedHash);
    }

    template <typename T>
    void testConversion(T data, uint32_t width, uint32_t height, LdpColorFormat srcColor,
                        LdpColorFormat dstColor, bool toInternal, const std::string& expectedHash) const
    {
        auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());

        const LdpPictureDesc srcDesc{width, height, srcColor};
        auto* src = static_cast<PictureVulkan*>(pipeline->allocPictureManaged(srcDesc));
        auto* srcBuffer = static_cast<BufferVulkan*>(src->buffer);
        std::memcpy(srcBuffer->ptr(), data.data(), data.size() * sizeof(data[0]));

        const LdpPictureDesc dstDesc{width, height, dstColor};
        auto* dst = static_cast<PictureVulkan*>(pipeline->allocPictureManaged(dstDesc));
        auto* dstBuffer = static_cast<BufferVulkan*>(dst->buffer);

        VulkanConversionArgs args{};
        args.src = src;
        args.dst = dst;
        args.toInternal = toInternal;

        EXPECT_TRUE(pipeline->conversion(&args));

        const std::string hash = vulkan_test_util::hashMd5(dstBuffer->ptr(), dstBuffer->size());
        EXPECT_EQ(hash, expectedHash);
    }
};

enum class TestType
{
    Uint8,
    Uint16,
};

struct ConversionTestParams
{
    int width;
    int height;
    LdpColorFormat srcFormat;
    LdpColorFormat dstFormat;
    bool toInternal;
    TestType type;
    std::string expectedHash;
};

class PipelineVulkanConversionParameterizedFixture
    : public PipelineVulkanConversionFixture
    , public ::testing::WithParamInterface<ConversionTestParams>
{};

TEST_P(PipelineVulkanConversionParameterizedFixture, ConvertToInternal)
{
    auto params = GetParam();

    switch (params.type) {
        case TestType::Uint8:
            testConversionFromGenerated<uint8_t>(params.width, params.height, params.srcFormat,
                                                 params.dstFormat, params.toInternal, params.expectedHash);
            break;
        case TestType::Uint16:
            testConversionFromGenerated<uint16_t>(params.width, params.height, params.srcFormat,
                                                  params.dstFormat, params.toInternal,
                                                  params.expectedHash);
            break;
    }
}

// TODO - more colour formats
INSTANTIATE_TEST_SUITE_P(
    ConversionTests, PipelineVulkanConversionParameterizedFixture,
    ::testing::Values(
        ConversionTestParams{960, 540, LdpColorFormatI420_8, LdpColorFormatI420_16_LE, true,
                             TestType::Uint8, "634ca3ac8ef6efd65226a45a38d32fd3"}, /* Convert8bitToInternal */
        ConversionTestParams{960, 540, LdpColorFormatI420_16_LE, LdpColorFormatI420_16_LE, true,
                             TestType::Uint16, "a72996a1bbc25a7b1393b37ab4b4674d"}, /* Convert10bitToInternal */
        ConversionTestParams{960, 540, LdpColorFormatNV12_8, LdpColorFormatI420_16_LE, true,
                             TestType::Uint8, "88b52dbc18eb4490bb8d8033189fd557"}, /* ConvertNv12ToInternal */
        ConversionTestParams{960, 540, LdpColorFormatI420_16_LE, LdpColorFormatI420_8, false, TestType::Uint16,
                             "76ad479964c5d14c1f36625cd2dbde74"}, /* ConvertFromInternalTo8bit */
        ConversionTestParams{960, 540, LdpColorFormatI420_16_LE, LdpColorFormatI420_16_LE, false,
                             TestType::Uint16, "01fd286216c1aaf8507a79e86d8fa972"}, /* ConvertFromInternalTo10bit */
        ConversionTestParams{960, 540, LdpColorFormatI420_16_LE, LdpColorFormatNV12_8, false, TestType::Uint16,
                             "e1041a891fb891a7d25fea57b1452ef1"} /* ConvertFromInternalToNv12 */
        ));
