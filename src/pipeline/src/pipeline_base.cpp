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

#include <LCEVC/common/return_code.h>
#include <LCEVC/pipeline/buffer_base.h>
#include <LCEVC/pipeline/pipeline_base.h>
#include <LCEVC/pipeline/types.h>
//
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
#include <LCEVC/pipeline/frame_base.h>
#include <LCEVC/pipeline/picture_base.h>

namespace lcevc_dec::pipeline {

// Utility functions for finding and sorting things in Vectors
//
namespace {
    // Compare two frames in an array of frame pointers
    inline int sortFramePtrTimestamp(const void* lhs, const void* rhs)
    {
        const auto* frameLhs{*static_cast<const FrameBase* const *>(lhs)};
        const auto* frameRhs{*static_cast<const FrameBase* const *>(rhs)};

        return pipeline::compareTimestamps(frameLhs->timestamp, frameRhs->timestamp);
    }

    // Check timestamp of an allocated BasePicture
    inline int findBasePictureTimestamp(const void* element, const void* ptr)
    {
        const auto* alloc{static_cast<const LdcMemoryAllocation*>(element)};
        assert(VNIsAllocated(*alloc));
        const uint64_t ets{VNAllocationPtr(*alloc, BasePicture)->timestamp};
        const uint64_t ts{*static_cast<const uint64_t*>(ptr)};

        return pipeline::compareTimestamps(ets, ts);
    }

