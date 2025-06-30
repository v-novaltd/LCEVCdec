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

// PipelineBuilder
//
// Interface between API and decoder pipeline creation.
//
//
// Pipeline implementations will expose a factory function along the lines of:
//
// std::unique_ptr<PipelineBuilder> createPipelineBuilderXXXX();
//
// Depending on the pipeline, it may need connections to system objects or resources - these
// would be pipeline specific parameters to the factory function.
//
// Configuration settings are passed to the builder, then `finish()` is called to create the actual
// pipeline.
//
// This two stage process is to allow the specialization of the Pipeline implementation depending
// on configuration, and to keep the online interface clear of configuration.
//
#ifndef VN_LCEVC_PIPELINE_CPU_PIPELINE_CPU_H
#define VN_LCEVC_PIPELINE_CPU_PIPELINE_CPU_H

#include "buffer_cpu.h"
#include "pipeline_builder_cpu.h"

#include <LCEVC/common/constants.h>
#include <LCEVC/common/threads.h>
//
#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/ring_buffer.hpp>
#include <LCEVC/common/rolling_arena.h>
#include <LCEVC/common/task_pool.h>
#include <LCEVC/common/threads.hpp>
#include <LCEVC/common/vector.hpp>
#include <LCEVC/enhancement/config_pool.h>
#include <LCEVC/pipeline/event_sink.h>
#include <LCEVC/pipeline/frame.h>
#include <LCEVC/pipeline/pipeline.h>
#include <LCEVC/pixel_processing/dither.h>

namespace lcevc_dec::pipeline_cpu {

class BufferCPU;
class FrameCPU;
class PictureCPU;

// Description of a temporal buffer
struct TemporalBufferDesc
{
    uint64_t timestamp;
    bool clear;
    uint32_t plane;
    uint32_t width;
    uint32_t height;
};

// Temporal buffer associated with pipeline
//
struct TemporalBuffer
{
    // Description of this buffer
    TemporalBufferDesc desc;

    // Timestamp upper limit that this buffer could fulfil
    uint64_t timestampLimit;

    // Frame that is using this buffer or null if available
    FrameCPU* frame;
    // pointer/stride for buffer
    LdpPicturePlaneDesc planeDesc;
    // Buffer allocation
    LdcMemoryAllocation allocation;
};

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

// PipelineCPU
//
class PipelineCPU : public pipeline::Pipeline
{
public:
    PipelineCPU(const PipelineBuilderCPU& builder, pipeline::EventSink* eventSink);
    ~PipelineCPU() override;

    // Send/receive
    LdcReturnCode sendBasePicture(uint64_t timestamp, LdpPicture* basePicture, uint32_t timeoutUs,
                                  void* userData) override;
    LdcReturnCode sendEnhancementData(uint64_t timestamp, const uint8_t* data, uint32_t byteSize) override;
    LdcReturnCode sendOutputPicture(LdpPicture* outputPicture) override;

    LdpPicture* receiveOutputPicture(LdpDecodeInformation& decodeInfoOut) override;
    LdpPicture* receiveFinishedBasePicture() override;

    // "Trick-play"
    LdcReturnCode flush(uint64_t timestamp) override;
    LdcReturnCode peek(uint64_t timestamp, uint32_t& widthOut, uint32_t& heightOut) override;
    LdcReturnCode skip(uint64_t timestamp) override;
    LdcReturnCode synchronize(bool dropPending) override;

    // Picture-handling
    LdpPicture* allocPictureManaged(const LdpPictureDesc& desc) override;
    LdpPicture* allocPictureExternal(const LdpPictureDesc& desc, const LdpPicturePlaneDesc* planeDescArr,
                                     const LdpPictureBufferDesc* buffer) override;

    void freePicture(LdpPicture* picture) override;

    // Accessors for use by frames
    const PipelineConfigCPU& configuration() const { return m_configuration; }
    LdcMemoryAllocator* allocator() const { return m_allocator; }
    LdcTaskPool* taskPool() { return &m_taskPool; }
    LdppDitherGlobal* globalDitherBuffer() { return &m_dither; }

