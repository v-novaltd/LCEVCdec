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

#include "pipeline_cpu.h"
//
#include "frame_cpu.h"
#include "picture_cpu.h"
#include "pipeline_config_cpu.h"

#include <LCEVC/common/check.h>
#include <LCEVC/common/constants.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/return_code.h>
#include <LCEVC/common/task_pool.h>
#include <LCEVC/common/threads.h>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/enhancement/decode.h>
#include <LCEVC/pipeline/buffer.h>
#include <LCEVC/pipeline/picture_layout.h>
#include <LCEVC/pixel_processing/apply_cmdbuffer.h>
#include <LCEVC/pixel_processing/blit.h>
#include <LCEVC/pixel_processing/upscale.h>
//
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace lcevc_dec::pipeline_cpu {

// Utility functions for finding and sorting things in Vectors
//
namespace {
    // Compare 'close' timestamps - allows wrapping around end of uint64_t range
    // (Unlikely when starting at zero - but allows timestamps to start 'before' zero)
    inline int compareTimestamps(uint64_t lhs, uint64_t rhs)
    {
        const int64_t delta = (int64_t)(lhs - rhs);
        if (delta < 0) {
            return -1;
        }
        if (delta > 0) {
            return 1;
        }
        return 0;
    }

    inline int findFrameTimestamp(const void* element, const void* ptr)
    {
        const auto* alloc{static_cast<const LdcMemoryAllocation*>(element)};
        assert(VNIsAllocated(*alloc));
        const uint64_t ets{VNAllocationPtr(*alloc, FrameCPU)->timestamp};
        const uint64_t ts{*static_cast<const uint64_t*>(ptr)};

        return compareTimestamps(ets, ts);
    }

    inline int sortFramePtrTimestamp(const void* lhs, const void* rhs)
    {
        const auto* frameLhs{*static_cast<const FrameCPU* const *>(lhs)};
        const auto* frameRhs{*static_cast<const FrameCPU* const *>(rhs)};

        return compareTimestamps(frameLhs->timestamp, frameRhs->timestamp);
    }

    inline int findBasePictureTimestamp(const void* element, const void* ptr)
    {
        const auto* alloc{static_cast<const LdcMemoryAllocation*>(element)};
        assert(VNIsAllocated(*alloc));
        const uint64_t ets{VNAllocationPtr(*alloc, BasePicture)->timestamp};
        const uint64_t ts{*static_cast<const uint64_t*>(ptr)};

        return compareTimestamps(ets, ts);
    }

} // namespace

// PipelineCPU
//
PipelineCPU::PipelineCPU(const PipelineBuilderCPU& builder, pipeline::EventSink* eventSink)
    : m_configuration(builder.configuration())
    , m_eventSink(eventSink ? eventSink : pipeline::EventSink::nullSink())
    , m_allocator(builder.allocator())
    , m_buffers(builder.configuration().maxLatency, builder.allocator())
    , m_pictures(builder.configuration().maxLatency, builder.allocator())
    , m_frames(builder.configuration().maxLatency, builder.allocator())
    , m_reorderIndex(builder.configuration().maxLatency, builder.allocator())
    , m_processingIndex(builder.configuration().maxLatency, builder.allocator())
    , m_maxReorder(m_configuration.defaultMaxReorder)
    , m_temporalBuffers(builder.configuration().numTemporalBuffers * RCMaxPlanes, builder.allocator())
    , m_basePicturePending(nextPowerOfTwoU32(builder.configuration().maxLatency + 1), builder.allocator())
    , m_basePictureOutBuffer(nextPowerOfTwoU32(builder.configuration().maxLatency + 1), builder.allocator())
    , m_outputPictureAvailableBuffer(nextPowerOfTwoU32(builder.configuration().maxLatency + 1),
                                     builder.allocator())
{
    // Set up dithering
    ldppDitherGlobalInitialize(m_allocator, &m_dither, m_configuration.ditherSeed);

    // Set up an allocator for per frame data
    ldcRollingArenaInitialize(&m_rollingArena, m_allocator, m_configuration.initialArenaCount,
                              m_configuration.initialArenaSize);

    // Configuration pool
    LdeBitstreamVersion bitstreamVersion = BitstreamVersionUnspecified;
    if (m_configuration.forceBitstreamVersion >= BitstreamVersionInitial &&
        m_configuration.forceBitstreamVersion <= BitstreamVersionCurrent) {
        bitstreamVersion = static_cast<LdeBitstreamVersion>(m_configuration.forceBitstreamVersion);
    }
    ldeConfigPoolInitialize(m_allocator, &m_configPool, bitstreamVersion);

    // Start task pool - pool threads is 1 less than configured threads
    VNCheck(m_configuration.numThreads >= 1);
    ldcTaskPoolInitialize(&m_taskPool, m_allocator, m_allocator, m_configuration.numThreads - 1,
                          m_configuration.numReservedTasks);

    // Fill in empty temporal buffer anchors
    TemporalBuffer buf{};
    buf.desc.timestamp = kInvalidTimestamp;
    buf.timestampLimit = kInvalidTimestamp;
    for (uint32_t i = 0; i < m_configuration.numTemporalBuffers * RCMaxPlanes; ++i) {
        buf.desc.plane = i;
        m_temporalBuffers.append(buf);
    }

    m_eventSink->generate(pipeline::EventCanSendEnhancement);
    m_eventSink->generate(pipeline::EventCanSendBase);
    m_eventSink->generate(pipeline::EventCanSendPicture);
}

PipelineCPU::~PipelineCPU()
{
    // Release pictures
    for (uint32_t i = 0; i < m_pictures.size(); ++i) {
        PictureCPU* picture{VNAllocationPtr(m_pictures[i], PictureCPU)};
        // Call destructor directly, as we are doing in-place construct/destruct
        picture->~PictureCPU();
        VNFree(m_allocator, &m_pictures[i]);
    }

    // Release frames
    for (uint32_t i = 0; i < m_frames.size(); ++i) {
        FrameCPU* frame{VNAllocationPtr(m_frames[i], FrameCPU)};
        frame->release();
        // Call destructor directly, as we are doing in-place construct/destruct
        frame->~FrameCPU();
        VNFree(m_allocator, &m_frames[i]);
    }

    // Release any temporal buffers
    for (uint32_t i = 0; i < m_temporalBuffers.size(); ++i) {
        TemporalBuffer* tb = m_temporalBuffers.at(i);
        if (VNIsAllocated(tb->allocation)) {
            VNFree(m_allocator, &tb->allocation);
        }
    }
    // Release dither
    ldppDitherGlobalRelease(&m_dither);

    ldeConfigPoolRelease(&m_configPool);

    ldcRollingArenaDestroy(&m_rollingArena);

    // Close down task pool
    ldcTaskPoolDestroy(&m_taskPool);

    m_eventSink->generate(pipeline::EventExit);
}

// Send/receive
//
LdcReturnCode PipelineCPU::sendEnhancementData(uint64_t timestamp, const uint8_t* data, uint32_t byteSize)
{
    VNLogDebug("sendEnhancementData: %" PRIx64 " %d", timestamp, byteSize);
    VNTraceInstant("sendEnhancementData", timestamp);

    // Invalid if this timestamp is already present in decoder.
    //
    // NB: API clients are expected to make distinct timestamps over discontinuities using utility library
    if (findFrame(timestamp)) {
        return LdcReturnCodeInvalidParam;
    }

    if (frameLatency() >= m_configuration.maxLatency) {
        VNLogDebug("sendEnhancementData: %" PRIx64 " AGAIN", timestamp);
        return LdcReturnCodeAgain;
    }

    // New pending frame
    FrameCPU* const frame{allocateFrame(timestamp)};
    if (!frame) {
        return LdcReturnCodeError;
    }

    LdcMemoryAllocation enhancementDataAllocation{};
    uint8_t* const enhancement{VNAllocateArray(m_allocator, &enhancementDataAllocation, uint8_t, byteSize)};
    memcpy(enhancement, data, byteSize);
    frame->m_enhancementData = enhancementDataAllocation;
    frame->m_state = FrameStateReorder;

    // Add frame to reorder table sorted by timestamp
    m_reorderIndex.insert(sortFramePtrTimestamp, frame);

    // Attach any pending base for matching timestamp
    if (BasePicture* bp = m_basePicturePending.findUnordered(findBasePictureTimestamp, &frame->timestamp);
        bp) {
        frame->setBase(bp->picture, bp->deadline, bp->userData);
        m_basePicturePending.remove(bp);
        m_eventSink->generate(pipeline::EventCanSendBase);
    }

    startReadyFrames();
    return LdcReturnCodeSuccess;
}

LdcReturnCode PipelineCPU::sendBasePicture(uint64_t timestamp, LdpPicture* basePicture,
                                           uint32_t timeoutUs, void* userData)
{
    VNLogDebug("sendBasePicture: %" PRIx64 " %p", timestamp, (void*)basePicture);
    VNTraceInstant("sendBasePicture", timestamp);

    // Find the frame associated with PTS
    FrameCPU* frame{findFrame(timestamp)};
    if (frame) {
        // Enhancement exists
        if (LdcReturnCode ret = frame->setBase(
                basePicture, threadTimeMicroseconds(static_cast<int32_t>(timeoutUs)), userData);
            ret != LdcReturnCodeSuccess) {
            return ret;
        }

        // Kick off any frames that are at or before the base timestamp
        startProcessing(timestamp);
        m_eventSink->generate(pipeline::EventCanSendBase);
        return LdcReturnCodeSuccess;
    }

    BasePicture bp = {timestamp, basePicture,
                      threadTimeMicroseconds(static_cast<int32_t>(timeoutUs)), userData};

    if (m_basePicturePending.size() < m_configuration.enhancementDelay) {
        // Room to buffer picture
        m_basePicturePending.append(bp);
        return LdcReturnCodeSuccess;
    }

    // Cannot buffer any more pending bases
    if (m_configuration.passthroughMode == PassthroughMode::Disable) {
        // No pass-through
        return LdcReturnCodeAgain;
    }

    // Base frame is going to go through pipeline as some sort of pass-through ...
    if (!m_basePicturePending.isEmpty()) {
        m_basePicturePending.append(bp);
        bp = m_basePicturePending[0];
        m_basePicturePending.removeIndex(0);
        m_eventSink->generate(pipeline::EventCanSendBase);
    }

    // New pass-through frame - no enhancement
    FrameCPU* const passFrame{allocateFrame(timestamp)};

    if (!passFrame) {
        return LdcReturnCodeError;
    }

    // Add frame to reorder table sorted by timestamp
    passFrame->m_state = FrameStateReorder;
    passFrame->m_ready = true;
    passFrame->m_passthrough = true;
    passFrame->setBase(basePicture, threadTimeMicroseconds(static_cast<int32_t>(timeoutUs)), userData);

    m_reorderIndex.insert(sortFramePtrTimestamp, passFrame);

    startReadyFrames();
    return LdcReturnCodeSuccess;
}

LdcReturnCode PipelineCPU::sendOutputPicture(LdpPicture* outputPicture)
{
    VNLogDebug("sendOutputPicture: %p", (void*)outputPicture);
    VNTraceInstant("sendOutputPicture", (void*)outputPicture);

    // Add to available queue
    if (m_outputPictureAvailableBuffer.size() > m_configuration.maxLatency ||
        !m_outputPictureAvailableBuffer.tryPush(outputPicture)) {
        VNLogDebug("sendOutputPicture: AGAIN");
        return LdcReturnCodeAgain;
    }

    connectOutputPictures();

    startReadyFrames();
    return LdcReturnCodeSuccess;
}

LdpPicture* PipelineCPU::receiveOutputPicture(LdpDecodeInformation& decodeInfoOut)
{
    FrameCPU* frame{};

    // Pull any done frame from start (lowest timestamp) of 'processing' frame index.
    while (true) {
        common::ScopedLock lock(m_interTaskMutex);

        if (m_processingIndex.isEmpty()) {
            // No frames in progress
            break;
        }

        if (m_processingIndex[0]->m_state == FrameStateDone) {
            // Earliest frame is finished
            frame = m_processingIndex[0];
            m_processingIndex.removeIndex(0);
            break;
        }

        if (m_processingIndex.size() > m_configuration.minLatency && m_processingIndex[0]->canComplete()) {
            // Earliest frame will complete, so hang around and wait for it
            VNLogDebug("receiveOutputPicture waiting for %" PRIx64, m_processingIndex[0]->timestamp);

            if (m_interTaskFrameDone.waitDeadline(lock, m_processingIndex[0]->m_deadline)) {
                continue;
            }
            VNLogWarning("receiveOutputPicture wait timed out");
#ifdef VN_SDK_LOG_ENABLE_DEBUG
            ldcTaskPoolDump(&m_taskPool, nullptr);
#endif
        } else {
            break;
        }
    }

    if (!frame) {
        return nullptr;
    }

    // Copy surviving data from frame
    decodeInfoOut = frame->m_decodeInfo;
    LdpPicture* pictureOut{frame->outputPicture};

    VNLogDebug("receiveOutputPicture: %" PRIx64 " %p hb:%d he:%d sk:%d enh:%d",
               decodeInfoOut.timestamp, (void*)pictureOut, decodeInfoOut.hasBase,
               decodeInfoOut.hasEnhancement, decodeInfoOut.skipped, decodeInfoOut.enhanced);

    VNTraceInstant("receiveOutputPicture", frame->timestamp, (void*)frame->outputPicture);

    // Once an output picture has left the building - we can drop the associated frame
    freeFrame(frame);

    return pictureOut;
}

LdpPicture* PipelineCPU::receiveFinishedBasePicture()
{
    // Is there anything in finished base FIFO?
    LdpPicture* basePicture{};
    if (!m_basePictureOutBuffer.tryPop(basePicture)) {
        return nullptr;
    }

    VNLogDebug("receiveFinishedBasePicture: %" PRIx64 " %p", (void*)basePicture);
    VNTraceInstant("receiveFinishedBasePicture", (void*)basePicture);

    return basePicture;
}

// Dig out info about a current timestamp
LdcReturnCode PipelineCPU::peek(uint64_t timestamp, uint32_t& widthOut, uint32_t& heightOut)
{
    // Flush everything up to given timestamp
    startProcessing(timestamp);

    // Find the frame associated with PTS
    const FrameCPU* frame{findFrame(timestamp)};
    if (!frame) {
        return LdcReturnCodeNotFound;
    }
    if (!frame->globalConfig) {
        if (m_configuration.passthroughMode == PassthroughMode::Disable) {
            return LdcReturnCodeNotFound;
        } else {
            return LdcReturnCodeAgain;
        }
    }

    widthOut = frame->globalConfig->width;
    heightOut = frame->globalConfig->height;
    return LdcReturnCodeSuccess;
}

// Move any reorder frames at or before timestamp into processing state
void PipelineCPU::startProcessing(uint64_t timestamp)
{
    // Mark any frames in reorder buffer as 'flush'
    for (uint32_t i = 0; i < m_reorderIndex.size(); ++i) {
        FrameCPU* const frame = m_reorderIndex[i];
        if (frame->m_state == FrameStateReorder && compareTimestamps(frame->timestamp, timestamp) <= 0) {
            frame->m_ready = true;
        }
    }

    startReadyFrames();
}

// Make frames before timestamp as disposable
LdcReturnCode PipelineCPU::flush(uint64_t timestamp)
{
    VNLogDebug("flush: %" PRIx64 " %p", timestamp);
    VNTraceInstant("flush", timestamp);

    // Mark any frames in reorder buffer as 'flush'
    for (uint32_t i = 0; i < m_reorderIndex.size(); ++i) {
        FrameCPU* const frame = m_reorderIndex[i];
        if (compareTimestamps(frame->timestamp, timestamp) <= 0) {
            // Mark frame as flushable
            frame->m_ready = true;
        }
    }

    startReadyFrames();
    return LdcReturnCodeSuccess;
}

// Mark everything before timestamp as not needing decoding
LdcReturnCode PipelineCPU::skip(uint64_t timestamp)
{
    VNLogDebug("skip: %" PRIx64 " %p", timestamp);
    VNTraceInstant("skip", timestamp);

    // Look at all frames
    for (uint32_t i = 0; i < m_frames.size(); ++i) {
        FrameCPU* const frame{VNAllocationPtr(m_frames[i], FrameCPU)};
        if (compareTimestamps(frame->timestamp, timestamp) <= 0) {
            // Mark frame as skippable and flushable
            frame->m_skip = true;
            frame->m_ready = true;
        }
    }

    startReadyFrames();
    return LdcReturnCodeSuccess;
}

// Wait for all work to be finished - optionally stopping anything in progress
LdcReturnCode PipelineCPU::synchronize(bool dropPending)
{
    VNLogDebug("synchronize: %d", dropPending);
    VNTraceInstant("synchronize", dropPending);

    // Mark current frames as skippable
    for (uint32_t i = 0; i < m_frames.size(); ++i) {
        FrameCPU* const frame{VNAllocationPtr(m_frames[i], FrameCPU)};
        frame->m_skip = dropPending;
    }
    startReadyFrames();

    // For all pending frames that are not blocked on input - wait in timestamp order
    for (uint32_t i = 0; i < m_processingIndex.size(); ++i) {
        FrameCPU* frame = m_processingIndex[i];
        if (!frame->canComplete()) {
            continue;
        }
        ldcTaskGroupWait(&frame->m_taskGroup);
    }

    return LdcReturnCodeSuccess;
}

// Buffers
//
BufferCPU* PipelineCPU::allocateBuffer(uint32_t requiredSize)
{
    // Allocate buffer structure
    LdcMemoryAllocation allocation;
    BufferCPU* const buffer{VNAllocateZero(m_allocator, &allocation, BufferCPU)};
    if (!buffer) {
        return nullptr;
    }
    // Insert into table
    m_buffers.append(allocation);

    // In place construction
    return new (buffer) BufferCPU(*this, requiredSize); // NOLINT(cppcoreguidelines-owning-memory)
}

void PipelineCPU::releaseBuffer(BufferCPU* buffer)
{
    assert(buffer);

    // Release buffer structure
    LdcMemoryAllocation* const pAlloc{m_buffers.findUnordered(ldcVectorCompareAllocationPtr, buffer)};

    if (!pAlloc) {
        // Could not find picture!
        VNLogWarning("Could not find buffer to release: %p", (void*)buffer);
        return;
    }

    // Call destructor directly, as we are doing in-place construct/destruct
    buffer->~BufferCPU();

    // Release memory
    VNFree(m_allocator, pAlloc);

    m_buffers.removeReorder(pAlloc);
}

// Pictures
//

// Internal allocation
PictureCPU* PipelineCPU::allocatePicture()
{
    // Allocate picture
    LdcMemoryAllocation pictureAllocation;
    PictureCPU* picture{VNAllocateZero(m_allocator, &pictureAllocation, PictureCPU)};
    if (!picture) {
        return nullptr;
    }
    // Insert into table
    m_pictures.append(pictureAllocation);

    // In place construction
    return new (picture) PictureCPU(*this); // NOLINT(cppcoreguidelines-owning-memory)
}

void PipelineCPU::releasePicture(PictureCPU* picture)
{
    picture->unbindMemory();

    // Find slot
    LdcMemoryAllocation* pAlloc{m_pictures.findUnordered(ldcVectorCompareAllocationPtr, picture)};

    if (!pAlloc) {
        // Could not find picture!
        VNLogWarning("Could not find picture to release: %p", (void*)picture);
        return;
    }

    // Call destructor directly, as we are doing in-place construct/destruct
    picture->~PictureCPU();

    // Release memory
    VNFree(m_allocator, pAlloc);

    m_pictures.removeReorder(pAlloc);
}

LdpPicture* PipelineCPU::allocPictureManaged(const LdpPictureDesc& desc)
{
    PictureCPU* picture{allocatePicture()};
    picture->setDesc(desc);
    return picture;
}

LdpPicture* PipelineCPU::allocPictureExternal(const LdpPictureDesc& desc,
                                              const LdpPicturePlaneDesc* planeDescArr,
                                              const LdpPictureBufferDesc* buffer)
{
    PictureCPU* picture{allocatePicture()};
    picture->setDesc(desc);
    picture->setExternal(planeDescArr, buffer);
    return picture;
}

void PipelineCPU::freePicture(LdpPicture* ldpPicture)
{
    // Get back to derived Picture class
    PictureCPU* picture{static_cast<PictureCPU*>(ldpPicture)};
    assert(ldpPicture);

    releasePicture(picture);
}

// Frames
//

// Allocate or find working data for a timestamp
//
// Given that there is going to be in the order of 100 or less frames, stick
// with an array and linear searches.
//
// Returns nullptr if there is no capacity for another frame.
//
FrameCPU* PipelineCPU::allocateFrame(uint64_t timestamp)
{
    assert(findFrame(timestamp) == nullptr);
    assert(m_frames.size() < m_configuration.maxLatency);

    // Allocate frame with in place construction
    LdcMemoryAllocation frameAllocation = {};
    FrameCPU* const frame{VNAllocateZero(m_allocator, &frameAllocation, FrameCPU)};
    if (!frame) {
        return nullptr;
    }

    // Append allocation into table
    m_frames.append(frameAllocation);

    // In place construction
    return new (frame) FrameCPU(this, timestamp); // NOLINT(cppcoreguidelines-owning-memory)
}

// Find existing Frame for a timestamp, or return nullptr if it does not exist.
//
FrameCPU* PipelineCPU::findFrame(uint64_t timestamp)
{
    // Look for frame
    if (const LdcMemoryAllocation * pAlloc{m_frames.findUnordered(findFrameTimestamp, &timestamp)}) {
        return VNAllocationPtr(*pAlloc, FrameCPU);
    }

    return nullptr;
}

// Release frame back to pool
//
void PipelineCPU::freeFrame(FrameCPU* frame)
{
    // Release task group and allocations
    frame->release();

    // Find slot
    LdcMemoryAllocation* frameAlloc{m_frames.findUnordered(ldcVectorCompareAllocationPtr, frame)};

    if (!frameAlloc) {
        // Could not find picture!
        VNLogWarning("Could not find frame to release: %p", (void*)frame);
        return;
    }

    // Call destructor directly, as we are doing in-place construct/destruct
    frame->~FrameCPU();

    // Release memory
    VNFree(m_allocator, frameAlloc);

    m_frames.removeReorder(frameAlloc);
}

// Number of outstanding frames
uint32_t PipelineCPU::frameLatency() const
{
    return m_reorderIndex.size() + m_processingIndex.size();
}

//// Frame start
//
// Get the next frame, if any, in timestamp order - taking into account reorder and flushing.
//
FrameCPU* PipelineCPU::getNextReordered()
{
    // Are there any frames at all?
    if (m_reorderIndex.isEmpty()) {
        return nullptr;
    }

    // If exceeded reorder limit, or flushing
    if (m_reorderIndex.size() >= m_maxReorder || m_reorderIndex[0]->m_ready) {
        FrameCPU* const frame{m_reorderIndex[0]};
        m_reorderIndex.removeIndex(0);
        // Tell API there is enhancement space
        m_eventSink->generate(pipeline::EventCanSendEnhancement);
        return frame;
    }

    return nullptr;
}

// Resolve ready frame configurations in timestamp order, and generate tasks for each one.
//
// Once we are handling frames here, the frame is in flight - async to the API, so no error returns.
//
void PipelineCPU::startReadyFrames()
{
    // Pull ready frames from reorder table
    while (FrameCPU* frame = getNextReordered()) {
        const uint64_t timestamp{frame->timestamp};
        bool goodConfig = false;

        if (m_previousTimestamp != kInvalidTimestamp &&
            compareTimestamps(m_previousTimestamp, timestamp) > 0) {
            // Frame has been flushed out of reorder queue too late - mark as pass- through
            VNLogDebug("startReadyFrames: out of order: ts:%" PRIx64 " prev: %" PRIx64);
            frame->m_passthrough = true;
        }

        if (!frame->m_passthrough) {
            // Parse the LCEVC configuration into distinct per-frame data
            // Switch to pass-through if configuration parse failed.
            goodConfig = ldeConfigPoolFrameInsert(&m_configPool, timestamp,
                                                  VNAllocationPtr(frame->m_enhancementData, uint8_t),
                                                  VNAllocationSize(frame->m_enhancementData, uint8_t),
                                                  &frame->globalConfig, &frame->config);

            if (!goodConfig) {
                frame->m_passthrough = true;
            }
        }

        if (frame->m_passthrough) {
            // Set up enough frame configuration to support pass-through
            ldeConfigPoolFramePassthrough(&m_configPool, &frame->globalConfig, &frame->config);
        }

        VNLogDebug("Start Frame: %" PRIx64 " goodConfig:%d temporalEnabled:%d, temporalPresent:%d "
                   "temporalRefresh:%d loqEnabled[0]:%d loqEnabled[1]:%d passthrough:%d",
                   timestamp, goodConfig, frame->globalConfig->temporalEnabled,
                   frame->config.temporalSignallingPresent, frame->config.temporalRefresh,
                   frame->config.loqEnabled[0], frame->config.loqEnabled[1], frame->m_passthrough);

        // Once we have per frame configuration, we can properly initialize and figure out tasks for the frame
        if (!frame->initialize()) {
            VNLogError("Could not allocate frame buffers: %" PRIx64, frame->timestamp);
            // Could not allocate buffers - switch to pass-through
            frame->m_passthrough = true;
        }

        // All good - make tasks, and add to processing index
        // with frame it should get temporal from if it needs it

        {
            common::ScopedLock lock(m_interTaskMutex);
            frame->m_state = FrameStateProcessing;
            m_processingIndex.append(frame);
        }

        frame->generateTasks(m_lastGoodTimestamp);

        // Remember timestamps for next time
        m_previousTimestamp = timestamp;
        if (goodConfig) {
            m_lastGoodTimestamp = timestamp;
        }
    }

    // Connect available output pictures to started pictures
    connectOutputPictures();
}

// Connect any available output pictures to frames that can use them
//
void PipelineCPU::connectOutputPictures()
{
    // While there are available output pictures and pending frames,
    // go through frames in timestamp order, assigning next output picture
    while (true) {
        FrameCPU* frame{};

        if (m_outputPictureAvailableBuffer.isEmpty()) {
            // No output pictures left
            break;
        }

        // Find next in process frame with base data, and without an assigned output picture
        {
            common::ScopedLock lock(m_interTaskMutex);

            for (uint32_t idx = 0; idx < m_processingIndex.size(); ++idx) {
                if (!m_processingIndex[idx]->outputPicture && m_processingIndex[idx]->baseDataValid()) {
                    frame = m_processingIndex[idx];
                    break;
                }
            }
        }

        if (!frame) {
            // No frames without output pictures left
            break;
        }

        //  Get the picture
        LdpPicture* ldpPicture{};
        m_outputPictureAvailableBuffer.pop(ldpPicture);
        assert(ldpPicture);

        // Set the output layout
        const LdpPictureDesc desc{frame->getOutputPictureDesc()};
        ldpPictureSetDesc(ldpPicture, &desc);
        if (frame->globalConfig->cropEnabled) {
            ldpPicture->margins.left = frame->globalConfig->crop.left;
            ldpPicture->margins.right = frame->globalConfig->crop.right;
            ldpPicture->margins.top = frame->globalConfig->crop.top;
            ldpPicture->margins.bottom = frame->globalConfig->crop.bottom;
        }

        // Poke it into the frame's task group
        frame->outputPicture = ldpPicture;

        VNLogDebug("connectOutputPicture: %" PRIx64 " %p %ux%u (r:%d p:%d o:%d)", frame->timestamp,
                   (void*)ldpPicture, desc.width, desc.height, m_reorderIndex.size(),
                   m_processingIndex.size(), m_outputPictureAvailableBuffer.size());
        ldcTaskDependencyMet(&frame->m_taskGroup, frame->m_depOutputPicture, ldpPicture);

        // Tell API there is output picture space

        m_eventSink->generate(pipeline::EventCanSendPicture);
    }
}

//// Temporal
//
// Mark a frame as needing a temporal buffer of given timestamp and dimensions
//
// This may be resolved immediately if the previous frame is done already, otherwise
// the buffer will be connected later when another frame releases it.
//
LdcTaskDependency PipelineCPU::requireTemporalBuffer(FrameCPU* frame, uint64_t timestamp, uint32_t plane)
{
    LdcTaskDependency dep{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    uint32_t width = frame->globalConfig->width;
    uint32_t height = frame->globalConfig->height;
    width >>= ldpColorFormatPlaneWidthShift(frame->baseFormat, plane);
    height >>= ldpColorFormatPlaneHeightShift(frame->baseFormat, plane);

    // Fill in requirements
    frame->m_temporalBufferDesc[plane].timestamp = timestamp;
    frame->m_temporalBufferDesc[plane].clear =
        frame->config.nalType == NTIDR || frame->config.temporalRefresh;
    frame->m_temporalBufferDesc[plane].width = width;
    frame->m_temporalBufferDesc[plane].height = height;
    frame->m_temporalBufferDesc[plane].plane = plane;

    frame->m_depTemporalBuffer[plane] = dep;

    VNLogDebug("requireTemporalBuffer: %" PRIx64 " wants %" PRIx64 " plane %" PRIu32 " (%d %dx%d)",
               frame->timestamp, timestamp, plane, frame->m_temporalBufferDesc[plane].clear, width,
               height);

    if (TemporalBuffer* temporalBuffer = matchTemporalBuffer(frame, plane)) {
        ldcTaskDependencyMet(&frame->m_taskGroup, dep, temporalBuffer);
    }

    return dep;
}

TemporalBuffer* PipelineCPU::matchTemporalBuffer(FrameCPU* frame, uint32_t plane)
{
    TemporalBuffer* foundTemporalBuffer{};
    const uint64_t timestamp = frame->m_temporalBufferDesc[plane].timestamp;

    {
        common::ScopedLock lock(m_interTaskMutex);

        // Do any of the available temporal buffers meet the requirements?
        for (uint32_t i = 0; i < m_temporalBuffers.size(); ++i) {
            TemporalBuffer* tb{m_temporalBuffers.at(i)};
            if (tb->frame) {
                // In use
                continue;
            }

            if (tb->desc.plane == plane && tb->desc.timestamp == timestamp) {
                // Exact plane index and timestamp match
                foundTemporalBuffer = tb;
                break;
            }

            if (frame->m_temporalBufferDesc[plane].clear && tb->desc.plane == plane &&
                tb->desc.timestamp == kInvalidTimestamp) {
                // An existing unused buffer
                foundTemporalBuffer = tb;
                break;
            }
        }

        // Got one - mark it as in use
        if (foundTemporalBuffer) {
            frame->m_temporalBuffer[plane] = foundTemporalBuffer;
            foundTemporalBuffer->frame = frame;

            if (frame->m_temporalBufferDesc[plane].clear) {
                // Update limit on any other prior buffers
                for (uint32_t i = 0; i < m_temporalBuffers.size(); ++i) {
                    TemporalBuffer* tb = m_temporalBuffers.at(i);
                    if (tb == foundTemporalBuffer) {
                        continue;
                    }
                }
            }
        }
    }

    if (!foundTemporalBuffer) {
        // Not found - will get resolved later by prior frame
        return nullptr;
    }

    VNLogDebug("  matchTemporalBuffer found: plane=%" PRIu32 " frame=%" PRIx64 " prev=%" PRIx64,
               plane, frame->timestamp, foundTemporalBuffer->desc.timestamp);

    // Make sure found buffer meets requirements
    updateTemporalBufferDesc(foundTemporalBuffer, frame->m_temporalBufferDesc[plane]);

    return foundTemporalBuffer;
}

// Mark the frame as having finished with it's temporal buffer, and possibly
// hand buffer on to another frame
//
void PipelineCPU::releaseTemporalBuffer(FrameCPU* frame, uint32_t plane)
{
    VNLogDebug("releaseTemporalBuffer: %" PRIx64 " plane: %" PRIu32, frame->timestamp, plane);

    FrameCPU* foundNextFrame{nullptr};
    TemporalBuffer* const tb{frame->m_temporalBuffer[plane]};

    {
        common::ScopedLock lock(m_interTaskMutex);

        if (!tb) {
            // No temporal buffer to be released
            return;
        }

        // Detach from frame
        frame->m_temporalBuffer[plane] = nullptr;
        tb->frame = nullptr;

        tb->desc.timestamp = frame->timestamp;

        // Do any of the pending frames want this buffer?
        for (uint32_t idx = 0; idx < m_processingIndex.size(); ++idx) {
            FrameCPU* nextFrame{m_processingIndex[idx]};
            if (tb->desc.timestamp == nextFrame->m_temporalBufferDesc[plane].timestamp &&
                tb->desc.plane == nextFrame->m_temporalBufferDesc[plane].plane) {
                // Matches this frame
                foundNextFrame = nextFrame;
                break;
            }
        }

        if (foundNextFrame) {
            foundNextFrame->m_temporalBuffer[plane] = tb;
            tb->frame = foundNextFrame;
        }
    }

    if (foundNextFrame) {
        VNLogDebug("  CPU::releaseTemporalBuffer found: plane=%" PRIu32 " frame=%" PRIx64
                   " prev=%" PRIx64,
                   plane, foundNextFrame->timestamp, frame->timestamp);
        updateTemporalBufferDesc(tb, foundNextFrame->m_temporalBufferDesc[plane]);
        ldcTaskDependencyMet(&foundNextFrame->m_taskGroup, foundNextFrame->m_depTemporalBuffer[plane], tb);
    }
}

// Make a temporal buffer match the given description
void PipelineCPU::updateTemporalBufferDesc(TemporalBuffer* buffer, const TemporalBufferDesc& desc) const
{
    const size_t paddedWidth = alignU32(desc.width, kBufferRowAlignment);
    const size_t byteStride{paddedWidth * sizeof(uint16_t)};
    const size_t bufferSize{byteStride * static_cast<size_t>(desc.height)};

    if (!VNIsAllocated(buffer->allocation) || buffer->desc.width != desc.width ||
        buffer->desc.height != desc.height) {
        // Reallocate buffer
        if (!desc.clear && desc.timestamp != kInvalidTimestamp) {
            // Frame was expecting prior residuals - but dimensions are wrong!?
            VNLogWarning("Temporal buffer does not match: %08d Got %dx%d, Wanted %dx%d", desc.timestamp,
                         buffer->desc.width, buffer->desc.height, desc.width, desc.height);
        }
        buffer->planeDesc.firstSample = VNAllocateAlignedZeroArray(
            allocator(), &buffer->allocation, uint8_t, kBufferRowAlignment, bufferSize);
        buffer->planeDesc.rowByteStride = static_cast<uint32_t>(byteStride);
        memset(buffer->planeDesc.firstSample, 0, bufferSize);
    } else if (desc.clear) {
        memset(buffer->planeDesc.firstSample, 0, bufferSize);
    }

    // Update description
    buffer->desc = desc;
    buffer->desc.clear = false;
}

//// ConvertToInternal
//
// Copy incoming picture plane to internal fixed point surface format
//
// NB: There is likely a good templated C++ class that wraps these tasks up neatly,
// Worth figuring out once this has stabilised.
//
struct TaskConvertToInternalData
{
    PipelineCPU* pipeline;
    FrameCPU* frame;
    uint32_t planeIndex;
    uint32_t baseDepth;
    uint32_t enhancementDepth;
};

void* PipelineCPU::taskConvertToInternal(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskConvertToInternalData));

    const TaskConvertToInternalData& data{VNTaskData(task, TaskConvertToInternalData)};
    PipelineCPU* const pipeline{data.pipeline};
    FrameCPU* const frame{data.frame};

    if (frame->m_skip) {
        return nullptr;
    }

    bool isNV12 = frame->basePicture->layout.layoutInfo->format == LdpColorFormatNV12_8;
    uint32_t srcPlaneIndex = (isNV12 && data.planeIndex == 2) ? 1 : data.planeIndex;
    LdpPicturePlaneDesc srcPlane;
    frame->getBasePlaneDesc(srcPlaneIndex, srcPlane);

    // Intermediate buffers are set up so that unused ones point to higher LoQs - so requesting
    // LOQ2 will pick up the correct 'input' buffer
    LdpPicturePlaneDesc dstPlane;
    frame->getIntermediatePlaneDesc(data.planeIndex, LOQ2, dstPlane);

    VNLogDebug("taskConvertToInternal timestamp:%" PRIx64 " plane:%d enhanced:%d",
               data.frame->timestamp, data.planeIndex);

    if (!ldppPlaneBlit(&pipeline->m_taskPool, task, pipeline->m_configuration.forceScalar,
                       data.planeIndex, &frame->basePicture->layout,
                       &frame->m_intermediateLayout[LOQ2], &srcPlane, &dstPlane, BMCopy)) {
        VNLogError("ldppPlaneBlit In failed");
    }
    return nullptr;
}

