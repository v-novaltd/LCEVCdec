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

#ifndef VN_LCEVC_PIPELINE_PIPELINE_BASE_H
#define VN_LCEVC_PIPELINE_PIPELINE_BASE_H

#include <LCEVC/common/constants.h>
#include <LCEVC/common/free_pool.hpp>
#include <LCEVC/common/recycling_allocator.h>
#include <LCEVC/common/ring_buffer.hpp>
#include <LCEVC/common/simple_allocator.h>
#include <LCEVC/common/task_pool.h>
#include <LCEVC/common/threads.hpp>
#include <LCEVC/common/vector.hpp>
#include <LCEVC/enhancement/config_pool.h>
#include <LCEVC/pipeline/buffer_base.h>
#include <LCEVC/pipeline/picture.h>
#include <LCEVC/pipeline/pipeline.h>
#include <LCEVC/pipeline/pipeline_builder_base.h>
#include <LCEVC/pipeline/temporal_buffer_base.h>
#include <LCEVC/pixel_processing/dither.h>

// #include <atomic>

namespace lcevc_dec::pipeline {

class BufferBase;
class FrameBase;
class PictureBase;

// A base picture reference and other arguments from sendBase()
//
// Used for pending base pictures, before association with frames.
//
struct BasePicture
{
    uint64_t timestamp;
    LdpPicture* picture;
    uint64_t deadline;
    void* userData;
};

// PipelineBase
//
class PipelineBase : public pipeline::Pipeline
{
public:
    PipelineBase(const PipelineBuilderBase& builder, pipeline::EventSink* eventSink);
    ~PipelineBase() override;

    // Get base pipeline configuration
    virtual const PipelineConfigBase& configuration() const = 0;

    // Buffer allocation
    virtual BufferBase* allocateBuffer(uint32_t requiredSize, pipeline::BufferUsage usage) = 0;
    virtual void releaseBuffer(BufferBase* buffer) = 0;

    // Picture allocation
    virtual PictureBase* allocatePicture() = 0;
    virtual void releasePicture(PictureBase* picture) = 0;

    // Given a timestamp, either find existing frame, or create a new one
    virtual FrameBase* allocateFrame(uint64_t timestamp) = 0;
    virtual void freeFrame(FrameBase* frame) = 0;

    // Get numbered temporal buffer, return 0 if not available
    virtual TemporalBufferBase* getTemporalBuffer(uint32_t n) = 0;

    // Get dither generator
    virtual LdppDitherGlobal* ditherGlobal() = 0;

    // Called by allocPictureExternal when the buffer descriptor carries an accelBuffer.
    // Return LdcReturnCodeNotSupported if the buffer cannot be handled..
    virtual LdcReturnCode storeAccelBuffer(PictureBase* picture, const LdpPictureDesc& desc,
                                           LdpAccelBuffer* accelBuffer);

    // Send/receive
    LdcReturnCode sendDecoderBase(uint64_t timestamp, LdpPicture* basePicture, uint32_t timeoutUs,
                                  void* userData) override;
    LdcReturnCode sendDecoderEnhancementData(uint64_t timestamp, const uint8_t* data,
                                             uint32_t byteSize) override;
    LdcReturnCode sendDecoderPicture(LdpPicture* outputPicture) override;

    LdcReturnCode receiveDecoderPicture(LdpDecodeInformation& decodeInfoOut, LdpPicture*& pictureOut) override;
    LdcReturnCode receiveDecoderBase(LdpPicture*& pictureOut) override;

    void getCapacity(LdpPipelineCapacity* capacity) override;

    // Skip/flush
    LdcReturnCode skip(uint64_t timestamp) override;
    LdcReturnCode flush(uint64_t timestamp) override;
    LdcReturnCode peekDecoder(uint64_t timestamp, uint32_t& widthOut, uint32_t& heightOut) override;
    LdcReturnCode peekDecoderPictureDesc(uint64_t timestamp, LdpPictureDesc* desc) override;

    LdcReturnCode synchronizeDecoder(uint64_t timestamp, bool flushPending) override;

    // Picture-handling
    LdcReturnCode allocPicture(const LdpPictureDesc& desc, LdpPicture*& pictureOut) override;
    LdcReturnCode allocPictureExternal(const LdpPictureDesc& desc, const LdpPicturePlaneDesc* planeDescArr,
                                       const LdpPictureBufferDesc* buffer, LdpPicture*& pictureOut) override;

    void freePicture(LdpPicture* picture) override;

    // Render
    LdcReturnCode renderInit() override { return LdcReturnCodeNotSupported; }
    LdcReturnCode renderSendPicture(uint64_t timestamp, LdpPicture* outputPicture,
                                    const LdpRenderSendInformation* renderSendInformation,
                                    uint64_t delayUs) override
    {
        return LdcReturnCodeNotSupported;
    }
    LdpPicture* renderReceivePicture(LdpRenderReceiveInformation& renderReceiveInformation) override
    {
        return nullptr;
    }

    LdcReturnCode renderSetWindow(void* externalWindow, bool secure) override
    {
        return LdcReturnCodeNotSupported;
    }

