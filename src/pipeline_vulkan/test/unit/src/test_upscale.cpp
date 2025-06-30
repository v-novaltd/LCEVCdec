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

static constexpr int16_t kernelNearest[] = {0, 16384, 0, 0};
static constexpr int16_t kernelLinear[] = {0, 12288, 4096, 0};
static constexpr int16_t kernelCubic[] = {-1382, 14285, 3942, -461};
static constexpr int16_t kernelModifiedCubic[] = {-2360, 15855, 4165, -1276};

class PipelineVulkanUpscaleFixture : public testing::Test
{
public:
    PipelineVulkanUpscaleFixture() noexcept {};

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

    void testUpscaleFromGenerated(uint32_t width, uint32_t height, LdeScalingMode mode,
                                  bool vertical, bool applyPA, const int16_t upscaleKernel[],
                                  const std::string& expectedHash) const
    {
        const auto data = vulkan_test_util::generateYUV420FromFixedSeed<int16_t>(width, height);
        testUpscale(data, width, height, mode, vertical, applyPA, upscaleKernel, expectedHash);
    }

    template <typename T>
    void testUpscale(T data, uint32_t width, uint32_t height, LdeScalingMode mode, bool vertical,
                     bool applyPA, const int16_t upscaleKernel[], const std::string& expectedHash) const
    {
        auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());

        const LdpPictureDesc srcDesc{width, height, LdpColorFormatI420_16_LE};
        auto* src = static_cast<PictureVulkan*>(pipeline->allocPictureManaged(srcDesc));
        auto* srcBuffer = static_cast<BufferVulkan*>(src->buffer);
        std::memcpy(srcBuffer->ptr(), data.data(), data.size() * sizeof(data[0]));

        PictureVulkan* base = nullptr;
        if (mode == Scale1D && !vertical) { // make a base picture for PA in horizontal shader
            const LdpPictureDesc baseDesc{width, height / 2, LdpColorFormatI420_16_LE};
            base = static_cast<PictureVulkan*>(pipeline->allocPictureManaged(baseDesc));
            auto* baseBuffer = static_cast<BufferVulkan*>(base->buffer);
            std::memcpy(baseBuffer->ptr(), data.data(), data.size() * sizeof(data[0]) / 2);
        }

        const LdpPictureDesc dstDesc{2, 2, LdpColorFormatI420_8};
        auto* dst = static_cast<PictureVulkan*>(pipeline->allocPictureManaged(dstDesc));

        VulkanUpscaleArgs upscaleArgs{};
        upscaleArgs.src = src;
        upscaleArgs.dst = dst;
        upscaleArgs.base = base ? base : src;
        upscaleArgs.applyPA = applyPA;
        upscaleArgs.dither = nullptr;
        upscaleArgs.mode = mode;
        upscaleArgs.vertical = vertical;

        LdeKernel kernel{};
        for (int i = 0; i < 4; ++i) {
            kernel.coeffs[0][i] = upscaleKernel[i];
        }

        EXPECT_TRUE(pipeline->upscaleFrame(&kernel, &upscaleArgs));

        auto* dstBuffer = static_cast<BufferVulkan*>(dst->buffer);

        const std::string hash = vulkan_test_util::hashMd5(dstBuffer->ptr(), dstBuffer->size());
        EXPECT_EQ(hash, expectedHash);
    }
};

TEST_F(PipelineVulkanUpscaleFixture, Upscale1D)
{
    // vertical
    testUpscaleFromGenerated(960, 540, Scale1D, true, false, kernelLinear,
                             "169d5585a19a9d29ad17a85db2f8d7ea");

    // TODO - horizontal without PA
    // testUpscaleFromGenerated(960, 540, Scale1D, false, false, kernelLinear,
    //                         "e4edebf68fbd014c860ee8537e8f492d");

    // TODO - horizontal with PA
    // testUpscaleFromGenerated(960, 540, Scale1D, false, true, kernelLinear,
    //                         "69d8c4c57aa66b652b16325072b92adf");
}

TEST_F(PipelineVulkanUpscaleFixture, Upscale2D)
{
    // without PA
    testUpscaleFromGenerated(960, 540, Scale2D, false, false, kernelNearest,
                             "3f2cef016be867f62eb680d98945cd5c");
    testUpscaleFromGenerated(960, 540, Scale2D, false, false, kernelLinear,
                             "bf6c98b5df4f5a5ada4b84838bd3d6be");
    testUpscaleFromGenerated(960, 540, Scale2D, false, false, kernelCubic,
                             "462f4ce1a64c4873fcc0b7b9f26bad0a");
    testUpscaleFromGenerated(960, 540, Scale2D, false, false, kernelModifiedCubic,
                             "bcae755510e5cfedbc755435e816fc4b");

    // with PA
    testUpscaleFromGenerated(960, 540, Scale2D, false, true, kernelNearest,
                             "3f2cef016be867f62eb680d98945cd5c");
    testUpscaleFromGenerated(960, 540, Scale2D, false, true, kernelLinear,
                             "db5a2f998c1754ebc43bca0c25c986ca");
    testUpscaleFromGenerated(960, 540, Scale2D, false, true, kernelCubic, "cd72218fbed84546e20424fe423056fa");
    testUpscaleFromGenerated(960, 540, Scale2D, false, true, kernelModifiedCubic,
                             "74cde59279990bb8100ecb8a370daf69");
}