LdcTaskDependency PipelineCPU::addTaskConvertToInternal(FrameCPU* frame, uint32_t planeIndex,
                                                        uint32_t baseDepth, uint32_t enhancementDepth,
                                                        LdcTaskDependency inputDep)
{
    const TaskConvertToInternalData data{this, frame, planeIndex, baseDepth, enhancementDepth};
    const LdcTaskDependency inputs[] = {inputDep};
    const LdcTaskDependency outputDep{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), outputDep,
                    taskConvertToInternal, nullptr, 1, 1, sizeof(data), &data, "ConvertToInternal");

    return outputDep;
}

//// ConvertFromInternal
//
// Concert a picture plane from internal fixed point to output picture pixel format.
//
struct TaskConvertFromInternalData
{
    PipelineCPU* pipeline;
    FrameCPU* frame;
    uint32_t planeIndex;
    uint32_t baseDepth;
    uint32_t enhancementDepth;
};

void* PipelineCPU::taskConvertFromInternal(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskConvertFromInternalData));

    const TaskConvertFromInternalData& data{VNTaskData(task, TaskConvertFromInternalData)};
    PipelineCPU* const pipeline{data.pipeline};
    FrameCPU* const frame{data.frame};

    if (frame->m_skip) {
        return nullptr;
    }

    LdpPicturePlaneDesc srcPlane;
    frame->getIntermediatePlaneDesc(data.planeIndex, LOQ0, srcPlane);

    bool isNV12 = frame->outputPicture->layout.layoutInfo->format == LdpColorFormatNV12_8;
    uint32_t dstPlaneIndex = (isNV12 && data.planeIndex == 2) ? 1 : data.planeIndex;
    LdpPicturePlaneDesc dstPlane;
    frame->getOutputPlaneDesc(dstPlaneIndex, dstPlane);

    VNLogDebug("taskConvertFromInternal timestamp:%" PRIx64 " plane:%d", data.frame->timestamp,
               data.planeIndex);

    if (!ldppPlaneBlit(&pipeline->m_taskPool, task, pipeline->m_configuration.forceScalar,
                       data.planeIndex, &frame->m_intermediateLayout[LOQ0],
                       &frame->outputPicture->layout, &srcPlane, &dstPlane, BMCopy)) {
        VNLogError("ldppPlaneBlit out failed");
    }

    return nullptr;
}

