/* Copyright (c) V-Nova International Limited 2025-2026. All rights reserved.
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

#include <LCEVC/build_config.h>

#if VN_OS(ANDROID)

#include "buffer_vulkan_ahb.h"
#include "pipeline_builder_vulkan.h"
#include "pipeline_vulkan.h"

#include <android/hardware_buffer.h>
#include <gtest/gtest.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/pipeline/picture_layout.h>
#include <LCEVC/pipeline/pipeline.h>
#include <vulkan/vulkan.h>

#include <memory>

using namespace lcevc_dec::pipeline;
using namespace lcevc_dec::pipeline_vulkan;

class BufferVulkanAhbFixture : public testing::Test
{
public:
    std::unique_ptr<Pipeline> mPipeline;

    void SetUp() override
    {
        auto* pipelineBuilder = new PipelineBuilderVulkan(ldcMemoryAllocatorMalloc());
        ASSERT_TRUE(pipelineBuilder);

        EventSink* eventSink = nullptr;
        mPipeline = pipelineBuilder->finish(eventSink);
        delete pipelineBuilder;

        if (!mPipeline) {
            GTEST_SKIP() << "Skipping test due to lack of Vulkan support";
        }
    }
};

TEST_F(BufferVulkanAhbFixture, ImportBlobBuffer)
{
    constexpr uint32_t kSize = 4096;

    AHardwareBuffer_Desc desc{};
    desc.width = kSize;
    desc.height = 1;
    desc.layers = 1;
    desc.format = AHARDWAREBUFFER_FORMAT_BLOB;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER | AHARDWAREBUFFER_USAGE_CPU_READ_RARELY |
                 AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;

    AHardwareBuffer* ahb = nullptr;
    ASSERT_EQ(0, AHardwareBuffer_allocate(&desc, &ahb));
    ASSERT_NE(nullptr, ahb);

    auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());
    ASSERT_NE(nullptr, pipeline);

    {
        auto buffer = BufferVulkanAhb::create(pipeline->backend(), ahb);
        ASSERT_NE(nullptr, buffer);
        EXPECT_NE(VK_NULL_HANDLE, buffer->getVkBuffer());
        EXPECT_EQ(VK_NULL_HANDLE, buffer->getVkImage());
        EXPECT_EQ(kSize, buffer->size());
        EXPECT_EQ(ahb, buffer->getHardwareBuffer());
        // BLOB has no picture layout.
        EXPECT_EQ(nullptr, buffer->getPictureLayout());
    }

    AHardwareBuffer_release(ahb);
}

// Parametrized test covering each AHardwareBuffer image format that
// BufferVulkanAhb maps to an LdpPictureLayout. For each format we verify the
// layout describes the expected dimensions and LdpColorFormat. VkImage
// presence is also asserted, but only when the AHB format reports a known
// VkFormat; YUV formats often report VK_FORMAT_UNDEFINED ("external format")
// which needs a sampler ycbcr conversion to be usable - that path isn't
// implemented yet, so we don't require a VkImage in that case.
struct AhbImageFormatCase
{
    uint32_t ahbFormat;
    LdpColorFormat expectedLdpFormat;
    const char* name;
};

class BufferVulkanAhbImageFixture
    : public BufferVulkanAhbFixture
    , public testing::WithParamInterface<AhbImageFormatCase>
{};

TEST_P(BufferVulkanAhbImageFixture, ImportImage)
{
    const auto& p = GetParam();
    constexpr uint32_t kWidth = 64;
    constexpr uint32_t kHeight = 64;

    AHardwareBuffer_Desc desc{};
    desc.width = kWidth;
    desc.height = kHeight;
    desc.layers = 1;
    desc.format = p.ahbFormat;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

    if (!AHardwareBuffer_isSupported(&desc)) {
        GTEST_SKIP() << "AHardwareBuffer " << p.name << " not supported on this device";
    }
    AHardwareBuffer* ahb = nullptr;
    ASSERT_EQ(0, AHardwareBuffer_allocate(&desc, &ahb));
    ASSERT_NE(nullptr, ahb);

    auto* pipeline = static_cast<PipelineVulkan*>(mPipeline.get());
    ASSERT_NE(nullptr, pipeline);

    {
        auto buffer = BufferVulkanAhb::create(pipeline->backend(), ahb);
        ASSERT_NE(nullptr, buffer);
        EXPECT_EQ(VK_NULL_HANDLE, buffer->getVkBuffer());

        const LdpPictureLayout* layout = buffer->getPictureLayout();
        ASSERT_NE(nullptr, layout);
        EXPECT_EQ(kWidth, layout->width);
        EXPECT_EQ(kHeight, layout->height);
        EXPECT_EQ(p.expectedLdpFormat, ldpPictureLayoutFormat(layout));

        if (buffer->getVkImage() != VK_NULL_HANDLE) {
            EXPECT_GT(buffer->size(), 0u);
            EXPECT_EQ(ahb, buffer->getHardwareBuffer());
        }
    }

    AHardwareBuffer_release(ahb);
}

INSTANTIATE_TEST_SUITE_P(
    AllFormats, BufferVulkanAhbImageFixture,
    testing::Values(
        AhbImageFormatCase{AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM, LdpColorFormatRGBA_8, "R8G8B8A8_UNORM"},
        AhbImageFormatCase{AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM, LdpColorFormatRGBA_10_2_LE,
                           "R10G10B10A2_UNORM"},
        AhbImageFormatCase{AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420, LdpColorFormatI420_8, "Y8Cb8Cr8_420"},
        AhbImageFormatCase{AHARDWAREBUFFER_FORMAT_YCbCr_P010, LdpColorFormatP010_10, "YCbCr_P010"}),
    [](const testing::TestParamInfo<AhbImageFormatCase>& info) { return info.param.name; });

#endif // VN_OS(ANDROID)
