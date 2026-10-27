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

#include "frame_vulkan.h"
#include "picture_vulkan.h"
#include "pipeline_vulkan.h"
#include "test_utility.h"
//
#include <LCEVC/common/acceleration.h>
#include <LCEVC/common/cpp_tools.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/pipeline/event_sink.h>
#include <LCEVC/pipeline/picture_layout.h>
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

namespace {

struct SubmissionOrderTaskData
{
    PipelineVulkan* pipeline;
    FrameVulkan* frame;
    VulkanComputeOutcome outcome;
    std::vector<uint64_t>* order;
};

void* recordSubmissionTurn(LdcTask* task, const LdcTaskPart*)
{
    const auto& data = VNTaskData(task, SubmissionOrderTaskData);
    data.order->push_back(data.frame->timestamp);
    // Successful submissions deliberately remain pending until the test completes them.
    if (data.outcome != VulkanComputeOutcome::Succeeded && data.frame->setComputeOutcome(data.outcome)) {
        data.frame->signalComputeTaskDone();
    }
    data.pipeline->completeSubmissionTurn(data.frame);
    return nullptr;
}

} // namespace

class PipelineVulkanSubmissionOrder : public testing::TestWithParam<int32_t>
{};

TEST_P(PipelineVulkanSubmissionOrder, LaterReadyFramesSubmitInOrderWithoutWaitingForCompletion)
{
    PipelineBuilderVulkan builder(ldcMemoryAllocatorMalloc());
    ASSERT_TRUE(builder.configure("threads", GetParam()));
    ASSERT_TRUE(builder.configure("vulkan_contexts", int32_t{4}));
    std::unique_ptr<Pipeline> owner = builder.finish(nullptr);
    if (!owner) {
        GTEST_SKIP() << "Skipping test due to lack of Vulkan support";
    }
    auto* pipeline = static_cast<PipelineVulkan*>(owner.get());

    std::vector<uint64_t> order;
    std::vector<std::unique_ptr<FrameVulkan>> frames;
    std::vector<LdcTaskDependency> ready;
    const VulkanComputeOutcome outcomes[] = {VulkanComputeOutcome::Succeeded,
                                             VulkanComputeOutcome::Cancelled, VulkanComputeOutcome::Failed,
                                             VulkanComputeOutcome::Succeeded};

    for (uint32_t i = 0; i < VNArraySize(outcomes); ++i) {
        auto frame = std::make_unique<FrameVulkan>(ldcMemoryAllocatorMalloc(),
                                                   ldcMemoryAllocatorMalloc(), 16 + i);
        EXPECT_TRUE(ldcTaskGroupInitialize(frame->taskGroup(), pipeline->taskPool(),
                                           kTaskPoolMaxDependencies, frame->timestamp));
        frame->setComputeTaskDone(frame->taskDependencyAdd());
        ready.push_back(frame->taskDependencyAdd());
        // Turn dependencies deliberately have different indices in the frame-local groups.
        for (uint32_t extra = 0; extra < i; ++extra) {
            frame->taskDependencyAdd();
        }
        const LdcTaskDependency inputs[] = {ready.back(), pipeline->registerSubmissionTurn(frame.get())};
        const SubmissionOrderTaskData data{pipeline, frame.get(), outcomes[i], &order};
        frame->taskAddSink(inputs, VNArraySize(inputs), recordSubmissionTurn, &data, sizeof(data),
                           "RecordSubmissionTurn");
        frames.push_back(std::move(frame));
    }

    // Reproduce N+1 becoming ready before N. No worker should run later compute yet.
    for (size_t i = 1; i < frames.size(); ++i) {
        frames[i]->unblockDependency(ready[i]);
        uint32_t waiting = 0;
        EXPECT_EQ(1u, ldcTaskGroupGetTaskCount(frames[i]->taskGroup(), &waiting));
        EXPECT_EQ(1u, waiting);
    }
    EXPECT_TRUE(order.empty());

    frames.front()->unblockDependency(ready.front());
    for (const auto& frame : frames) {
        frame->waitForTasks();
    }
    EXPECT_EQ((std::vector<uint64_t>{16, 17, 18, 19}), order);
    for (size_t i = 0; i < frames.size(); ++i) {
        if (outcomes[i] == VulkanComputeOutcome::Succeeded) {
            EXPECT_EQ(VulkanComputeOutcome::Pending, frames[i]->computeOutcome());
        } else {
            EXPECT_EQ(outcomes[i], frames[i]->computeOutcome());
        }
    }

    // A drained queue must immediately admit the next frame, including inline execution.
    FrameVulkan next(ldcMemoryAllocatorMalloc(), ldcMemoryAllocatorMalloc(), 20);
    EXPECT_TRUE(ldcTaskGroupInitialize(next.taskGroup(), pipeline->taskPool(),
                                       kTaskPoolMaxDependencies, next.timestamp));
    next.setComputeTaskDone(next.taskDependencyAdd());
    const LdcTaskDependency turn = pipeline->registerSubmissionTurn(&next);
    const SubmissionOrderTaskData data{pipeline, &next, VulkanComputeOutcome::Succeeded, &order};
    next.taskAddSink(&turn, 1, recordSubmissionTurn, &data, sizeof(data), "RecordSubmissionTurn");
    next.waitForTasks();
    EXPECT_EQ((std::vector<uint64_t>{16, 17, 18, 19, 20}), order);
    EXPECT_EQ(VulkanComputeOutcome::Pending, next.computeOutcome());

    // Fences may complete after the submission queue has drained and in a different order.
    next.computeTaskFinish(pipeline);
    for (size_t i = frames.size(); i-- > 0;) {
        if (outcomes[i] == VulkanComputeOutcome::Succeeded) {
            frames[i]->computeTaskFinish(pipeline);
        }
        EXPECT_EQ(outcomes[i], frames[i]->computeOutcome());
        frames[i]->release(false);
    }
    next.release(false);
}

