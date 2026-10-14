/* Copyright (c) V-Nova International Limited 2024-2026. All rights reserved.
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

#include "picture_vulkan.h"
#include "pipeline_vulkan.h"
#include "test_utility.h"
//
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/cpp_tools.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/pipeline/event_sink.h>
#include <LCEVC/pipeline/pipeline.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pipeline_vulkan/create_pipeline.h>
#include <LCEVC/pipeline_vulkan/types_vulkan.h>
//
#include <fmt/core.h>
#include <gtest/gtest.h>
//
#include <algorithm>
#include <memory>

using namespace lcevc_dec::pipeline;
using namespace lcevc_dec::pipeline_vulkan;

TEST(PipelineVulkanConfiguration, ContextCountDefaultsToOne)
{
    PipelineBuilderVulkan builder(ldcMemoryAllocatorMalloc());
    EXPECT_EQ(1, builder.configuration().contexts);
}

TEST(PipelineVulkanConfiguration, ContextCountMustBePositive)
{
    PipelineBuilderVulkan builder(ldcMemoryAllocatorMalloc());
    EXPECT_FALSE(builder.configure("vulkan_contexts", int32_t{0}));
    EXPECT_FALSE(builder.configure("vulkan_contexts", int32_t{-1}));
    EXPECT_EQ(1, builder.configuration().contexts);
}

TEST(PipelineVulkanConfiguration, CreatesConfiguredFrameContexts)
{
    PipelineBuilderVulkan builder(ldcMemoryAllocatorMalloc());
    ASSERT_TRUE(builder.configure("vulkan_contexts", int32_t{4}));

    std::unique_ptr<Pipeline> pipeline = builder.finish(nullptr);
    if (!pipeline) {
        GTEST_SKIP() << "Skipping test due to lack of Vulkan support";
    }

    auto* vulkanPipeline = static_cast<PipelineVulkan*>(pipeline.get());
    EXPECT_EQ(4u, vulkanPipeline->backend().compute().frameContextCount());
}

class PipelineVulkanFixture : public testing::Test
{
public:
    PipelineVulkanFixture() noexcept {};

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

TEST_F(PipelineVulkanFixture, MappedBufferWindowsSupportAccessModes)
{
    auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());
    ASSERT_NE(nullptr, pipeline);

    constexpr uint32_t kBufferSize = 64;
    BufferVulkan* buffer = pipeline->allocateBuffer(kBufferSize, BufferUsageIn);
    ASSERT_NE(nullptr, buffer);

    std::vector<uint8_t> initial(kBufferSize, 0);
    buffer->copyIn(0, initial.data(), static_cast<uint32_t>(initial.size()));

    LdpBufferMapping mapping{};
    ASSERT_TRUE(buffer->map(&mapping, 8, 16, LdpAccessWrite));
    ASSERT_EQ(16u, mapping.size);
    std::fill_n(mapping.ptr, mapping.size, uint8_t{0x11});
    buffer->unmap(&mapping);

    ASSERT_TRUE(buffer->map(&mapping, 12, 4, LdpAccessModify));
    EXPECT_EQ(uint8_t{0x11}, mapping.ptr[0]);
    std::fill_n(mapping.ptr, mapping.size, uint8_t{0x22});

    const std::vector<uint8_t> mixedWrite{0x33, 0x44, 0x55, 0x66};
    buffer->copyIn(32, mixedWrite.data(), static_cast<uint32_t>(mixedWrite.size()));
    std::vector<uint8_t> mixedRead(mixedWrite.size(), 0);
    buffer->copyOut(mixedRead.data(), 32, static_cast<uint32_t>(mixedRead.size()));
    EXPECT_EQ(mixedWrite, mixedRead);
    buffer->unmap(&mapping);

    ASSERT_TRUE(buffer->map(&mapping, 8, 16, LdpAccessRead));
    for (uint32_t i = 0; i < mapping.size; ++i) {
        EXPECT_EQ(i >= 4 && i < 8 ? uint8_t{0x22} : uint8_t{0x11}, mapping.ptr[i]);
    }
    buffer->unmap(&mapping);

    std::vector<uint8_t> output(16, 0);
    buffer->copyOut(output.data(), 8, static_cast<uint32_t>(output.size()));
    std::vector<uint8_t> expected(16, 0x11);
    std::fill(expected.begin() + 4, expected.begin() + 8, uint8_t{0x22});
    EXPECT_EQ(expected, output);

    ASSERT_TRUE(buffer->map(&mapping, 8, 16, LdpAccessRead));
    buffer->clear();
    buffer->unmap(&mapping);
    buffer->copyOut(output.data(), 8, static_cast<uint32_t>(output.size()));
    EXPECT_EQ(std::vector<uint8_t>(output.size(), 0), output);

    pipeline->releaseBuffer(buffer);
}

TEST_F(PipelineVulkanFixture, AllocatePicturesManaged)
{
    const LdpPictureDesc pictureDesc{1920, 1080, LdpColorFormatI420_8};

    LdpPicture* picture = nullptr;
    ASSERT_EQ(LdcReturnCodeSuccess, mPipeline->allocPicture(pictureDesc, picture));
    ASSERT_TRUE(picture);

    mPipeline->freePicture(picture);
}
