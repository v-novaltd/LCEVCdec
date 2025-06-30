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

#ifndef VN_LCEVC_PIPELINE_VULKAN_FRAME_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_FRAME_VULKAN_H

#include "pipeline_vulkan.h"

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/return_code.h>
#include <LCEVC/common/task_pool.h>
#include <LCEVC/enhancement/dimensions.h>
#include <LCEVC/pipeline/frame.h>

#include <atomic>
#include <cstdint>

namespace lcevc_dec::pipeline_vulkan {

class PipelineVulkan;
class PictureVulkan;

enum FrameState
{
    FrameStateUnknown,
    FrameStateReorder,
    FrameStateProcessing,
    FrameStateDone,
};

// Extended frame structure for this pipeline
//
class FrameVulkan : public LdpFrame
{
public:
    explicit FrameVulkan(PipelineVulkan* pipeline, uint64_t timestamp);
    ~FrameVulkan() = default;

    // Allocate per frame buffers
    bool initialize();

    // Generate per-frame tasks, called once base frame has valid configuration
    void generateTasks(uint64_t previousTimestamp);

    // Tidy up
    void release();

    bool initializeCommandBuffers();
    void releaseCommandBuffers();

    bool initializeIntermediateBuffers();
    void releaseIntermediateBuffers();

    // Find command buffer given the tile index
    LdpEnhancementTile* getEnhancementTile(uint32_t tileIdx) const
    {
        assert(tileIdx < ldeTotalNumTiles(globalConfig));

        return VNAllocationPtr(m_enhancementTilesAllocation, LdpEnhancementTile) + tileIdx;
    }

    // Attach a base picture to the frame (and mark dependency as met)
    LdcReturnCode setBase(LdpPicture* picture, uint64_t deadline, void* userData);

    // Return true if a base picture has been set for frame, and it's description recorded
    // NB: the base picture itself may have gone by time this data is needed at output time
    bool baseDataValid() const { return baseFormat != LdpColorFormatUnknown; }

    // Return true if frame need an intermediate buffer for given loq/plane
    bool needsIntermediateBuffer(LdeLOQIndex loq, uint8_t plane) const;

    // Return true if the frame has everything such that it will complete without further inputs
    bool canComplete() const;

    // Work out color format for base
    LdpColorFormat getBaseColorFormat() const;

    // Work out color format for output
    LdpColorFormat getOutputColorFormat() const;

    // Construct picture description for output
    LdpPictureDesc getOutputPictureDesc() const;

    uint8_t numEnhancedPlanes() const { return globalConfig->numPlanes; }
    uint8_t numImagePlanes() const;

    // Return true if the frame's plane should have enhancement applied
    bool isEnhanced(LdeLOQIndex loq, uint32_t plane) const;

    // Get plane descriptions for each of the buffers
    void getBasePlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const;
    void getOutputPlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const;
    void getIntermediatePlaneDesc(uint32_t plane, LdeLOQIndex loq, LdpPicturePlaneDesc& planeDesc) const;

#ifdef VN_SDK_LOG_ENABLE_DEBUG
    // Create a debug description of this frame
    size_t longDescription(char* buffer, size_t bufferSize) const;
#endif

    VNNoCopyNoMove(FrameVulkan);

private:
    friend PipelineVulkan;

    // Associated pipeline
    PipelineVulkan* m_pipeline{};

    // Current frame state
    // This get peeked at across threads - so it is atomic
    std::atomic<FrameState> m_state{FrameStateUnknown};

    // Task group for this frame
    LdcTaskGroup m_taskGroup{};

    // Allocation for un-encapsulated enhancement data
    LdcMemoryAllocation m_enhancementData{};

    // An array of LdpEnhancementTile
    LdcMemoryAllocation m_enhancementTilesAllocation{};

    // Internal buffers for residual application
    PictureVulkan* m_intermediatePicture[LOQMaxCount]{};
    LdcMemoryAllocation m_intermediateBufferAllocation[RCMaxPlanes][LOQMaxCount] = {};
    LdpPictureLayout m_intermediateLayout[LOQMaxCount] = {};

    // Pointers to buffer to use for each LOQ - may share buffers between LoQs depending on scaling modes
    uint8_t* m_intermediateBufferPtr[RCMaxPlanes][LOQMaxCount] = {};

    // Dependencies in task group
    LdcTaskDependency m_depBasePicture{kTaskDependencyInvalid};
    LdcTaskDependency m_depOutputPicture{kTaskDependencyInvalid};
    LdcTaskDependency m_depTemporalBuffer[RCMaxPlanes] = {kTaskDependencyInvalid};

    // Description of temporal buffer(s) needed for this frame
    TemporalBufferDesc m_temporalBufferDesc[RCMaxPlanes] = {};

    // Temporal buffer(s) assigned to this frame (once dependency is met)
    TemporalBuffer* m_temporalBuffer[RCMaxPlanes] = {};

    // Dithering info for this frame
    LdppDitherFrame m_frameDither{};

    // True if this frame can be moved from reorder to in-process
    bool m_ready{false};

    // True if this frame should be skipped - the output picture will not be used,
    // but any frame to frame decode state (e.g. temporal) should still be updated.
    bool m_skip{false};

    // True if this frame should be copied to output with no processing (but optionally scaled)
    bool m_passthrough{false};

    // Deadline for this frame in microseconds relative to threadTimeMicroseconds()
    uint64_t m_deadline{UINT64_MAX};

    // Final decodeInfo to sent back to API
    LdpDecodeInformation m_decodeInfo;
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_FRAME_VULKAN_H