LdcTaskDependency PipelineCPU::addTaskConvertFromInternal(FrameCPU* frame, uint32_t planeIndex,
                                                          uint32_t baseDepth, uint32_t enhancementDepth,
                                                          LdcTaskDependency dst, LdcTaskDependency src)
{
    const TaskConvertFromInternalData data{this, frame, planeIndex, baseDepth, enhancementDepth};
    const LdcTaskDependency inputs[] = {dst, src};
    const LdcTaskDependency output = ldcTaskDependencyAdd(&frame->m_taskGroup);

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), output, taskConvertFromInternal,
                    nullptr, 1, 1, sizeof(data), &data, "ConvertFromInternal");

    return output;
}

//// Upsample
//
// Upscale (1D or 2D) for one plane of picture.
//
// Inputs and outputs may be fixed point or 'external' format if no residuals are being applied.
//
struct TaskUpsampleData
{
    PipelineCPU* pipeline;
    FrameCPU* frame;
    LdeLOQIndex fromLoq;
    uint32_t plane;
};

void* PipelineCPU::taskUpsample(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskUpsampleData));

    const TaskUpsampleData& data{VNTaskData(task, TaskUpsampleData)};
    PipelineCPU* const pipeline{data.pipeline};
    FrameCPU* const frame{data.frame};

    if (frame->m_skip) {
        return nullptr;
    }

    LdppUpscaleArgs upscaleArgs{};

    const LdeLOQIndex loq = data.fromLoq;
    assert(loq > LOQ0);
    upscaleArgs.srcLayout = &frame->m_intermediateLayout[loq];
    frame->getIntermediatePlaneDesc(data.plane, loq, upscaleArgs.srcPlane);

    upscaleArgs.dstLayout = &frame->m_intermediateLayout[loq - 1];
    frame->getIntermediatePlaneDesc(data.plane, static_cast<LdeLOQIndex>(loq - 1), upscaleArgs.dstPlane);

    upscaleArgs.planeIndex = data.plane;
    upscaleArgs.applyPA = frame->globalConfig->predictedAverageEnabled;
    upscaleArgs.frameDither = frame->m_frameDither.strength ? &frame->m_frameDither : NULL;
    upscaleArgs.mode = frame->globalConfig->scalingModes[data.fromLoq - 1];
    upscaleArgs.forceScalar = pipeline->m_configuration.forceScalar;

    assert(upscaleArgs.mode != Scale0D);
    VNLogDebug("taskUpsample timestamp:%" PRIx64 " loq:%d plane:%d", frame->timestamp,
               (uint32_t)data.fromLoq, data.plane);

    if (!ldppUpscale(pipeline->allocator(), &pipeline->m_taskPool, task,
                     &frame->globalConfig->kernel, &upscaleArgs)) {
        VNLogError("Upsample failed");
    }

    return nullptr;
}