    // Buffer allocation
    BufferCPU* allocateBuffer(uint32_t requiredSize);
    void releaseBuffer(BufferCPU* buffer);

    // Picture allocation
    PictureCPU* allocatePicture();
    void releasePicture(PictureCPU* picture);

    //// Temporal buffer management

    // Mark a frame as needing a temporal buffer, given possible previous timestamp
    LdcTaskDependency requireTemporalBuffer(FrameCPU* frame, uint64_t timestamp, uint32_t plane);

    // Mark the frame as having finished with it's temporal buffer
    void releaseTemporalBuffer(FrameCPU* frame, uint32_t plane);

    // Create task group for this frame
    void generateTasksEnhancement(FrameCPU* frame, uint64_t previousTimestamp);

    void generateTasksPassthrough(FrameCPU* frame);

    void updateTemporalBufferDesc(TemporalBuffer* buffer, const TemporalBufferDesc& desc) const;

#ifdef VN_SDK_LOG_ENABLE_DEBUG
    // Write Debug log of current frame state
    void logFrames() const;
#endif

    VNNoCopyNoMove(PipelineCPU);

private:
    friend PipelineBuilderCPU;

    // Given a timestamp, either find existing frame, or create a new one
    FrameCPU* allocateFrame(uint64_t timestamp);

    // Find the Frame associated with a timestamp, or NULL if none.
    FrameCPU* findFrame(uint64_t timestamp);

    // Frame for given timestamp is finished - release resources
    void releaseFrame(uint64_t timestamp);
    void freeFrame(FrameCPU* frame);

    // Get next frame reference following reorder and flushing rules
    FrameCPU* getNextReordered();

    // Move frames from reorder table to generated tasks
    void startReadyFrames();

    // Assign incoming output pictures to Frames
    void connectOutputPictures();

    // Number of outstanding frames
    uint32_t frameLatency() const;

    // Move any frames before `timestamp` into processing queue
    void startProcessing(uint64_t timestamp);

    // Try to match a frame to current temporal buffer(s)
    TemporalBuffer* matchTemporalBuffer(FrameCPU* frame, uint32_t plane);

    // Create new tasks
    LdcTaskDependency addTaskGenerateCmdBuffer(FrameCPU* frame, LdpEnhancementTile* enhancementTile);
    LdcTaskDependency addTaskConvertToInternal(FrameCPU* frame, uint32_t planeIndex, uint32_t baseDepth,
                                               uint32_t enhancementDepth, LdcTaskDependency input);
    LdcTaskDependency addTaskConvertFromInternal(FrameCPU* frame, uint32_t planeIndex,
                                                 uint32_t baseDepth, uint32_t enhancementDepth,
                                                 LdcTaskDependency dst, LdcTaskDependency src);
    LdcTaskDependency addTaskUpsample(FrameCPU* frame, LdeLOQIndex loq, uint32_t plane,
                                      LdcTaskDependency input);

    LdcTaskDependency addTaskApplyCmdBufferDirect(FrameCPU* frame, LdpEnhancementTile* enhancementTile,
                                                  LdcTaskDependency inputDep,
                                                  LdcTaskDependency cmdBufferDep);

    LdcTaskDependency addTaskApplyCmdBufferTemporal(FrameCPU* frame, LdpEnhancementTile* enhancementTile,
                                                    LdcTaskDependency temporal,
                                                    LdcTaskDependency cmdBufferDep);

    LdcTaskDependency addTaskApplyAddTemporal(FrameCPU* frame, uint32_t planeIndex,
                                              LdcTaskDependency temporalDep, LdcTaskDependency sourceDep);

    LdcTaskDependency addTaskWaitForMany(FrameCPU* frame, const LdcTaskDependency* deps, uint32_t numDeps);
    void addTaskBaseDone(FrameCPU* frame, const LdcTaskDependency* inputDeps, uint32_t inputDepsCount);
    void addTaskOutputDone(FrameCPU* frame, const LdcTaskDependency* deps, uint32_t numDeps);
    LdcTaskDependency addTaskPassthrough(FrameCPU* frame, uint32_t planeIndex,
                                         LdcTaskDependency destDep, LdcTaskDependency srcDep);

