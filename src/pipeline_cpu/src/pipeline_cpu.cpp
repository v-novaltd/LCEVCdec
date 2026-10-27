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

#include "pipeline_cpu.h"
//
#include "buffer_cpu_ahb.h"
#include "frame_cpu.h"
#include "from_base.h"

#include <LCEVC/common/log.h>

namespace lcevc_dec::pipeline_cpu {

// PipelineCPU
//
PipelineCPU::PipelineCPU(const PipelineBuilderCPU& builder, pipeline::EventSink* eventSink)
    : PipelineBase(builder, eventSink)
    , m_configuration(builder.configuration())
    , m_buffersPool(builder.configuration().maxLatency * 2, builder.allocator())
    , m_picturesPool(builder.configuration().maxLatency * 2, builder.allocator())
    , m_framesPool(builder.configuration().maxLatency * 2, builder.allocator())
    , m_temporalBuffers(builder.configuration().numTemporalBuffers * RCMaxPlanes, builder.allocator())
{
    // Set up dithering
    ldppDitherGlobalInitialize(m_allocator, &m_dither, configuration().ditherSeed);

    if (m_configuration.useSystemAllocator == false) {
        // Special allocator for per frame enhancement data
        m_enhancementAllocator = ldcMemorySimpleAllocatorInitialize(&m_simpleAllocator, m_allocator);

        // Special allocator for per frame image buffer data
        m_bufferAllocator = ldcRecyclingAllocatorInitialize(&m_recyclingAllocator, m_allocator,
                                                            m_configuration.bufferRecycleCount);
    } else {
        // Use system allocator for all allocations
        m_enhancementAllocator = m_allocator;
        m_bufferAllocator = m_allocator;
    }

    // Fill in empty temporal buffer anchors
    TemporalBufferCPU buf{};
    buf.allocator = m_bufferAllocator;
    buf.desc.timestamp = kInvalidTimestamp;
    buf.timestampLimit = kInvalidTimestamp;
    for (uint32_t i = 0; i < m_configuration.numTemporalBuffers * RCMaxPlanes; ++i) {
        buf.desc.plane = i;
        m_temporalBuffers.append(buf);
    }
}

PipelineCPU::~PipelineCPU()
{
    // Flush and wait for any remaining frames
    this->synchronizeDecoder(kInvalidTimestamp, true);

    // Release pictures
    for (uint32_t i = 0; i < m_allocatedPictures.size(); ++i) {
        PictureCPU* picture{fromBase(m_allocatedPictures[i])};
        m_picturesPool.destroy(picture);
    }

    // Release frames
    for (uint32_t i = 0; i < m_allocatedFrames.size(); ++i) {
        FrameCPU* frame{fromBase(m_allocatedFrames[i])};
        frame->release(true);
        // Call destructor directly, as we are doing in-place construct/destruct
        m_framesPool.destroy(frame);
    }

    // Release any temporal buffers
    for (uint32_t i = 0; i < m_temporalBuffers.size(); ++i) {
        TemporalBufferCPU* tb = m_temporalBuffers.at(i);
        if (VNIsAllocated(tb->allocation)) {
            VNFree(m_bufferAllocator, &tb->allocation);
        }
    }
    // Release dither
    ldppDitherGlobalRelease(&m_dither);

    if (m_configuration.useSystemAllocator == false) {
        // Release buffer cache
        ldcRecyclingAllocatorDestroy(&m_recyclingAllocator);
        // Release frame memory arena
        ldcMemorySimpleAllocatorDestroy(&m_simpleAllocator);
    }
}

// Buffers
//
pipeline::BufferBase* PipelineCPU::allocateBuffer(uint32_t requiredSize, pipeline::BufferUsage usage)
{
    // Allocate buffer structure
    BufferCPU* const buffer = m_buffersPool.make(m_bufferAllocator, requiredSize, usage);
    if (!buffer) {
        VNLogError("Could not allocate buffer");
        return nullptr;
    }
    return buffer;
}

void PipelineCPU::releaseBuffer(pipeline::BufferBase* buffer)
{
    assert(buffer);

    // Release buffer structure
    m_buffersPool.destroy(fromBase(buffer));
}

// Pictures
//
// Internal allocation
pipeline::PictureBase* PipelineCPU::allocatePicture()
{
    // Allocate picture
    PictureCPU* picture{m_picturesPool.make(*this, this->m_allocator)};
    if (!picture) {
        VNLogError("Could not allocate picture");
        return nullptr;
    }

    // Insert into table
    m_allocatedPictures.append(picture);

    return picture;
}

void PipelineCPU::releasePicture(pipeline::PictureBase* picture)
{
    uint32_t idx = findAllocatedPicture(picture);
    if (idx == UINT32_MAX) {
        return;
    }

    m_allocatedPictures.removeReorderIndex(idx);

    if (picture->isExternalBuffer() && picture->buffer != nullptr) {
        // The only external BufferBase attached by this pipeline is the
        // AHardwareBuffer wrapper from storeAccelBuffer() - unlock and free it.
        BufferCPUAhb::destroy(m_allocator, static_cast<BufferCPUAhb*>(
                                               static_cast<pipeline::BufferBase*>(picture->buffer)));
        picture->setExternalBuffer(nullptr);
    }

    picture->unbindBuffer();
    m_picturesPool.destroy(fromBase(picture));
}

LdcReturnCode PipelineCPU::storeAccelBuffer(pipeline::PictureBase* picture,
                                            const LdpPictureDesc& desc, LdpAccelBuffer* accelBuffer)
{
#if VN_OS(ANDROID)
    // accelBuffer is an AHardwareBuffer* that was cast into LCEVC_AccelBufferHandle by the caller.
    auto* hardwareBuffer =
        reinterpret_cast<AHardwareBuffer*>(accelBuffer); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    LdpPicturePlaneDesc planes[kLdpPictureMaxNumPlanes] = {};
    BufferCPUAhb* buffer = BufferCPUAhb::create(m_allocator, hardwareBuffer, desc, planes);
    if (buffer == nullptr) {
        return LdcReturnCodeNotSupported;
    }

    // Attach the wrapper as an externally-owned buffer (so no pipeline buffer
    // is bound) and point the picture's planes into the locked memory.
    picture->setExternalBuffer(buffer);
    picture->setExternalPlaneDesc(planes);
    return LdcReturnCodeSuccess;
#else
    VNUnused(picture);
    VNUnused(desc);
    VNUnused(accelBuffer);
    VNLogVerbose("storeAccelBuffer: AHardwareBuffer support requires Android");
    return LdcReturnCodeNotSupported;
#endif
}

// Frames
//

// Allocate or find working data for a timestamp
//
// Given that there is going to be in the order of 100 or less frames, stick
// with an array and linear searches.
//
// NB: There may be more allocated frames that the configured latency - 'Done' frames
// do not count towards latency limit.
//
// Returns nullptr if there is no capacity for another frame.
//
pipeline::FrameBase* PipelineCPU::allocateFrame(uint64_t timestamp)
{
    assert(findFrame(timestamp, true) == nullptr);

    // Allocate frame
    FrameCPU* frame = m_framesPool.make(this->m_enhancementAllocator, this->m_bufferAllocator, timestamp);
    if (!frame) {
        return nullptr;
    }

    // Append allocation into table
    m_allocatedFrames.append(frame);

    return frame;
}

// Release frame back to pool
//
void PipelineCPU::freeFrame(pipeline::FrameBase* frame)
{
    uint32_t idx = findAllocatedFrame(frame);
    if (idx == UINT32_MAX) {
        return;
    }

    m_allocatedFrames.removeReorderIndex(idx);

    // Release task group and allocations
    frame->release(true);
    m_framesPool.destroy(fromBase(frame));

    // XXX Move
    // If there are no remaining frames, reset limits so that we can accept 'earlier' timestamp
    // into an empty decoder.
    if (m_allocatedFrames.isEmpty()) {
        VNLogDebug("Reset limits");
        m_sendLimit = kInvalidTimestamp;
        m_processingLimit = kInvalidTimestamp;
        m_skipLimit = kInvalidTimestamp;
        m_flushLimit = kInvalidTimestamp;
    }
}

//// Temporal
//
// Render
//
LdcReturnCode PipelineCPU::renderSendPicture(uint64_t timestamp, LdpPicture* outputPicture,
                                             const LdpRenderSendInformation* renderSendInformation,
                                             uint64_t delayUs)
{
    VNUnused(timestamp);

    if (outputPicture == nullptr) {
        return LdcReturnCodeInvalidParam;
    }
    if (renderSendInformation != nullptr && renderSendInformation->rotation != 0) {
        VNLogWarning(
            "renderSendPicture rotation is unused by the CPU pipeline, the system compositor "
            "rotates for the display");
    }
    if (delayUs > 0) {
        VNLogWarning("renderSendPicture delayUs is not implemented yet, rendering with zero delay");
    }

    return m_render.sendPicture(static_cast<pipeline::PictureBase*>(outputPicture));
}

TemporalBufferCPU* PipelineCPU::getTemporalBuffer(uint32_t n)
{
    if (n >= m_temporalBuffers.size()) {
        return nullptr;
    }

    return m_temporalBuffers.at(n);
}

} // namespace lcevc_dec::pipeline_cpu
