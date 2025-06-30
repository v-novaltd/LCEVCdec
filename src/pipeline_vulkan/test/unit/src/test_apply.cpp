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

class PipelineVulkanApplyFixture : public testing::Test
{
public:
    PipelineVulkanApplyFixture() noexcept {};

    std::unique_ptr<Pipeline> mPipeline;

    LdcMemoryAllocator* allocator{};
    LdeCmdBufferGpu cmdBuffer{};
    LdeCmdBufferGpuBuilder cmdBufferBuilder{};

    const void incrementResiduals(int16_t* residuals, const uint32_t layerCount)
    {
        const int16_t newVal = residuals[0] + 1;
        for (uint32_t layer = 0; layer < layerCount; layer++) {
            residuals[layer] = newVal;
        }
    }

    void SetUp() override
    {
        allocator = ldcMemoryAllocatorMalloc();
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

    void makeCommandBuffer()
    {
        EXPECT_EQ(ldeCmdBufferGpuInitialize(allocator, &cmdBuffer, &cmdBufferBuilder), true);
        static const uint32_t kLayerCount = 16;
        int16_t residuals[kLayerCount] = {0};
        ldeCmdBufferGpuReset(&cmdBuffer, &cmdBufferBuilder, kLayerCount);

        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOClearAndSet, residuals, 0));

        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOAdd, residuals, 5));

        incrementResiduals(residuals, kLayerCount); // 1
        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOAdd, residuals, 63));

        incrementResiduals(residuals, kLayerCount); // 2
        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOSet, residuals, 2));

        incrementResiduals(residuals, kLayerCount); // 3
        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOAdd, residuals, 64));

        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOClearAndSet, residuals, 128));

        ASSERT_TRUE(ldeCmdBufferGpuAppend(&cmdBuffer, &cmdBufferBuilder, CBGOSetZero, residuals, 2038));

        ASSERT_TRUE(ldeCmdBufferGpuBuild(&cmdBuffer, &cmdBufferBuilder));
    }
};

TEST_F(PipelineVulkanApplyFixture, ApplyGpuCommandBufferToTemporal)
{
    makeCommandBuffer();

    auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());

    constexpr auto width = 1920;
    constexpr auto height = 1080;

    VulkanApplyArgs args{};
    args.plane = nullptr;
    args.planeWidth = width;
    args.planeHeight = height;
    args.bufferGpu = cmdBuffer;
    args.temporalRefresh = false;
    args.highlightResiduals = false;

    EXPECT_TRUE(pipeline->apply(&args));

    auto* temporal = pipeline->getTemporalPicture();
    auto* temporalBuffer = static_cast<BufferVulkan*>(temporal->buffer);

    const std::string hash = vulkan_test_util::hashMd5(temporalBuffer->ptr(), temporalBuffer->size());
    EXPECT_EQ(hash, "039f0cce21e8139795ad9b57100c7d45");
}

TEST_F(PipelineVulkanApplyFixture, ApplyGpuCommandBufferToPlane)
{
    makeCommandBuffer();

    auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());

    constexpr auto width = 1920;
    constexpr auto height = 1080;

    const LdpPictureDesc srcDesc{width, height, LdpColorFormatI420_16_LE};
    auto* src = static_cast<PictureVulkan*>(pipeline->allocPictureManaged(srcDesc));
    auto* srcBuffer = static_cast<BufferVulkan*>(src->buffer);
    const auto data = vulkan_test_util::generateYUV420FromFixedSeed<int16_t>(width, height);
    std::memcpy(srcBuffer->ptr(), data.data(), data.size() * sizeof(data[0]));

    VulkanApplyArgs args{};
    args.plane = src;
    args.planeWidth = width;
    args.planeHeight = height;
    args.bufferGpu = cmdBuffer;
    args.temporalRefresh = false;
    args.highlightResiduals = false;

    EXPECT_TRUE(pipeline->apply(&args));

    const std::string hash = vulkan_test_util::hashMd5(srcBuffer->ptr(), srcBuffer->size());
    EXPECT_EQ(hash, "2026379c4a0a0aef687b65de565553b4");
}