    void addTaskTemporalRelease(FrameCPU* frame, const LdcTaskDependency* deps, uint32_t planeIndex);
    // // Task bodies
    static void* taskConvertToInternal(LdcTask* task, const LdcTaskPart* part);
    static void* taskConvertFromInternal(LdcTask* task, const LdcTaskPart* part);
    static void* taskGenerateCmdBuffer(LdcTask* task, const LdcTaskPart* part);
    static void* taskUpsample(LdcTask* task, const LdcTaskPart* part);
    static void* taskApplyCmdBufferDirect(LdcTask* task, const LdcTaskPart* part);
    static void* taskApplyCmdBufferTemporal(LdcTask* task, const LdcTaskPart* part);
    static void* taskApplyAddTemporal(LdcTask* task, const LdcTaskPart* part);
    static void* taskWaitForMany(LdcTask* task, const LdcTaskPart* part);
    static void* taskOutputDone(LdcTask* task, const LdcTaskPart* part);
    static void* taskBaseDone(LdcTask* task, const LdcTaskPart* part);
    static void* taskPassthrough(LdcTask* task, const LdcTaskPart* part);
    static void* taskTemporalRelease(LdcTask* task, const LdcTaskPart* part);

    // Configuration from builder
    const PipelineConfigCPU m_configuration;

    // Interface to event mechanism
    pipeline::EventSink* m_eventSink = nullptr;

    // The system allocator to use
    LdcMemoryAllocator* m_allocator = nullptr;

    // A rolling memory allocator for per-frame blocks
    LdcMemoryAllocatorRollingArena m_rollingArena = {};

    // Enhancement configuration pool
    LdeConfigPool m_configPool = {};

    // Task pool
    LdcTaskPool m_taskPool = {};

    // Vector of Buffer allocations
    lcevc_dec::common::Vector<LdcMemoryAllocation> m_buffers;

    // Vector of Picture allocations
    lcevc_dec::common::Vector<LdcMemoryAllocation> m_pictures;

    // Vector of Frames allocations
    // These frames are NOT in timestamp order.
    // The `reorderIndex` and `processingIndex` vectors contain timestamp-order pointers to the
    // FrameCPU structures.
    lcevc_dec::common::Vector<LdcMemoryAllocation> m_frames;

    // Vector of pending frames pointers during reorder - sorted by timestamp
    lcevc_dec::common::Vector<FrameCPU*> m_reorderIndex;

    // Vector of pending frames pointers whilst in progress - sorted by timestamp
    lcevc_dec::common::Vector<FrameCPU*> m_processingIndex;

    // Limit for frame reordering - can be dynamically updated as enhancement data comes in
    uint32_t m_maxReorder = 0;

    // Vector of temporal buffers
    // A small pool of  (1 or more) temporal buffers is allocated on startup, then passed along
    // between frames.
    lcevc_dec::common::Vector<TemporalBuffer> m_temporalBuffers;

    // The prior frame during initial in-order config parsing - used to negotiate temporal buffers
    uint64_t m_previousTimestamp = kInvalidTimestamp;

    // The timestamp of the last frame to have it's config parseed successfully
    uint64_t m_lastGoodTimestamp = kInvalidTimestamp;

    // Pending base pictures
    lcevc_dec::common::Vector<BasePicture> m_basePicturePending;

    // Base pictures Out - thread safe FIFO
    lcevc_dec::common::RingBuffer<LdpPicture*> m_basePictureOutBuffer;

    // Output pictures available for rendering - thread safe FIFO
    lcevc_dec::common::RingBuffer<LdpPicture*> m_outputPictureAvailableBuffer;

    // Global dither module
    LdppDitherGlobal m_dither;

    // Lock for interaction between frame tasks and pipeline - when temporal buffers
    // are handed over / negotiated.
    //
    // Protects m_temporalBuffers and m_processingIndex
    common::Mutex m_interTaskMutex;

    // Signalled when frames are done, whilst holding m_interTaskMutex
    common::CondVar m_interTaskFrameDone;
};

} // namespace lcevc_dec::pipeline_cpu

#endif // VN_LCEVC_PIPELINE_CPU_PIPELINE_CPU_H