    // Check frame against current limits
    bool isProcessing(const FrameBase* frame) const;
    bool isSkipped(const FrameBase* frame) const;
    bool isPassthrough(const FrameBase* frame) const;
    bool isFlushed(const FrameBase* frame) const;

    // Accessors for use by frames
    LdcMemoryAllocator* staticAllocator() const { return m_allocator; }

    LdcTaskPool* taskPool() { return &m_taskPool; }

    //// Temporal buffer management

    // Look through all temporal buffers, looking for one that matches the given frame and plane's requirements
    TemporalBufferBase* findTemporalBuffer(FrameBase* frame, uint32_t plane);

    // Mark the frame as having finished with it's temporal buffer and hand off to next frame that needs it
    void transferTemporalBuffer(FrameBase* frame, uint32_t plane);

    // End of frame processing
    void baseDone(LdpPicture* picture);
    void outputDone(FrameBase* frame);

    void updateTemporalBufferDesc(TemporalBufferBase* buffer, const TemporalBufferDesc& desc) const;

#if VN_SDK_LOG(DEBUG)
    // Write Debug log of current frame state
    void logFrames();
    void logFrameIndex(const char* indexName, const lcevc_dec::common::Vector<FrameBase*>& index) const;
#endif

    VNNoCopyNoMove(PipelineBase);

protected:
    uint32_t findAllocatedPicture(const PictureBase* frame) const;

    // Find the Frame associated with a timestamp, or NULL if none.
    FrameBase* findFrame(uint64_t timestamp, bool ignoreSkipped);

    // Find the index in allocated frames
    uint32_t findAllocatedFrame(const FrameBase* frame) const;

    // Get next frame reference following reorder and flushing rules
    FrameBase* getNextReordered();

    // Move frames from reorder table to generated tasks
    void startReadyFrames();

    // Assign incoming output pictures to Frames
    void connectOutputPictures();

    void unblockSkippedFrames(uint64_t fromTimestamp);
    void unblockFlushedFrames(uint64_t fromTimestamp);
    void releaseFlushedFrames();

    // Number of outstanding frames
    uint32_t frameLatency() const;

    // Move any frames before `timestamp` into processing queue
    void process(uint64_t timestamp);

    // Try to match a frame to current temporal buffer(s)
    TemporalBufferBase* matchTemporalBuffer(FrameBase* frame, uint32_t plane);

    // Interface to event mechanism
    pipeline::EventSink* m_eventSink{};

    // The system allocator to use
    LdcMemoryAllocator* m_allocator{};

    // The allocator for enhancement data
    LdcMemoryAllocator* m_enhancementAllocator{};

    // Vector of Picture allocations
    common::Vector<PictureBase*> m_allocatedPictures;

    // Vector of Frames allocations
    // These frames are NOT in timestamp order.
    // The `m_...Index` vectors contain timestamp-order pointers to theFrameBase structures.
    common::Vector<FrameBase*> m_allocatedFrames;

    // The timestamp of the highest sent frame
    std::atomic<uint64_t> m_sendLimit{kInvalidTimestamp};

    // Timestamp for frames in processing state
    std::atomic<uint64_t> m_processingLimit{kInvalidTimestamp};

    // Timestamp for frames to be skipped
    std::atomic<uint64_t> m_skipLimit{kInvalidTimestamp};

    // Timestamp for frames to be flushed
    std::atomic<uint64_t> m_flushLimit{kInvalidTimestamp};

private:
    //  Enhancement configuration pool
    LdeConfigPool m_configPool{};

    // Task pool
    LdcTaskPool m_taskPool{};

    // Vector of pending frames pointers during reorder - sorted by timestamp
    common::Vector<FrameBase*> m_reorderIndex;

    // Vector of pending frames pointers whilst in progress - sorted by timestamp
    common::Vector<FrameBase*> m_processingIndex;

    // Vector of pending frames pointers when done - sorted by timestamp
    common::Vector<FrameBase*> m_doneIndex;

    // Vector of pending frames pointers when flushed - sorted by timestamp
    common::Vector<FrameBase*> m_flushIndex;

    // Limit for frame reordering - can be dynamically updated as enhancement data comes in
    uint32_t m_maxReorder{};

    // The timestamp of the last frame to have it's config parsed successfully
    uint64_t m_lastGoodTimestamp{kInvalidTimestamp};

    // The prior frame during initial in-order config parsing - used to negotiate temporal buffers
    uint64_t m_previousTimestamp{kInvalidTimestamp};

    // Pending base pictures
    lcevc_dec::common::Vector<BasePicture> m_basePicturePending;

    // Base pictures Out - thread safe FIFO
    lcevc_dec::common::RingBuffer<LdpPicture*> m_basePictureOutBuffer;

    // Output pictures available for rendering - thread safe FIFO
    lcevc_dec::common::RingBuffer<LdpPicture*> m_outputPictureAvailableBuffer;

    // Lock for interaction between frame tasks and pipeline - when temporal buffers
    // are handed over / negotiated.
    //
    // Protects m_temporalBuffers and m_processingIndex
    common::Mutex m_interTaskMutex;

    // Signalled when frames are done, whilst holding m_interTaskMutex
    common::CondVar m_interTaskFrameDone;
};

} // namespace lcevc_dec::pipeline

#endif // VN_LCEVC_PIPELINE_PIPELINE_BASE_H
