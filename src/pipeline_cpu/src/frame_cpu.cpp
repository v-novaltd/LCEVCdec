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

#include "frame_cpu.h"
//
#include "from_base.h"
#include "picture_cpu.h"
#include "pipeline_config_cpu.h"
#include "tasks_cpu.h"

#include <LCEVC/common/log.h>
#include <LCEVC/enhancement/config_parser.h>
#include <LCEVC/enhancement/config_types.h>
#include <LCEVC/pipeline/buffer_alignment.h>

namespace lcevc_dec::pipeline_cpu {

FrameCPU::FrameCPU(LdcMemoryAllocator* enhancementAllocator, LdcMemoryAllocator* bufferAllocator,
                   uint64_t timestamp)
    : FrameBase(enhancementAllocator, bufferAllocator, timestamp)
    , m_bufferAllocator(bufferAllocator)
{}

void FrameCPU::getBasePlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const
{
    const PictureCPU* picture = fromPipeline(basePicture);
    picture->getPlaneDescInternal(plane, planeDesc);
}

void FrameCPU::getOutputPlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const
{
    const PictureCPU* picture = fromPipeline(outputPicture);
    picture->getPlaneDescInternal(plane, planeDesc);
}

// Set up intermediate buffers
//
// Allocate intermediate buffers for each LOQ/plane that needs it.
//
bool FrameCPU::initializeIntermediateBuffers()
{
    // Already done?
    if (m_intermediateInitialized) {
        return true;
    }

    if (!hasGoodConfig()) {
        return false;
    }

    const LdpColorFormat format = getBaseColorFormat();

    if (globalConfig->initialized && globalConfig->numPlanes > 1 &&
        (baseFormat == LdpColorFormatNV12_8 || baseFormat == LdpColorFormatNV21_8)) {
        VNLogError("Base pipeline doesn't support NV12 base pictures with chroma residuals");
        return false;
    }

    // Allocate buffers starting at LOQ0, down to LOQ2 - As we go down, if there is no scaling
    // between layers, then the buffer will be shared with lower LOQ.
    for (int8_t loq = LOQ0; loq <= LOQ2; loq++) {
        uint16_t width = 0;
        uint16_t height = 0;
        if (globalConfig->initialized) {
            ldePlaneDimensionsFromConfig(globalConfig, static_cast<LdeLOQIndex>(loq), 0, &width, &height);
        } else {
            // Some sort of passthrough - use base size
            width = static_cast<uint16_t>(baseWidth);
            height = static_cast<uint16_t>(baseHeight);
        }

        ldpInternalPictureLayoutInitialize(&m_intermediateLayout[loq], format, width, height,
                                           kBufferRowAlignment);

        const uint8_t numPlanes = std::min(ldpPictureLayoutPlanes(&m_intermediateLayout[loq]),
                                           static_cast<uint8_t>(RCMaxPlanes));

        for (uint8_t plane = 0; plane < numPlanes; plane++) {
            if (needsIntermediateBuffer(static_cast<LdeLOQIndex>(loq), plane)) {
                // Create an internal buffer for this LoQ/plane
                const uint32_t loqSize = ldpPictureLayoutPlaneSize(&m_intermediateLayout[loq], plane);
                VNAllocateIdAlignedArray(m_bufferAllocator, &m_intermediateBufferAllocation[plane][loq],
                                         uint8_t, kBufferRowAlignment, loqSize,
                                         "FrameBase_IntermediateBuffer", timestamp);
                if (!VNIsAllocated(m_intermediateBufferAllocation[plane][loq])) {
                    return false;
                }
                m_intermediateBufferPtr[plane][loq] =
                    VNAllocationPtr(m_intermediateBufferAllocation[plane][loq], uint8_t);

                VNLogVerbose("Intermediate buffer %" PRIx64 ": LoQ:%d Plane:%d %ux%u:%d %p", timestamp,
                             loq, plane, ldpPictureLayoutPlaneWidth(&m_intermediateLayout[loq], plane),
                             ldpPictureLayoutPlaneHeight(&m_intermediateLayout[loq], plane),
                             (int)ldpPictureLayoutFormat(&m_intermediateLayout[loq]),
                             (void*)m_intermediateBufferPtr[plane][loq]);
            } else {
                // Share internal buffer from higher LoQ
                if (loq != LOQ0) {
                    m_intermediateBufferPtr[plane][loq] = m_intermediateBufferPtr[plane][loq - 1];
                } else {
                    m_intermediateBufferPtr[plane][loq] = nullptr;
                }
            }
        }
    }

    m_intermediateInitialized = true;
    return true;
}

void FrameCPU::releaseIntermediateBuffers()
{
    // Release intermediate buffers
    for (int8_t loq = LOQ0; loq <= LOQ2; loq++) {
        for (uint8_t plane = 0; plane < RCMaxPlanes; plane++) {
            if (VNIsAllocated(m_intermediateBufferAllocation[plane][loq])) {
                VNFree(m_bufferAllocator, &m_intermediateBufferAllocation[plane][loq]);
            }
        }
    }

    m_intermediateInitialized = false;
}

// Return true if frame needs an intermediate buffer for given loq/plane
//
bool FrameCPU::needsIntermediateBuffer(LdeLOQIndex loq, uint8_t plane) const
{
    if (plane > globalConfig->numPlanes &&
        ((globalConfig->scalingModes[LOQ0] != Scale0D) != (globalConfig->scalingModes[LOQ1] != Scale0D)) &&
        globalConfig->baseDepth == globalConfig->enhancedDepth) {
        return false;
    }

    if (loq == LOQ0) {
        return true;
    }

    if (globalConfig->scalingModes[loq - 1] != Scale0D) {
        return true;
    }

    return false;
}

void FrameCPU::getIntermediatePlaneDesc(uint32_t plane, LdeLOQIndex loq, LdpPicturePlaneDesc& planeDesc) const
{
    assert(loq < LOQMaxCount);
    assert(plane < RCMaxPlanes);
    assert(m_intermediateBufferPtr[plane][loq]);

    planeDesc.firstSample = m_intermediateBufferPtr[plane][loq];
    planeDesc.rowByteStride = ldpPictureLayoutRowStride(&m_intermediateLayout[loq], plane);
}

const LdpPictureLayout* FrameCPU::getIntermediateLayout(LdeLOQIndex loq) const
{
    assert(loq < LOQMaxCount);
    return &m_intermediateLayout[loq];
}

void FrameCPU::getTemporalBufferPlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const
{
    assert(plane < RCMaxPlanes);
    assert(m_temporalBuffer[plane]);

    TemporalBufferCPU* tb = fromBase(m_temporalBuffer[plane]);
    planeDesc = tb->planeDesc;
}

void FrameCPU::generateTasks(pipeline::PipelineBase* pipeline, uint64_t previousTimestamp)
{
    localGenerateTasks(static_cast<PipelineCPU*>(pipeline), this, previousTimestamp);
}

} // namespace lcevc_dec::pipeline_cpu
