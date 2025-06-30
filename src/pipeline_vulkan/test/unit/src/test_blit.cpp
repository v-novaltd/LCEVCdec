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

class PipelineVulkanBlitFixture : public testing::Test
{
public:
    PipelineVulkanBlitFixture() noexcept {};

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
};

TEST_F(PipelineVulkanBlitFixture, AddTemporalToOutputPicture)
{
    auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());

    constexpr auto width = 1920;
    constexpr auto height = 1080;

    const LdpPictureDesc srcDesc{width, height, LdpColorFormatI420_16_LE};
    auto* src = static_cast<PictureVulkan*>(pipeline->allocPictureManaged(srcDesc));
    auto* srcBuffer = static_cast<BufferVulkan*>(src->buffer);
    auto srcData = vulkan_test_util::generateYUV420FromFixedSeed<uint16_t>(width, height);
    std::memcpy(srcBuffer->ptr(), srcData.data(), srcData.size() * sizeof(srcData[0]));

    const LdpPictureDesc dstDesc{width, height, LdpColorFormatI420_16_LE};
    auto* dst = static_cast<PictureVulkan*>(pipeline->allocPictureManaged(dstDesc));
    auto* dstBuffer = static_cast<BufferVulkan*>(dst->buffer);
    auto dstData = vulkan_test_util::generateYUV420FromFixedSeed<uint16_t>(width, height);
    std::memcpy(dstBuffer->ptr(), dstData.data(), dstData.size() * sizeof(dstData[0]));

    VulkanBlitArgs args{};
    args.src = src;
    args.dst = dst;

    EXPECT_TRUE(pipeline->blit(&args));

    const std::string hash = vulkan_test_util::hashMd5(dstBuffer->ptr(), dstBuffer->size());
    EXPECT_EQ(hash, "6ab4deece6fa070dd0bced3535c67ff4");
}