LdcTaskDependency PipelineCPU::addTaskUpsample(FrameCPU* frame, LdeLOQIndex fromLoq, uint32_t plane,
                                               LdcTaskDependency src)
{
    assert(fromLoq > LOQ0);
    assert(frame->globalConfig->scalingModes[fromLoq - 1] != Scale0D);

    const TaskUpsampleData data{this, frame, fromLoq, plane};
    const LdcTaskDependency inputs[] = {src};
    const LdcTaskDependency output{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), output, taskUpsample, nullptr,
                    1, 1, sizeof(data), &data, "Upsample");

    return output;
}

//// GenerateCmdBuffer
//
// Convert un-encapsulated chunks into a single command buffer.
//
struct TaskGenerateCmdBufferData
{
    PipelineCPU* pipeline;
    FrameCPU* frame;
    LdpEnhancementTile* enhancementTile;
};

void* PipelineCPU::taskGenerateCmdBuffer(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskGenerateCmdBufferData));

    const TaskGenerateCmdBufferData& data{VNTaskData(task, TaskGenerateCmdBufferData)};
    FrameCPU* const frame{data.frame};

    VNLogDebug("taskGenerateCmdBuffer timestamp:%" PRIx64 " tile:%d loq:%d plane:%d",
               data.frame->timestamp, data.enhancementTile->tile,
               (uint32_t)data.enhancementTile->loq, data.enhancementTile->plane);

    if (!ldeDecodeEnhancement(frame->globalConfig, &frame->config, data.enhancementTile->loq,
                              data.enhancementTile->plane, data.enhancementTile->tile,
                              &data.enhancementTile->buffer, nullptr, nullptr)) {
        VNLogError("ldeDecodeEnhancement failed");
    }

    return nullptr;
}