    inline int compareFramePtr(const void* element, const void* other)
    {
        const auto* frameLhs{*static_cast<const FrameBase* const *>(element)};
        const auto* frameRhs{static_cast<const FrameBase*>(other)};

        if (frameLhs < frameRhs) {
            return -1;
        }
        if (frameLhs > frameRhs) {
            return 1;
        }
        return 0;
    }

} // namespace

// PipelineBase
//
PipelineBase::PipelineBase(const PipelineBuilderBase& builder, pipeline::EventSink* eventSink)
    : m_eventSink(eventSink ? eventSink : pipeline::EventSink::nullSink())
    , m_allocator(builder.allocator())
    , m_allocatedPictures(builder.configuration().maxLatency * 2, builder.allocator())
    , m_allocatedFrames(builder.configuration().maxLatency * 2, builder.allocator())
    , m_reorderIndex(builder.configuration().maxLatency, builder.allocator())
    , m_processingIndex(builder.configuration().maxLatency, builder.allocator())
    , m_doneIndex(builder.configuration().maxLatency, builder.allocator())
    , m_flushIndex(builder.configuration().maxLatency, builder.allocator())
    , m_maxReorder(builder.configuration().defaultMaxReorder)
    , m_basePicturePending(nextPowerOfTwoU32(builder.configuration().maxLatency + 1), builder.allocator())
    , m_basePictureOutBuffer(nextPowerOfTwoU32(builder.configuration().maxLatency + 1), builder.allocator())
    , m_outputPictureAvailableBuffer(nextPowerOfTwoU32(builder.configuration().maxLatency + 1),
                                     builder.allocator())
{
    // Configuration pool
    LdeBitstreamVersion bitstreamVersion = BitstreamVersionUnspecified;
    if (builder.configuration().forceBitstreamVersion >= BitstreamVersionInitial &&
        builder.configuration().forceBitstreamVersion <= BitstreamVersionCurrent) {
        bitstreamVersion = static_cast<LdeBitstreamVersion>(builder.configuration().forceBitstreamVersion);
    }
    ldeConfigPoolInitialize(m_allocator, m_allocator, &m_configPool, bitstreamVersion);

    ldcTaskPoolInitialize(&m_taskPool, m_allocator, m_allocator, builder.configuration().numThreads,
                          builder.configuration().numReservedTasks);

    m_eventSink->generate(pipeline::EventCanSendEnhancement);
    m_eventSink->generate(pipeline::EventCanSendBase);
    m_eventSink->generate(pipeline::EventCanSendPicture);
}

PipelineBase::~PipelineBase()
{
    // Release config
    ldeConfigPoolRelease(&m_configPool);

    // Close down task pool
    ldcTaskPoolDestroy(&m_taskPool);

    m_eventSink->generate(pipeline::EventExit);
}

// Send/receive
//
LdcReturnCode PipelineBase::sendDecoderEnhancementData(uint64_t timestamp, const uint8_t* data,
                                                       uint32_t byteSize)
{
    VNLogDebug("sendDecoderEnhancementData: ts:%" PRIx64 " %d", timestamp, byteSize);

    // Invalid if this timestamp is already present in decoder.
    //
    // NB: API clients are expected to make distinct timestamps over discontinuities using utility library
    if (findFrame(timestamp, true) != nullptr) {
        VNLogDebug("sendDecoderEnhancementData: ts:%" PRIx64 " Duplicate Frame", timestamp);
        return LdcReturnCodeInvalidParam;
    }

    // Drop enhancement with a pts less than or equal to current 'processing' limit.
    if (m_processingLimit != kInvalidTimestamp &&
        pipeline::compareTimestamps(timestamp, m_processingLimit) <= 0) {
        VNLogWarning("sendDecoderEnhancementData: dropping ts:%" PRIx64
                     " <= processing limit ts:%" PRIx64,
                     timestamp, m_processingLimit.load());
        return LdcReturnCodeError;
    }

    if (frameLatency() >= configuration().maxLatency) {
        VNLogDebug("sendDecoderEnhancementData: ts:%" PRIx64 " AGAIN", timestamp);
        return LdcReturnCodeAgain;
    }

    // New pending frame
    FrameBase* const frame{allocateFrame(timestamp)};
    if (!frame) {
        return LdcReturnCodeError;
    }

    // Keep record of highest sent frame timestamp
    if (m_sendLimit == kInvalidTimestamp || pipeline::compareTimestamps(timestamp, m_sendLimit)) {
        m_sendLimit = timestamp;
    }

    // Attach enhancement data to frame
    frame->setEnhancementData(data, byteSize);

    // Frame ready to be reordered into presentation order for correct LCEVC decode
    frame->setState(FrameStateReorder);

    // Add frame to reorder table sorted by timestamp
    m_reorderIndex.insert(sortFramePtrTimestamp, frame);

    // Attach any pending base for matching timestamp
    if (BasePicture* bp = m_basePicturePending.findUnordered(findBasePictureTimestamp, &frame->timestamp);
        bp) {
        frame->setBasePicture(bp->picture, bp->deadline, bp->userData);
        m_basePicturePending.remove(bp);
        m_eventSink->generate(pipeline::EventCanSendBase);
    }

    startReadyFrames();
    return LdcReturnCodeSuccess;
}

LdcReturnCode PipelineBase::sendDecoderBase(uint64_t timestamp, LdpPicture* basePicture,
                                            uint32_t timeoutUs, void* userData)
{
    VNLogDebug("sendDecoderBase: ts:%" PRIx64 " %p", timestamp, (void*)basePicture);

    // Drop a base that has a pts less than current 'processing' limit
    if (m_processingLimit != kInvalidTimestamp &&
        pipeline::compareTimestamps(timestamp, m_processingLimit) < 0) {
        VNLogWarning("sendDecoderBase: dropping ts:%" PRIx64 " < processing limit ts:%" PRIx64,
                     timestamp, m_processingLimit.load());
        return LdcReturnCodeError;
    }

    // Find the frame associated with PTS
    FrameBase* frame{findFrame(timestamp, true)};
    if (frame) {
        // Enhancement exists
        if (LdcReturnCode ret = frame->setBasePicture(
                basePicture, threadTimeMicroseconds(static_cast<int32_t>(timeoutUs)), userData);
            ret != LdcReturnCodeSuccess) {
            return ret;
        }

        // Force pass-through if requested
        if (configuration().passthroughMode == PassthroughMode::Force) {
            frame->setPassthrough();
        }
        // Kick off any frames that are at or before the base timestamp
        process(timestamp);
        m_eventSink->generate(pipeline::EventCanSendBase);
        return LdcReturnCodeSuccess;
    }

    BasePicture bp = {timestamp, basePicture,
                      threadTimeMicroseconds(static_cast<int32_t>(timeoutUs)), userData};

    if (m_basePicturePending.size() < configuration().enhancementDelay) {
        // There is capacity to buffer base picture
        m_basePicturePending.append(bp);
        return LdcReturnCodeSuccess;
    }

    // Cannot buffer any more pending bases
    if (configuration().passthroughMode == PassthroughMode::Disable) {
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

    // Keep record of highest sent frame timestamp
    if (m_sendLimit == kInvalidTimestamp || pipeline::compareTimestamps(timestamp, m_sendLimit)) {
        m_sendLimit = timestamp;
    }

    // New pass-through frame - no enhancement
    FrameBase* const passFrame{allocateFrame(timestamp)};

    if (!passFrame) {
        return LdcReturnCodeError;
    }

    // Add frame to reorder table sorted by timestamp
    passFrame->setBasePicture(basePicture, threadTimeMicroseconds(static_cast<int32_t>(timeoutUs)), userData);
    passFrame->setPassthrough();
    passFrame->setState(FrameStateReorder);
    m_reorderIndex.insert(sortFramePtrTimestamp, passFrame);

    process(timestamp);
    return LdcReturnCodeSuccess;
}

LdcReturnCode PipelineBase::sendDecoderPicture(LdpPicture* outputPicture)
{
    VNLogDebug("sendDecoderPicture: %p", (void*)outputPicture);

    // Add to available queue
    if (m_outputPictureAvailableBuffer.size() > configuration().maxLatency ||
        !m_outputPictureAvailableBuffer.tryPush(outputPicture)) {
        VNLogDebug("sendDecoderPicture: AGAIN");
        return LdcReturnCodeAgain;
    }

    connectOutputPictures();

    startReadyFrames();
    return LdcReturnCodeSuccess;
}

LdcReturnCode PipelineBase::receiveDecoderPicture(LdpDecodeInformation& decodeInfoOut, LdpPicture*& pictureOut)
{
    pictureOut = nullptr;

    FrameBase* frame{};

    releaseFlushedFrames();

    // Pull any done frame from start (lowest timestamp) of 'processing' frame index.
    {
        common::ScopedLock lock(m_interTaskMutex);

        if (!m_doneIndex.isEmpty()) {
            // Something in 'done' index
            frame = m_doneIndex[0];
            m_doneIndex.removeIndex(0);
        } else if (m_processingIndex.size() > configuration().minLatency &&
                   m_processingIndex[0]->canComplete() && !isFlushed(m_processingIndex[0])) {
            const FrameBase* pendingFrame = m_processingIndex[0];

            // Earliest frame will complete, so hang around and wait for it to move to done index
            VNLogDebug("waiting for ts:%" PRIx64, pendingFrame->timestamp);

            if (!m_interTaskFrameDone.waitDeadline(lock, pendingFrame->deadline)) {
                VNLogWarning("wait timed out ts:%" PRIx64, pendingFrame->timestamp);
#if VN_SDK_LOG(DEBUG)
                ldcTaskPoolDump(&m_taskPool, nullptr);
#endif
            } else {
                VNLogDebug("wait done ts:%" PRIx64, pendingFrame->timestamp);
            }

            if (!m_doneIndex.isEmpty()) {
                frame = m_doneIndex[0];
                m_doneIndex.removeIndex(0);
            } else {
                VNLogDebug("no picture ts:%" PRIx64, m_processingIndex[0]->timestamp);
            }
        }
    }

    if (!frame) {
        return LdcReturnCodeAgain;
    }

    // Copy surviving data from frame
    decodeInfoOut = frame->decodeInformation;
    pictureOut = frame->outputPicture;
    const LdcReturnCode decodeResult = frame->decodeResult();

    VNLogDebug("receiveDecoderPicture: ts:%" PRIx64 " %p hb:%d he:%d sk:%d enh:%d",
               decodeInfoOut.timestamp, (void*)pictureOut, decodeInfoOut.hasBase,
               decodeInfoOut.hasEnhancement, decodeInfoOut.skipped, decodeInfoOut.enhanced);

    // Once an output picture has left the building - we can drop the associated frame
    freeFrame(frame);

    return decodeResult;
}

LdcReturnCode PipelineBase::receiveDecoderBase(LdpPicture*& pictureOut)
{
    pictureOut = nullptr;

    // Is there anything in finished base FIFO?
    if (!m_basePictureOutBuffer.tryPop(pictureOut)) {
        return LdcReturnCodeAgain;
    }

    VNLogDebug("receiveDecoderBase: %" PRIx64 " %p", (void*)pictureOut);

    return LdcReturnCodeSuccess;
}

void PipelineBase::getCapacity(LdpPipelineCapacity* capacity)
{
    assert(capacity);
    capacity->enhancementAvailable = configuration().maxLatency - frameLatency();
    capacity->baseAvailable = configuration().maxLatency - frameLatency();
    capacity->outputAvailable =
        m_outputPictureAvailableBuffer.capacity() - m_outputPictureAvailableBuffer.size();

    capacity->enhancementMaximum = configuration().maxLatency;
    capacity->baseMaximum = configuration().maxLatency;
    capacity->outputAvailable = m_outputPictureAvailableBuffer.capacity();
}

// Get the resolution for a timestamp
LdcReturnCode PipelineBase::peekDecoder(uint64_t timestamp, uint32_t& widthOut, uint32_t& heightOut)
{
    // Flush everything up to given timestamp
    process(timestamp);

    // Find the frame associated with PTS
    const FrameBase* frame{findFrame(timestamp, false)};
    if (!frame) {
        return LdcReturnCodeNotFound;
    }
    if (!frame->globalConfig) {
        if (configuration().passthroughMode == PassthroughMode::Disable) {
            return LdcReturnCodeNotFound;
        }
        return LdcReturnCodeAgain;
    }

    if (frame->isPassthrough()) {
        widthOut = frame->baseWidth;
        heightOut = frame->baseHeight;
    } else {
        widthOut = frame->globalConfig->width;
        heightOut = frame->globalConfig->height;
    }
    return LdcReturnCodeSuccess;
}

// Get a picture description for a timestamp
LdcReturnCode PipelineBase::peekDecoderPictureDesc(uint64_t timestamp, LdpPictureDesc* desc)
{
    // Flush everything up to given timestamp
    process(timestamp);

    // Find the frame associated with PTS
    const FrameBase* frame{findFrame(timestamp, false)};
    if (!frame) {
        return LdcReturnCodeNotFound;
    }
    if (!frame->globalConfig) {
        if (configuration().passthroughMode == PassthroughMode::Disable) {
            return LdcReturnCodeNotFound;
        }
        return LdcReturnCodeAgain;
    }

    if (!frame->baseDataValid()) {
        return LdcReturnCodeAgain;
    }

    // There is a frame - get the description
    //
    (*desc) = frame->getOutputPictureDesc(configuration().passthroughMode);

    // Figure correct return code given frame state
    if (isFlushed(frame)) {
        return LdcReturnCodeFlushed;
    }

    if (frame->deadline < threadTimeMicroseconds(0)) {
        return LdcReturnCodeTimeout;
    }

    return LdcReturnCodeSuccess;
}

// Move any reorder frames at or before timestamp into processing state
void PipelineBase::process(uint64_t timestamp)
{
    assert(timestamp != kInvalidTimestamp);

    // Move 'processing' point forwards
    if (m_processingLimit != kInvalidTimestamp &&
        pipeline::compareTimestamps(timestamp, m_processingLimit) < 0) {
        VNLogError("Processing timestamp went backwards.");
        return;
    }
    m_processingLimit = timestamp;

    startReadyFrames();
}

LdcReturnCode PipelineBase::skip(uint64_t timestamp)
{
    const uint64_t fromTimestamp = m_skipLimit;

    VNLogDebug("skip: ts:%" PRIx64 " %p", timestamp);

    // Using kInvalidTimestamp skips all sent frames
    if (timestamp == kInvalidTimestamp) {
        timestamp = m_sendLimit;
    }

    // Skipping beyond highest sent timestamp does nothing
    if (pipeline::compareTimestamps(timestamp, m_sendLimit) > 0) {
        return LdcReturnCodeSuccess;
    }

    // Move 'skip' point forwards
    if (m_skipLimit != kInvalidTimestamp && pipeline::compareTimestamps(timestamp, m_skipLimit) < 0) {
        VNLogError("Skip timestamp went backwards.");
        return LdcReturnCodeError;
    }
    m_skipLimit = timestamp;

    // Bump 'processing' point if necessary
    if (m_processingLimit == kInvalidTimestamp ||
        pipeline::compareTimestamps(timestamp, m_processingLimit) > 0) {
        m_processingLimit = timestamp;
    }

    startReadyFrames();
    unblockSkippedFrames(fromTimestamp);
    return LdcReturnCodeSuccess;
}

// Flush any frames  at or before timestamp into processing state
LdcReturnCode PipelineBase::flush(uint64_t timestamp)
{
    const uint64_t fromTimestamp = m_flushLimit;

    VNLogDebug("flush: ts:%" PRIx64 " %p", timestamp);

    // Using kInvalidTimestamp flushed all sent frames
    if (timestamp == kInvalidTimestamp) {
        timestamp = m_sendLimit;
    }

    // Move 'flush' point forwards
    if (m_flushLimit != kInvalidTimestamp && pipeline::compareTimestamps(timestamp, m_flushLimit) < 0) {
        VNLogError("Flush timestamp went backwards.");
        return LdcReturnCodeError;
    }
    m_flushLimit = timestamp;

    // Bump 'processing' point if necessary
    if (m_processingLimit == kInvalidTimestamp ||
        pipeline::compareTimestamps(timestamp, m_processingLimit) > 0) {
        m_processingLimit = timestamp;
    }

    // Bump 'skip' point if necessary
    if (m_skipLimit == kInvalidTimestamp || pipeline::compareTimestamps(timestamp, m_skipLimit) > 0) {
        m_skipLimit = timestamp;
    }

    startReadyFrames();
    unblockFlushedFrames(fromTimestamp);
    return LdcReturnCodeSuccess;
}

// Make sure any skipped frames are ready to run
void PipelineBase::unblockSkippedFrames(uint64_t fromTimestamp)
{
    for (uint32_t i = 0; i < m_allocatedFrames.size(); ++i) {
        FrameBase* const frame{m_allocatedFrames[i]};
        if (!frame->isStateProcessing()) {
            continue;
        }
        // Already skipped?
        if (fromTimestamp != kInvalidTimestamp &&
            pipeline::compareTimestamps(fromTimestamp, frame->timestamp) >= 0) {
            continue;
        }

        if (isSkipped(frame)) {
            frame->unblockForSkip();
        }
    }
}

// Make sure any flushed frames are ready to run
void PipelineBase::unblockFlushedFrames(uint64_t fromTimestamp)
{
    for (uint32_t i = 0; i < m_allocatedFrames.size(); ++i) {
        FrameBase* const frame{m_allocatedFrames[i]};
        if (!frame->isStateProcessing()) {
            continue;
        }

        // Already flushed?
        if (fromTimestamp != kInvalidTimestamp &&
            pipeline::compareTimestamps(fromTimestamp, frame->timestamp) >= 0) {
            continue;
        }

        if (isFlushed(frame)) {
            frame->unblockForFlush();
        }
    }
}

// Wait for all work to be finished - optionally stopping anything in progress
LdcReturnCode PipelineBase::synchronizeDecoder(uint64_t timestamp, bool flushPending)
{
    VNLogDebug("synchronizeDecoder: %d", flushPending);

    if (flushPending) {
        // Mark current frames as flushed
        flush(timestamp);

        while (m_processingIndex.size() > 0) {
            FrameBase* const frame = m_processingIndex[0];
            if (!frame->canComplete()) {
                VNLogError("Flushed frame cannot complete: ts:%" PRIx64, frame->timestamp);
            }
            frame->waitForTasks();
        }
    } else {
        // For frames that are not blocked on input - wait in timestamp order
        while (m_processingIndex.size() > 0) {
            FrameBase* const frame = m_processingIndex[0];
            if (!frame->canComplete()) {
                break;
            }
            frame->waitForTasks();
        }
    }

    releaseFlushedFrames();
    return LdcReturnCodeSuccess;
}

bool PipelineBase::isProcessing(const FrameBase* frame) const
{
    assert(frame);

    if (m_processingLimit == kInvalidTimestamp) {
        return false;
    }
    return frame->timestamp <= m_processingLimit;
}

bool PipelineBase::isSkipped(const FrameBase* frame) const
{
    assert(frame);
    if (m_skipLimit == kInvalidTimestamp) {
        return false;
    }
    return frame->timestamp <= m_skipLimit;
}

bool PipelineBase::isPassthrough(const FrameBase* frame) const
{
    assert(frame);
    if (m_skipLimit == kInvalidTimestamp) {
        return false;
    }
    return frame->isPassthrough();
}

bool PipelineBase::isFlushed(const FrameBase* frame) const
{
    assert(frame);
    if (m_flushLimit == kInvalidTimestamp) {
        return false;
    }
    return frame->timestamp <= m_flushLimit;
}

// Pictures
//
uint32_t PipelineBase::findAllocatedPicture(const PictureBase* frame) const
{
    for (uint32_t i = 0; i < m_allocatedPictures.size(); ++i) {
        if (m_allocatedPictures[i] == frame) {
            return i;
        }
    }

    VNLogError("Could not find picture!");
    return UINT32_MAX;
}

LdcReturnCode PipelineBase::allocPicture(const LdpPictureDesc& desc, LdpPicture*& pictureOut)
{
    pictureOut = nullptr;
    PictureBase* picture{allocatePicture()};
    if (!picture) {
        return LdcReturnCodeError;
    }
    picture->setDesc(desc);
    pictureOut = picture;
    return LdcReturnCodeSuccess;
}

LdcReturnCode PipelineBase::allocPictureExternal(const LdpPictureDesc& desc,
                                                 const LdpPicturePlaneDesc* planeDescArr,
                                                 const LdpPictureBufferDesc* buffer, LdpPicture*& pictureOut)
{
    pictureOut = nullptr;
    PictureBase* picture{allocatePicture()};
    if (!picture) {
        return LdcReturnCodeError;
    }
    picture->setExternalBufferDesc(buffer);
    picture->setDesc(desc);
    picture->setExternalPlaneDesc(planeDescArr);

    if (buffer && buffer->accelBuffer) {
        // Give pipeline opportunity to deal with implementation specific accelBuffer
        const LdcReturnCode rc = storeAccelBuffer(picture, desc, buffer->accelBuffer);
        if (rc != LdcReturnCodeSuccess) {
            // Pipeline can't handle this
            VNLogVerbose("Pipeline does not support accel buffer (rc=%d)", static_cast<int>(rc));
            releasePicture(picture);
            return rc;
        }
    }

    pictureOut = picture;
    return LdcReturnCodeSuccess;
}

LdcReturnCode PipelineBase::storeAccelBuffer(PictureBase* /*picture*/, const LdpPictureDesc& /*desc*/,
                                             LdpAccelBuffer* /*accelBuffer*/)
{
    // Default action - the pipeline does not support accel buffers.
    return LdcReturnCodeNotSupported;
}

void PipelineBase::freePicture(LdpPicture* ldpPicture)
{
    // Get back to derived Picture class
    PictureBase* picture{static_cast<PictureBase*>(ldpPicture)};
    assert(ldpPicture);

    releasePicture(picture);
}

// Frames
//

// Find existing Frame for a timestamp, or return nullptr if it does not exist.
//
FrameBase* PipelineBase::findFrame(uint64_t timestamp, bool ignoreSkipped)
{
    for (uint32_t i = 0; i < m_allocatedFrames.size(); ++i) {
        FrameBase* const frame{m_allocatedFrames[i]};
        if (ignoreSkipped && isSkipped(frame)) {
            continue;
        }

        if (pipeline::compareTimestamps(frame->timestamp, timestamp) == 0) {
            return frame;
        }
    }

    return nullptr;
}

uint32_t PipelineBase::findAllocatedFrame(const FrameBase* frame) const
{
    for (uint32_t i = 0; i < m_allocatedFrames.size(); ++i) {
        if (m_allocatedFrames[i] == frame) {
            return i;
        }
    }

    VNLogError("Could not find frame!");
    return UINT32_MAX;
}

// Number of outstanding frames
uint32_t PipelineBase::frameLatency() const
{
    return m_reorderIndex.size() + m_processingIndex.size();
}

//// Frame start
//
// Get the next frame, if any, in timestamp order - taking into account reorder and flushing.
//
FrameBase* PipelineBase::getNextReordered()
{
    // Are there any frames at all?
    if (m_reorderIndex.isEmpty()) {
        return nullptr;
    }

    // If exceeded reorder limit, or flushing
    FrameBase** const firstFrame = m_reorderIndex.at(0);
    if (!firstFrame || !*firstFrame) {
        return nullptr;
    }

    if (m_reorderIndex.size() >= m_maxReorder || isProcessing(*firstFrame)) {
        FrameBase* const frame{*firstFrame};
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
void PipelineBase::startReadyFrames()
{
    releaseFlushedFrames();

    // Pull ready frames from reorder table
    while (FrameBase* frame = getNextReordered()) {
        const uint64_t timestamp{frame->timestamp};
        bool goodConfig = false;

        if (m_previousTimestamp != kInvalidTimestamp &&
            pipeline::compareTimestamps(m_previousTimestamp, timestamp) > 0) {
            // Frame has been flushed out of reorder queue too late - mark as passthrough
            VNLogDebug("startReadyFrames: out of order: ts:%" PRIx64 " prev: %" PRIx64);
            frame->setPassthrough();
        }

        // Try and parse frame configuration
        if (!frame->isPassthrough()) {
            goodConfig = frame->parseEnhancementData(&m_configPool);
            if (!goodConfig) {
                frame->setPassthrough();
            }
        }

        if (frame->isPassthrough()) {
            // Set up enough frame configuration to support pass-through
            ldeConfigPoolFramePassthrough(&m_configPool, &frame->globalConfig, &frame->config);
        }

        VNLogDebug(
            "Start Frame: ts:%" PRIx64 " goodConfig:%d temporalEnabled:%d, temporalPresent:%d "
            "temporalRefresh:%d loqEnabled[0]:%d loqEnabled[1]:%d skip:%d flush:%d passthrough:%d",
            timestamp, goodConfig, frame->globalConfig->temporalEnabled,
            frame->config.temporalSignallingPresent, frame->config.temporalRefresh,
            frame->config.loqEnabled[0], frame->config.loqEnabled[1], isSkipped(frame),
            isFlushed(frame), frame->isPassthrough());

        // Once we have per frame configuration, we can properly initialize and figure out tasks for the frame
        if (!frame->initialize(configuration(), &m_taskPool, ditherGlobal())) {
            VNLogError("Could not allocate frame buffers: ts:%" PRIx64, frame->timestamp);
            // Could not allocate buffers - switch to pass-through
            frame->setPassthrough();
        }

        // Unblock frames if they are skipped or flushed
        if (isSkipped(frame)) {
            frame->unblockForSkip();
        }

        if (isFlushed(frame)) {
            frame->unblockForFlush();
        }

        if (isFlushed(frame) && frame->basePicture == nullptr && frame->outputPicture == nullptr) {
            // Frame can just be released now, otherwise let it go through normal processing
            // to allow pictures to be returned.
            VNLogDebug("Freeing flushed frame: ts:%" PRIx64, frame->timestamp);
            freeFrame(frame);
        } else {
            // Decode the frame as normal, add it to the processing index with the previous
            // frame's temporal buffer timestamp (if required for temporal=on)
            {
                common::ScopedLock lock(m_interTaskMutex);
                frame->setState(FrameStateProcessing);
                m_processingIndex.append(frame);
            }

            frame->generateTasks(this, m_lastGoodTimestamp);

            // Remember timestamps for the next frame
            m_previousTimestamp = timestamp;
            if (goodConfig) {
                m_lastGoodTimestamp = timestamp;
            }
        }
    }

    // Connect available output pictures to started pictures
    connectOutputPictures();
}

// Connect any available output pictures to frames that can use them
//
void PipelineBase::connectOutputPictures()
{
    // While there are available output pictures and pending frames,
    // go through frames in timestamp order, assigning next output picture
    while (true) {
        FrameBase* frame{};

        if (m_outputPictureAvailableBuffer.isEmpty()) {
            // No output pictures left
            break;
        }

        // Find next in process frame with base data, and without an assigned output picture
        {
            common::ScopedLock lock(m_interTaskMutex);

            for (uint32_t idx = 0; idx < m_processingIndex.size(); ++idx) {
                FrameBase** const framePtr = m_processingIndex.at(idx);
                if (framePtr && *framePtr && !(*framePtr)->outputPicture && (*framePtr)->baseDataValid()) {
                    frame = *framePtr;
                    break;
                }
            }
        }

        if (!frame) {
            // No frames without output pictures left
            break;
        }

        // get the output layout
        const LdpPictureDesc desc{frame->getOutputPictureDesc(configuration().passthroughMode)};

        //  Get the picture
        LdpPicture* ldpPicture{};
        m_outputPictureAvailableBuffer.pop(ldpPicture);
        if (!ldpPicture) {
            return;
        }
        auto* bp = static_cast<PictureBase*>(ldpPicture);

        // Set output picture layout and bind buffer
        bp->setDescAndBind(desc, BufferUsageOut);

        if (frame->globalConfig->cropEnabled) {
            ldpPicture->margins.left = frame->globalConfig->crop.left;
            ldpPicture->margins.right = frame->globalConfig->crop.right;
            ldpPicture->margins.top = frame->globalConfig->crop.top;
            ldpPicture->margins.bottom = frame->globalConfig->crop.bottom;
        }

        VNLogDebug("connectOutputPicture: ts:%" PRIx64 " %p %ux%u (r:%d p:%d o:%d)", frame->timestamp,
                   (void*)ldpPicture, desc.width, desc.height, m_reorderIndex.size(),
                   m_processingIndex.size(), m_outputPictureAvailableBuffer.size());
        // Poke it into the frame's task group
        frame->setOutputPicture(ldpPicture);

        // Tell API there is output picture space
        m_eventSink->generate(pipeline::EventCanSendPicture);
    }
}

// Clear 'flush' index
//
// This is done on main pipeline thread (and not as part of taskDone) allowing task groups tasks
// to finish cleanly.
//
void PipelineBase::releaseFlushedFrames()
{
    while (!m_flushIndex.isEmpty()) {
        FrameBase* frame{nullptr};
        {
            common::ScopedLock lock(m_interTaskMutex);
            FrameBase** const framePtr = m_flushIndex.at(0);
            if (!framePtr || !*framePtr) {
                return;
            }
            frame = *framePtr;
            m_flushIndex.removeIndex(0);
        }
        VNLogDebug("Released flushed frame: ts:%" PRIx64, frame->timestamp);
        freeFrame(frame);
    }
}

//// Temporal
//
// Look through all temporal buffers, looking for one that matches the given frame and plane's requirements
//
// The frame<->temporal buffer search loops are where individual frame tasks can interact with
// each other, so are protected by protected by m_interTaskMutex.
//
TemporalBufferBase* PipelineBase::findTemporalBuffer(FrameBase* frame, uint32_t plane)
{
    TemporalBufferBase* foundTemporalBuffer{};

    {
        common::ScopedLock lock(m_interTaskMutex);
        for (uint32_t i = 0;; ++i) {
            TemporalBufferBase* tb{getTemporalBuffer(i)};
            if (!tb) {
                // No more buffers
                break;
            }
            if (tb->frame) {
                // In use
                continue;
            }

            if (frame->tryAttachTemporalBuffer(plane, tb)) {
                foundTemporalBuffer = tb;
                break;
            }
        }
    }

    if (!foundTemporalBuffer) {
        // Not found - will get resolved later by being transferred from a previous frame
        return nullptr;
    }

    VNLogDebug("  findTemporalBuffer found: plane:%" PRIu32 " ts:%" PRIx64 " found_ts:%" PRIx64,
               plane, frame->timestamp, foundTemporalBuffer->desc.timestamp);

    //
    frame->updateTemporalBuffer(plane);

    return foundTemporalBuffer;
}

// Mark the frame as having finished with it's temporal buffer, and try to transfer buffer on to another frame
//
void PipelineBase::transferTemporalBuffer(FrameBase* frame, uint32_t plane)
{
    VNLogDebug("releaseTemporalBuffer: ts:%" PRIx64 " plane: %" PRIu32, frame->timestamp, plane);

    FrameBase* foundNextFrame{nullptr};
    TemporalBufferBase* tb{nullptr};

    {
        common::ScopedLock lock(m_interTaskMutex);

        tb = frame->detachTemporalBuffer(plane);
        if (tb == nullptr) {
            // No temporal buffer to be released
            return;
        }

        // Do any of the pending frames want this buffer?
        for (uint32_t idx = 0; idx < m_processingIndex.size(); ++idx) {
            FrameBase** const nextFramePtr = m_processingIndex.at(idx);
            if (!nextFramePtr || !*nextFramePtr) {
                continue;
            }
            FrameBase* nextFrame{*nextFramePtr};
            if (nextFrame->tryAttachTemporalBuffer(plane, tb)) {
                foundNextFrame = nextFrame;
                break;
            }
        }
        if (!foundNextFrame) {
            return;
        }

        VNLogDebug("  Base::releaseTemporalBuffer found: plane:%" PRIu32 " next_ts:%" PRIx64
                   " ts:%" PRIx64,
                   plane, foundNextFrame->timestamp, frame->timestamp);

        foundNextFrame->updateTemporalBuffer(plane);
    }
}

// End of frame processing
//
void PipelineBase::baseDone(LdpPicture* picture)
{
    assert(picture);

    // Generate event
    m_eventSink->generate(pipeline::EventBasePictureDone, picture);

    // Send base picture back to API
    m_basePictureOutBuffer.push(picture);
}

void PipelineBase::outputDone(FrameBase* frame)
{
    common::ScopedLock lock(m_interTaskMutex);

    // Remove from processing index
    const int idx = m_processingIndex.findUnorderedIndex(compareFramePtr, frame);
    assert(idx != -1);
    m_processingIndex.removeIndex(idx);

    if (!frame->outputPicture) {
        // Hand off to 'flush' index
        frame->setState(FrameStateFlush);
        m_flushIndex.insert(sortFramePtrTimestamp, frame);
    } else {
        // Hand off to 'done' index - even if frame was skipped, so that output
        // picture can be returned to integration (marked as skipped)
        frame->setState(FrameStateDone);
        m_doneIndex.insert(sortFramePtrTimestamp, frame);
        m_interTaskFrameDone.signal();

        m_eventSink->generate(pipeline::EventOutputPictureDone, frame->outputPicture,
                              &frame->decodeInformation);
    }
    m_eventSink->generate(pipeline::EventCanReceive);
}

#if VN_SDK_LOG(DEBUG)
// Dump frame and index state
//
void PipelineBase::logFrames()
{
    char buffer[512];

    VNLogDebug("Frames: %d", m_allocatedFrames.size());
    for (uint32_t i = 0; i < m_allocatedFrames.size(); ++i) {
        FrameBase* const frame{m_allocatedFrames[i]};
        frame->longDescription(buffer, sizeof(buffer));
        VNLogDebugF("  %4d: %s", i, buffer);
        frame->dumpTasks(&m_taskPool);
    }

    logFrameIndex("Reorder", m_reorderIndex);
    logFrameIndex("Processing", m_processingIndex);
    logFrameIndex("Done", m_doneIndex);
    logFrameIndex("Flush", m_flushIndex);

    VNLogDebug("Bases In: %d (%d)", m_basePicturePending.size(), m_basePicturePending.reserved());
    VNLogDebug("Bases Out: %d (%d)", m_basePictureOutBuffer.size(), m_basePictureOutBuffer.capacity());
    VNLogDebug("Output: %d (%d)", m_outputPictureAvailableBuffer.size(),
               m_outputPictureAvailableBuffer.capacity());
    VNLogDebug("Limits Flush:%" PRIx64 " Skip:%" PRIx64 " Processing:%" PRIx64 " Send:%" PRIx64,
               m_flushLimit.load(), m_skipLimit.load(), m_processingLimit.load(), m_sendLimit.load());
}

void PipelineBase::logFrameIndex(const char* indexName, const lcevc_dec::common::Vector<FrameBase*>& index) const
{
    VNLogDebug("Index %s: %d", indexName, index.size());
    for (uint32_t i = 0; i < index.size(); ++i) {
        const FrameBase* const frame{index[i]};
        uint32_t idx = findAllocatedFrame(frame);
        VNLogDebugF("  %2d: %4d ts:%" PRIx64, i, idx, frame->timestamp);
    }
}

#endif

} // namespace lcevc_dec::pipeline
