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

#include "frame_vulkan.h"

#include "from_base.h"
#include "tasks_vulkan.h"
#include "temporal_buffer_vulkan.h"

#include <LCEVC/common/limit.h>
#include <LCEVC/common/platform.h>

namespace lcevc_dec::pipeline_vulkan {

FrameVulkan::FrameVulkan(LdcMemoryAllocator* enhancementAllocator,
                         LdcMemoryAllocator* bufferAllocator, uint64_t timestamp)
    : FrameBase(enhancementAllocator, bufferAllocator, timestamp)
{}

FrameVulkan::~FrameVulkan() {}

void FrameVulkan::getBasePlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const
{
    const PictureVulkan* picture = fromPipeline(basePicture);
    picture->getPlaneDescInternal(plane, planeDesc);
}

void FrameVulkan::getOutputPlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const
{
    const PictureVulkan* picture = fromPipeline(outputPicture);
    picture->getPlaneDescInternal(plane, planeDesc);
}

// NB: Vulkan Does not use CPU memory intermediates
//
void FrameVulkan::getIntermediatePlaneDesc(uint32_t plane, LdeLOQIndex loq, LdpPicturePlaneDesc& planeDesc) const
{
    assert(loq < LOQMaxCount);
    assert(plane < RCMaxPlanes);

    assert(0);
}

bool FrameVulkan::initializeIntermediateBuffers() { return false; }

void FrameVulkan::releaseIntermediateBuffers() {}

bool FrameVulkan::needsIntermediateBuffer(LdeLOQIndex loq, uint8_t plane) const { return false; }

const LdpPictureLayout* FrameVulkan::getIntermediateLayout(LdeLOQIndex loq) const
{
    assert(0);
    return nullptr;
}

void FrameVulkan::getTemporalBufferPlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const
{
    assert(plane < RCMaxPlanes);
    assert(m_temporalBuffer[plane]);

    TemporalBufferVulkan* tb = fromBase(m_temporalBuffer[plane]);
    planeDesc = tb->planeDesc;
}

void FrameVulkan::freeIntermediatePictures(PipelineVulkan* pipeline)
{
    for (auto* picture : m_intermediatePicturePool) {
        if (picture) {
            pipeline->freePicture(picture);
        }
    }
    m_intermediatePicturePool.clear();
}

uint32_t FrameVulkan::getTotalGpuCommandBufferSize(LdeLOQIndex loq) const
{
    uint32_t totalBufferSize{0};

    for (uint32_t tile = 0; tile < enhancementTileCount; ++tile) {
        const LdpEnhancementTile* et = getEnhancementTile(tile);
        if (et->loq == loq && et->bufferGpu.commandCount > 0) {
            // Align each new command buffer to 4 bytes boundary
            totalBufferSize = alignU32(totalBufferSize, 4);

            const uint32_t cmdSize =
                static_cast<uint32_t>(sizeof(LdeCmdBufferGpuCmd)) * et->bufferGpu.commandCount;
            const uint32_t resSize = et->bufferGpu.residualCount * sizeof(uint16_t);

            totalBufferSize += cmdSize + resSize;
        }
    }

    return totalBufferSize;
}

bool FrameVulkan::setComputeOutcome(VulkanComputeOutcome outcome)
{
    VulkanComputeOutcome expected = VulkanComputeOutcome::Pending;
    const bool transitioned = m_computeOutcome.compare_exchange_strong(
        expected, outcome, std::memory_order_release, std::memory_order_relaxed);
    if (!transitioned) {
        VNLogError("setComputeOutcome: compare_exchange_strong failed - state was not pending");
    }
    return transitioned;
}

void FrameVulkan::signalComputeTaskDone()
{
    assert(computeOutcome() != VulkanComputeOutcome::Pending);
    ldcTaskDependencyMet(&m_taskGroup, m_computeTaskDone, nullptr);
}

void FrameVulkan::computeTaskFinish(PipelineVulkan* pipeline)
{
    // Copy output to external CPU planes if present.
    PictureVulkan* op = fromPipeline(outputPicture);
    LdpPictureBufferDesc exDesc{};
    if (op && !op->isExternalBuffer() && op->getBufferDesc(exDesc) && exDesc.data != nullptr &&
        exDesc.byteSize != 0) {
        BufferVulkan* managedBuffer = fromPipeline(op->buffer);
        managedBuffer->copyOut(exDesc.data, 0, exDesc.byteSize);
    }

    freeIntermediatePictures(pipeline);

    if (setComputeOutcome(VulkanComputeOutcome::Succeeded)) {
        signalComputeTaskDone();
    }
}

void FrameVulkan::generateTasks(pipeline::PipelineBase* pipeline, uint64_t previousTimestamp)
{
    lcevc_dec::pipeline_vulkan::generateTasks(fromBase(pipeline), this, previousTimestamp);
}

} // namespace lcevc_dec::pipeline_vulkan