LdcTaskDependency PipelineCPU::addTaskGenerateCmdBuffer(FrameCPU* frame, LdpEnhancementTile* enhancementTile)
{
    const TaskGenerateCmdBufferData data{this, frame, enhancementTile};
    const LdcTaskDependency outputDep{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, nullptr, 0, outputDep, taskGenerateCmdBuffer, nullptr, 1,
                    1, sizeof(data), &data, "GenerateCmdBuffer");

    return outputDep;
}

//// ApplyCmdBufferDirect
//
// Apply a generated CPU command buffer to directly to output plane. (No Temporal)
//
// NB: The output plane will be in 'internal' fixed point format
//
struct TaskApplyCmdBufferDirectData
{
    PipelineCPU* pipeline;
    FrameCPU* frame;
    LdpEnhancementTile* enhancementTile;
};

void* PipelineCPU::taskApplyCmdBufferDirect(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskApplyCmdBufferDirectData));

    const TaskApplyCmdBufferDirectData& data{VNTaskData(task, TaskApplyCmdBufferDirectData)};
    PipelineCPU* const pipeline{data.pipeline};
    FrameCPU* const frame{data.frame};

    if (frame->m_skip) {
        return nullptr;
    }

    VNLogDebug("taskApplyCmdBufferDirect timestamp:%" PRIx64 " loq:%d plane:%d", data.frame->timestamp,
               (uint32_t)data.enhancementTile->loq, data.enhancementTile->plane);

    LdpPicturePlaneDesc ppDesc{};

    frame->getIntermediatePlaneDesc(data.enhancementTile->plane, data.enhancementTile->loq, ppDesc);

    const bool tuRasterOrder =
        !frame->globalConfig->temporalEnabled && frame->globalConfig->tileDimensions == TDTNone;

    if (!ldppApplyCmdBuffer(&pipeline->m_taskPool, NULL, data.enhancementTile, LdpFPS14, &ppDesc,
                            tuRasterOrder, pipeline->m_configuration.forceScalar,
                            pipeline->m_configuration.highlightResiduals)) {
        VNLogError("taskApplyCmdBufferDirect failed");
    }

    return nullptr;
}