INSTANTIATE_TEST_SUITE_P(WorkerCounts, PipelineVulkanSubmissionOrder, testing::Values(1, 2, 4));

TEST(VulkanPhysicalPlaneMapping, PlanarComponentsMapToSeparatePlanes)
{
    LdpPictureLayout layout{};
    ldpPictureLayoutInitialize(&layout, LdpColorFormatI420_10_LE, 1920, 1080, 0);

    const auto masks = physicalPlaneComponentMasks(layout);
    EXPECT_EQ(masks[0], LdpColorComponentMaskC0);
    EXPECT_EQ(masks[1], LdpColorComponentMaskC1);
    EXPECT_EQ(masks[2], LdpColorComponentMaskC2);
    EXPECT_EQ(masks[3], LdpColorComponentMaskNone);
}

TEST(VulkanPhysicalPlaneMapping, SemiplanarChromaSharesOnePhysicalPlane)
{
    for (const LdpColorFormat format : {LdpColorFormatNV12_8, LdpColorFormatNV21_8, LdpColorFormatP010_10}) {
        LdpPictureLayout layout{};
        ldpPictureLayoutInitialize(&layout, format, 1920, 1080, 0);

        const auto masks = physicalPlaneComponentMasks(layout);
        EXPECT_EQ(masks[0], LdpColorComponentMaskC0);
        EXPECT_EQ(masks[1], LdpColorComponentMaskC1 | LdpColorComponentMaskC2);
        EXPECT_EQ(masks[2], LdpColorComponentMaskNone);
        EXPECT_EQ(masks[3], LdpColorComponentMaskNone);
    }
}

TEST(VulkanPhysicalPlaneMapping, MonochromeHasOneComponent)
{
    LdpPictureLayout layout{};
    ldpPictureLayoutInitialize(&layout, LdpColorFormatGRAY_8, 1920, 1080, 0);

    const auto masks = physicalPlaneComponentMasks(layout);
    EXPECT_EQ(masks[0], LdpColorComponentMaskC0);
    EXPECT_EQ(masks[1], LdpColorComponentMaskNone);
    EXPECT_EQ(masks[2], LdpColorComponentMaskNone);
    EXPECT_EQ(masks[3], LdpColorComponentMaskNone);
}

TEST(VulkanPhysicalPlaneMapping, PackedRgbComponentsShareOnePhysicalPlane)
{
    for (const LdpColorFormat format : {LdpColorFormatRGB_8, LdpColorFormatBGR_8}) {
        LdpPictureLayout layout{};
        ldpPictureLayoutInitialize(&layout, format, 1920, 1080, 0);

        const auto masks = physicalPlaneComponentMasks(layout);
        EXPECT_EQ(masks[0], LdpColorComponentMaskC0 | LdpColorComponentMaskC1 | LdpColorComponentMaskC2);
        EXPECT_EQ(masks[1], LdpColorComponentMaskNone);
    }

    LdpPictureLayout rgbaLayout{};
    ldpPictureLayoutInitialize(&rgbaLayout, LdpColorFormatRGBA_8, 1920, 1080, 0);
    const auto rgbaMasks = physicalPlaneComponentMasks(rgbaLayout);
    EXPECT_EQ(rgbaMasks[0], LdpColorComponentMaskC0 | LdpColorComponentMaskC1 |
                                LdpColorComponentMaskC2 | LdpColorComponentMaskC3);
    EXPECT_EQ(rgbaMasks[1], LdpColorComponentMaskNone);
}

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
