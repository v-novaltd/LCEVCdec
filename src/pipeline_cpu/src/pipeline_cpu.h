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

#ifndef VN_LCEVC_PIPELINE_CPU_PIPELINE_CPU_H
#define VN_LCEVC_PIPELINE_CPU_PIPELINE_CPU_H

#include "pipeline_config_cpu.h"
#include "render_cpu.h"
#include "temporal_buffer_cpu.h"

#include <LCEVC/pipeline/pipeline_base.h>

namespace lcevc_dec::pipeline_cpu {

class BufferCPU;
class FrameCPU;
class PictureCPU;
class PipelineBuilderCPU;

// PipelineCPU
//
class PipelineCPU : public pipeline::PipelineBase
{
public:
    PipelineCPU(const PipelineBuilderCPU& builder, pipeline::EventSink* eventSink);
    ~PipelineCPU() override;

    // Render
    LdcReturnCode renderInit() override
    {
        return RenderCPU::isSupported() ? LdcReturnCodeSuccess : LdcReturnCodeNotSupported;
    }
    LdcReturnCode renderSendPicture(uint64_t timestamp, LdpPicture* outputPicture,
                                    const LdpRenderSendInformation* renderSendInformation,
                                    uint64_t delayUs) override;
    LdpPicture* renderReceivePicture(LdpRenderReceiveInformation& renderReceiveInformation) override
    {
        return nullptr;
    }
    LdcReturnCode renderSetWindow(void* window, bool secure) override
    {
        VNUnused(secure);
        return m_render.setWindow(window);
    }

    const PipelineConfigCPU& configuration() const override { return m_configuration; }

    // Accept an AHardwareBuffer-backed external picture (Android): decode output
    // is written directly into the locked hardware buffer.
    LdcReturnCode storeAccelBuffer(pipeline::PictureBase* picture, const LdpPictureDesc& desc,
                                   LdpAccelBuffer* accelBuffer) override;

    //    LdppDitherGlobal* globalDitherBuffer() override { return &m_dither; }

    // Buffer allocation
    pipeline::BufferBase* allocateBuffer(uint32_t requiredSize, pipeline::BufferUsage usage) override;
    void releaseBuffer(pipeline::BufferBase* buffer) override;

    //// Temporal buffer management
    TemporalBufferCPU* getTemporalBuffer(uint32_t n) override;

    LdppDitherGlobal* ditherGlobal() override { return &m_dither; }

    VNNoCopyNoMove(PipelineCPU);

private:
    friend PipelineBuilderCPU;

    // Configuration
    PipelineConfigCPU m_configuration;

    // Picture allocation
    pipeline::PictureBase* allocatePicture() override;
    void releasePicture(pipeline::PictureBase* picture) override;

    // Given a timestamp, either find existing frame, or create a new one
    pipeline::FrameBase* allocateFrame(uint64_t timestamp) override;
    void freeFrame(pipeline::FrameBase* frame) override;

    // The allocator for image buffer data
    LdcMemoryAllocator* m_bufferAllocator{};

    // Fast allocator for enhancement
    LdcMemorySimpleAllocator m_simpleAllocator{};

    // A recycling buffer allocator for per-frame buffer data
    ldcMemoryRecyclingAllocator m_recyclingAllocator{};

    // Pool of buffers
    common::FreePool<BufferCPU> m_buffersPool;

    // Pool of pictures
    common::FreePool<PictureCPU> m_picturesPool;

    // Pool of frames
    common::FreePool<FrameCPU> m_framesPool;

    // Vector of temporal buffers
    // A small pool of  (1 or more) temporal buffers is allocated on startup, then transferred
    // between frames.
    lcevc_dec::common::Vector<TemporalBufferCPU> m_temporalBuffers;

    // Global dither module
    LdppDitherGlobal m_dither{};

    // Window rendering (Android surface output)
    RenderCPU m_render;
};

} // namespace lcevc_dec::pipeline_cpu

#endif // VN_LCEVC_PIPELINE_CPU_PIPELINE_CPU_H