LdcTaskDependency PipelineCPU::addTaskApplyCmdBufferDirect(FrameCPU* frame,
                                                           LdpEnhancementTile* enhancementTile,
                                                           LdcTaskDependency imageBuffer,
                                                           LdcTaskDependency cmdBuffer)
{
    const TaskApplyCmdBufferDirectData data{this, frame, enhancementTile};

    const LdcTaskDependency inputs[] = {imageBuffer, cmdBuffer};
    const LdcTaskDependency output{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), output, taskApplyCmdBufferDirect,
                    nullptr, 1, 1, sizeof(data), &data, "ApplyCmdBufferDirect");

    return output;
}

//// ApplyCmdBufferTemporal
//
// Apply a generated CPU command buffer to a temporal buffer.
//
struct TaskApplyCmdBufferTemporalData
{
    PipelineCPU* pipeline;
    FrameCPU* frame;
    LdpEnhancementTile* enhancementTile;
};

void* PipelineCPU::taskApplyCmdBufferTemporal(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskApplyCmdBufferTemporalData));

    const TaskApplyCmdBufferTemporalData& data{VNTaskData(task, TaskApplyCmdBufferTemporalData)};
    PipelineCPU* const pipeline{data.pipeline};
    FrameCPU* const frame{data.frame};

    VNLogDebug("taskApplyCmdBufferTemporal timestamp:%" PRIx64 " tile:%d loq:%d plane:%d",
               data.frame->timestamp, data.enhancementTile->tile,
               (uint32_t)data.enhancementTile->loq, data.enhancementTile->plane);

    LdpPicturePlaneDesc ppDesc{frame->m_temporalBuffer[data.enhancementTile->plane]->planeDesc};

    if (!ldppApplyCmdBuffer(&pipeline->m_taskPool, NULL, data.enhancementTile, LdpFPS14, &ppDesc,
                            false, pipeline->m_configuration.forceScalar,
                            pipeline->m_configuration.highlightResiduals)) {
        VNLogError("ldppApplyCmdBufferTemporal failed");
    }
    return nullptr;
}

LdcTaskDependency PipelineCPU::addTaskApplyCmdBufferTemporal(FrameCPU* frame,
                                                             LdpEnhancementTile* enhancementTile,
                                                             LdcTaskDependency temporalBuffer,
                                                             LdcTaskDependency cmdBuffer)
{
    const TaskApplyCmdBufferTemporalData data{this, frame, enhancementTile};

    const LdcTaskDependency inputs[] = {temporalBuffer, cmdBuffer};
    const LdcTaskDependency output{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), output, taskApplyCmdBufferTemporal,
                    nullptr, 1, 1, sizeof(data), &data, "ApplyCmdBufferTemporal");

    return output;
}

//// ApplyAddTemporal
//
// Add a temporal buffer to a picture plane.
//
struct TaskApplyAddTemporalData
{
    PipelineCPU* pipeline;
    FrameCPU* frame;
    uint32_t planeIndex;
};

void* PipelineCPU::taskApplyAddTemporal(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskApplyAddTemporalData));

    const TaskApplyAddTemporalData& data{VNTaskData(task, TaskApplyAddTemporalData)};
    PipelineCPU* const pipeline{data.pipeline};
    FrameCPU* const frame{data.frame};

    if (frame->m_skip || frame->m_passthrough) {
        pipeline->releaseTemporalBuffer(frame, data.planeIndex);
        return nullptr;
    }

    VNLogDebug("taskApplyAddTemporal timestamp:%" PRIx64 " plane:%d", data.frame->timestamp, data.planeIndex);

    LdpPicturePlaneDesc dstPlane{};
    frame->getIntermediatePlaneDesc(data.planeIndex, LOQ0, dstPlane);

    if (!ldppPlaneBlit(&pipeline->m_taskPool, task, pipeline->m_configuration.forceScalar, data.planeIndex,
                       &frame->m_intermediateLayout[LOQ0], &frame->m_intermediateLayout[LOQ0],
                       &frame->m_temporalBuffer[data.planeIndex]->planeDesc, &dstPlane, BMAdd)) {
        VNLogError("ldppPlaneBlit out failed");
    }

    return nullptr;
}

LdcTaskDependency PipelineCPU::addTaskApplyAddTemporal(FrameCPU* frame, uint32_t planeIndex,
                                                       LdcTaskDependency temporalBuffer,
                                                       LdcTaskDependency imageBuffer)
{
    const TaskApplyAddTemporalData data{this, frame, planeIndex};

    const LdcTaskDependency inputs[] = {temporalBuffer, imageBuffer};
    const LdcTaskDependency output{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), output, taskApplyAddTemporal,
                    nullptr, 1, 1, sizeof(data), &data, "ApplyAddTemporal");

    return output;
}

//// Passthrough
//
// Copy incoming picture plane to output picture
//
struct TaskPassthroughData
{
    PipelineCPU* pipeline;
    FrameCPU* frame;
    uint32_t planeIndex;
};

void* PipelineCPU::taskPassthrough(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskPassthroughData));

    const TaskPassthroughData& data{VNTaskData(task, TaskPassthroughData)};
    PipelineCPU* const pipeline{data.pipeline};
    const FrameCPU* const frame{data.frame};

    if (frame->m_skip) {
        return nullptr;
    }

    // Check if this plane is valid
    if (data.planeIndex >= ldpPictureLayoutPlanes(&frame->basePicture->layout)) {
        return nullptr;
    }

    LdpPicturePlaneDesc srcPlane;
    frame->getBasePlaneDesc(data.planeIndex, srcPlane);

    LdpPicturePlaneDesc dstPlane;
    frame->getOutputPlaneDesc(data.planeIndex, dstPlane);

    VNLogDebug("taskPassthrough timestamp:%" PRIx64 " plane:%d", data.frame->timestamp, data.planeIndex);

    if (!ldppPlaneBlit(&pipeline->m_taskPool, task, pipeline->m_configuration.forceScalar,
                       data.planeIndex, &frame->basePicture->layout, &frame->outputPicture->layout,
                       &srcPlane, &dstPlane, BMCopy)) {
        VNLogError("ldppPlaneBlit In failed");
    }

    return nullptr;
}

LdcTaskDependency PipelineCPU::addTaskPassthrough(FrameCPU* frame, uint32_t planeIndex,
                                                  LdcTaskDependency dest, LdcTaskDependency src)
{
    const TaskPassthroughData data{this, frame, planeIndex};
    const LdcTaskDependency inputs[] = {dest, src};
    const LdcTaskDependency output{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), output, taskPassthrough,
                    nullptr, 1, 1, sizeof(data), &data, "Passthrough");

    return output;
}

//// WaitForMany
//
// Wait for several input dependencies to be met.
//
// NB: If this appears to be a bottleneck, it could be integrated better into the task pool.
//
struct TaskWaitForManyData
{
    PipelineCPU* pipeline;
    FrameCPU* frame;
};

void* PipelineCPU::taskWaitForMany(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskWaitForManyData));

    VNLogDebug("taskWaitForMany timestamp:%" PRIx64 "",
               VNTaskData(task, TaskWaitForManyData).frame->timestamp);
    return nullptr;
}

LdcTaskDependency PipelineCPU::addTaskWaitForMany(FrameCPU* frame, const LdcTaskDependency* inputs,
                                                  uint32_t inputsCount)
{
    const TaskWaitForManyData data{this, frame};
    const LdcTaskDependency outputDep{ldcTaskDependencyAdd(&frame->m_taskGroup)};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, inputsCount, outputDep, taskWaitForMany, nullptr,
                    1, 1, sizeof(data), &data, "WaitForMany");

    return outputDep;
}

//// BaseDone
//
// Wait for base picture planes to be used, then send base picture back to client
//
struct TaskBaseDoneData
{
    PipelineCPU* pipeline;
    FrameCPU* frame;
};

void* PipelineCPU::taskBaseDone(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskBaseDoneData));

    const TaskBaseDoneData& data{VNTaskData(task, TaskBaseDoneData)};

    VNLogDebug("taskBaseDone timestamp:%" PRIx64, data.frame->timestamp);
    assert(data.frame->basePicture);

    // Generate event
    data.pipeline->m_eventSink->generate(pipeline::EventBasePictureDone, data.frame->basePicture);

    // Send base picture back to API
    data.pipeline->m_basePictureOutBuffer.push(data.frame->basePicture);

    // Frame no longer has access to base picture
    data.frame->basePicture = nullptr;
    return nullptr;
}

void PipelineCPU::addTaskBaseDone(FrameCPU* frame, const LdcTaskDependency* inputs, uint32_t inputsCount)
{
    const TaskBaseDoneData data{this, frame};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, inputsCount, kTaskDependencyInvalid, taskBaseDone,
                    nullptr, 1, 1, sizeof(data), &data, "BaseDone");
}

//// OutputSend
//
// Wait for a bunch of input dependencies to be met, then:
//
// - Send output picture to output queue
// - Release frame
//
struct TaskOutputDoneData
{
    PipelineCPU* pipeline;
    FrameCPU* frame;
};

void* PipelineCPU::taskOutputDone(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskOutputDoneData));

    const TaskOutputDoneData& data{VNTaskData(task, TaskOutputDoneData)};
    PipelineCPU* const pipeline{data.pipeline};
    FrameCPU* const frame{data.frame};

    VNLogDebug("taskOutputDone timestamp:%" PRIx64, frame->timestamp);

    // Mark as done, and signal pipeline if it is waiting
    {
        common::ScopedLock lock(pipeline->m_interTaskMutex);
        frame->m_state = FrameStateDone;

        // Build the decode info for the frame
        frame->m_decodeInfo.timestamp = frame->timestamp;
        frame->m_decodeInfo.hasBase = true;
        frame->m_decodeInfo.hasEnhancement =
            frame->config.loqEnabled[LOQ1] || frame->config.loqEnabled[LOQ0];
        frame->m_decodeInfo.skipped = frame->m_skip;
        frame->m_decodeInfo.enhanced = frame->config.loqEnabled[LOQ1] || frame->config.loqEnabled[LOQ0];
        frame->m_decodeInfo.baseWidth = frame->baseWidth;
        frame->m_decodeInfo.baseHeight = frame->baseHeight;
        frame->m_decodeInfo.baseBitdepth = frame->baseBitdepth;
        frame->m_decodeInfo.userData = frame->userData;

        pipeline->m_interTaskFrameDone.signal();

        pipeline->m_eventSink->generate(pipeline::EventOutputPictureDone, frame->outputPicture,
                                        &frame->m_decodeInfo);
        pipeline->m_eventSink->generate(pipeline::EventCanReceive);
    }

    return nullptr;
}

void PipelineCPU::addTaskOutputDone(FrameCPU* frame, const LdcTaskDependency* inputs, uint32_t inputsCount)
{
    const TaskOutputDoneData data{this, frame};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, inputsCount, kTaskDependencyInvalid,
                    taskOutputDone, nullptr, 1, 1, sizeof(data), &data, "OutputDone");
}

//// TemporalRelease
//
// Wait for a bunch of input dependencies to be met, then:
//
// - Release temporal buffer to next frame
//
struct TaskTemporalReleaseData
{
    PipelineCPU* pipeline;
    FrameCPU* frame;
    uint32_t planeIndex;
};

void* PipelineCPU::taskTemporalRelease(LdcTask* task, const LdcTaskPart* /*part*/)
{
    VNTraceScoped();
    assert(task->dataSize == sizeof(TaskTemporalReleaseData));

    const TaskTemporalReleaseData& data{VNTaskData(task, TaskTemporalReleaseData)};
    PipelineCPU* const pipeline{data.pipeline};
    FrameCPU* const frame{data.frame};
    const uint32_t planeIndex{data.planeIndex};

    VNLogDebug("taskTemporalRelease timestamp:%" PRIx64, frame->timestamp);

    pipeline->releaseTemporalBuffer(frame, planeIndex);

    return nullptr;
}

void PipelineCPU::addTaskTemporalRelease(FrameCPU* frame, const LdcTaskDependency* deps, uint32_t planeIndex)
{
    const TaskTemporalReleaseData data{this, frame, planeIndex};
    const LdcTaskDependency inputs[] = {deps[planeIndex]};

    ldcTaskGroupAdd(&frame->m_taskGroup, inputs, VNArraySize(inputs), kTaskDependencyInvalid,
                    taskTemporalRelease, nullptr, 1, 1, sizeof(data), &data, "TemporalRelease");
}

// Fill out a task group given a frame configuration
//
void PipelineCPU::generateTasksEnhancement(FrameCPU* frame, uint64_t previousTimestamp)
{
    VNTraceScoped();

    // Convenience values for readability
    const LdeFrameConfig& frameConfig{frame->config};
    const LdeGlobalConfig& globalConfig{*frame->globalConfig};
    const uint8_t numImagePlanes{frame->numImagePlanes()};

    uint32_t enhancementTileIdx = 0;

    if (frame->config.sharpenType != STDisabled && frame->config.sharpenStrength != 0.0f) {
        VNLogWarning("S-Filter is configured in stream, but not supported by decoder.");
    }

    //// LoQ 1
    //
    LdcTaskDependency basePlanes[kLdpPictureMaxNumPlanes] = {};

    // Input planes - will be filled in via sendBase()
    for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
        basePlanes[plane] = frame->m_depBasePicture;
    }

    // Upsample and residuals
    for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
        const bool isEnhanced = frame->isEnhanced(LOQ1, plane);

        //// Input conversion
        //
        LdcTaskDependency basePlane{};

        // Convert between base and enhancement bit depth
        basePlane = addTaskConvertToInternal(frame, plane, globalConfig.baseDepth,
                                             globalConfig.enhancedDepth, basePlanes[plane]);

        //// Base + Residuals
        //
        // First upsample
        LdcTaskDependency baseUpsampled{kTaskDependencyInvalid};
        if (globalConfig.scalingModes[LOQ1] != Scale0D) {
            baseUpsampled = addTaskUpsample(frame, LOQ2, plane, basePlane);
        } else {
            baseUpsampled = basePlane;
        }

        // Enhancement LOQ1 decoding
        if (isEnhanced && frameConfig.loqEnabled[LOQ1]) {
            const uint32_t numPlaneTiles = globalConfig.numTiles[plane][LOQ1];
            if (numPlaneTiles > 1) {
                LdcTaskDependency* tiles =
                    static_cast<LdcTaskDependency*>(alloca(numPlaneTiles * sizeof(LdcTaskDependency)));

                // Generate and apply each tile's command buffer
                for (uint32_t tile = 0; tile < numPlaneTiles; ++tile) {
                    LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                    assert(et->plane == plane && et->loq == LOQ1 && et->tile == tile);

                    LdcTaskDependency commands = addTaskGenerateCmdBuffer(frame, et);
                    tiles[tile] = addTaskApplyCmdBufferDirect(frame, et, baseUpsampled, commands);
                }
                // Wait for all tiles to finish
                basePlanes[plane] = addTaskWaitForMany(frame, tiles, numPlaneTiles);
            } else {
                LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                assert(et->plane == plane && et->loq == LOQ1 && et->tile == 0);

                LdcTaskDependency commands = addTaskGenerateCmdBuffer(frame, et);
                basePlanes[plane] = addTaskApplyCmdBufferDirect(frame, et, baseUpsampled, commands);
            }
        } else {
            basePlanes[plane] = baseUpsampled;
        }
    }

    // Upsample from combined intermediate picture to preliminary output picture
    LdcTaskDependency upsampledPlanes[kLdpPictureMaxNumPlanes] = {};

    for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
        if (globalConfig.scalingModes[LOQ0] != Scale0D) {
            upsampledPlanes[plane] = addTaskUpsample(frame, LOQ1, plane, basePlanes[plane]);
        } else {
            upsampledPlanes[plane] = basePlanes[plane];
        }
    }

    //// LoQ 0
    //
    LdcTaskDependency reconstructedPlanes[kLdpPictureMaxNumPlanes] = {};

    for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
        const bool isEnhanced = frame->isEnhanced(LOQ0, plane);

        LdcTaskDependency recon{upsampledPlanes[plane]};

        if (globalConfig.temporalEnabled && !frame->m_passthrough) {
            LdcTaskDependency temporal{kTaskDependencyInvalid};

            if (plane < globalConfig.numPlanes) {
                // Still need a temporal buffer, even if the particular frame is not enhanced
                // winds up getting passed through and applied
                temporal = requireTemporalBuffer(frame, previousTimestamp, plane);
            }

            if (isEnhanced && frameConfig.loqEnabled[LOQ0]) {
                // Enhancement residuals
                const uint32_t numPlaneTiles = globalConfig.numTiles[plane][LOQ0];
                if (numPlaneTiles > 1) {
                    LdcTaskDependency* tiles = static_cast<LdcTaskDependency*>(
                        alloca(numPlaneTiles * sizeof(LdcTaskDependency)));

                    // Generate and apply each tile's command buffer
                    for (uint32_t tile = 0; tile < numPlaneTiles; ++tile) {
                        LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                        assert(et->plane == plane && et->loq == LOQ0 && et->tile == tile);
                        LdcTaskDependency commands{addTaskGenerateCmdBuffer(frame, et)};

                        tiles[tile] = addTaskApplyCmdBufferTemporal(frame, et, temporal, commands);
                    }
                    // Wait for all tiles to finish
                    temporal = addTaskWaitForMany(frame, tiles, numPlaneTiles);
                } else {
                    LdpEnhancementTile* et = frame->getEnhancementTile(enhancementTileIdx++);
                    assert(et && et->plane == plane && et->loq == LOQ0 && et->tile == 0);

                    LdcTaskDependency commands{addTaskGenerateCmdBuffer(frame, et)};

                    temporal = addTaskApplyCmdBufferTemporal(frame, et, temporal, commands);
                }
            }

            // Always add temporal buffer, even if no enhancement this frame
            if (plane < globalConfig.numPlanes) {
                reconstructedPlanes[plane] = addTaskApplyAddTemporal(frame, plane, temporal, recon);
                addTaskTemporalRelease(frame, reconstructedPlanes, plane);
            } else {
                reconstructedPlanes[plane] = recon;
            }
        } else {
            if (isEnhanced && frameConfig.loqEnabled[LOQ0]) {
                // Enhancement residuals
                const uint32_t numPlaneTiles = globalConfig.numTiles[plane][LOQ0];
                if (numPlaneTiles > 1) {
                    LdcTaskDependency* tiles = static_cast<LdcTaskDependency*>(
                        alloca(numPlaneTiles * sizeof(LdcTaskDependency)));

                    // Generate and apply each tile's command buffer
                    for (uint32_t tile = 0; tile < numPlaneTiles; ++tile) {
                        LdpEnhancementTile* et{frame->getEnhancementTile(enhancementTileIdx++)};
                        assert(et->plane == plane && et->loq == LOQ0 && et->tile == tile);
                        LdcTaskDependency commands{addTaskGenerateCmdBuffer(frame, et)};
                        tiles[tile] = addTaskApplyCmdBufferDirect(frame, et, recon, commands);
                    }
                    // Wait for all tiles to finish
                    recon = addTaskWaitForMany(frame, tiles, numPlaneTiles);
                } else {
                    LdpEnhancementTile* et = frame->getEnhancementTile(enhancementTileIdx++);
                    assert(et->plane == plane && et->loq == LOQ0 && et->tile == 0);

                    LdcTaskDependency commands{addTaskGenerateCmdBuffer(frame, et)};

                    recon = addTaskApplyCmdBufferDirect(frame, et, recon, commands);
                }
            }

            reconstructedPlanes[plane] = recon;
        }
    }

    assert(enhancementTileIdx == frame->enhancementTileCount);

    LdcTaskDependency outputPlanes[kLdpPictureMaxNumPlanes] = {};

    // Convert any enhanced planes back to output
    for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
        outputPlanes[plane] =
            addTaskConvertFromInternal(frame, plane, globalConfig.baseDepth, globalConfig.enhancedDepth,
                                       frame->m_depOutputPicture, reconstructedPlanes[plane]);
    }

    // Send output when all planes are ready
    addTaskOutputDone(frame, outputPlanes, numImagePlanes);

    // Send base when all tasks that use it have completed
    LdcTaskDependency deps[kLdpPictureMaxNumPlanes] = {};
    uint32_t depsCount = 0;
    ldcTaskGroupFindOutputSetFromInput(&frame->m_taskGroup, frame->m_depBasePicture, deps,
                                       kLdpPictureMaxNumPlanes, &depsCount);
    addTaskBaseDone(frame, deps, depsCount);
}

// Fill out a task group for a simple unscaled passthrough configuration
//
void PipelineCPU::generateTasksPassthrough(FrameCPU* frame)
{
    VNTraceScoped();

    uint8_t numImagePlanes{kLdpPictureMaxNumPlanes};
    if (frame->basePicture) {
        VNLogDebugF("No base for passthrough: %" PRIx64, frame->timestamp);
        numImagePlanes = ldpPictureLayoutPlanes(&frame->basePicture->layout);
    }

    LdcTaskDependency outputPlanes[kLdpPictureMaxNumPlanes] = {};

    for (uint8_t plane = 0; plane < numImagePlanes; ++plane) {
        outputPlanes[plane] =
            addTaskPassthrough(frame, plane, frame->m_depOutputPicture, frame->m_depBasePicture);
    }

    // Send output and base when all planes are ready
    addTaskOutputDone(frame, outputPlanes, numImagePlanes);
    addTaskBaseDone(frame, outputPlanes, numImagePlanes);
}

#ifdef VN_SDK_LOG_ENABLE_DEBUG
// Dump frame and index state
//
void PipelineCPU::logFrames() const
{
    char buffer[512];

    VNLogDebugF("Frames: %d", m_frames.size());
    for (uint32_t i = 0; i < m_frames.size(); ++i) {
        FrameCPU* const frame{VNAllocationPtr(m_frames[i], FrameCPU)};
        frame->longDescription(buffer, sizeof(buffer));
        VNLogDebugF("  %4d: %s", i, buffer);
    }

    VNLogDebugF("Reorder: %d", m_reorderIndex.size());
    for (uint32_t i = 0; i < m_reorderIndex.size(); ++i) {
        FrameCPU* const frame{m_reorderIndex[i]};
        const LdcMemoryAllocation* const ptr =
            m_frames.findUnordered(ldcVectorCompareAllocationPtr, frame);
        const int32_t idx = static_cast<int32_t>(ptr ? (ptr - &m_frames[0]) : -1);
        VNLogDebugF("  %2d: %4d", i, idx);
    }

    VNLogDebugF("Processing: %d", m_processingIndex.size());
    for (uint32_t i = 0; i < m_processingIndex.size(); ++i) {
        FrameCPU* const frame{m_processingIndex[i]};
        const LdcMemoryAllocation* const ptr =
            m_frames.findUnordered(ldcVectorCompareAllocationPtr, frame);
        const int32_t idx = static_cast<int32_t>(ptr ? (ptr - &m_frames[0]) : -1);
        VNLogDebugF("  %2d: %4d", i, idx);
    }

    VNLogDebugF("Bases In: %d (%d)", m_basePicturePending.size(), m_basePicturePending.reserved());
    VNLogDebugF("Bases Out: %d (%d)", m_basePictureOutBuffer.size(), m_basePictureOutBuffer.capacity());
    VNLogDebugF("Output: %d (%d)", m_outputPictureAvailableBuffer.size(),
                m_outputPictureAvailableBuffer.capacity());
}
#endif

} // namespace lcevc_dec::pipeline_cpu
