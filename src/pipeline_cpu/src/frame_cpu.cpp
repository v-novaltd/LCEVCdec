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

#include "frame_cpu.h"
//
#include "picture_cpu.h"
#include "pipeline_config_cpu.h"

#include <LCEVC/common/log.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/return_code.h>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/enhancement/config_parser.h>
#include <LCEVC/enhancement/config_types.h>
#include <LCEVC/enhancement/dimensions.h>
#include <LCEVC/pipeline/buffer.h>
#include <LCEVC/pipeline/types.h>

#include <algorithm>
#include <cstdint>

namespace lcevc_dec::pipeline_cpu {

FrameCPU::FrameCPU(PipelineCPU* pipeline, uint64_t timestamp)
    : LdpFrame{}
    , m_pipeline(pipeline)
{
    // Only fill in timestamp at this point - full initialization happens
    // when enhancement data is seen
    //
    this->timestamp = timestamp;

    // Set up the task group
    unsigned maxDependencies = kTaskPoolMaxDependencies;
    ldcTaskGroupInitialize(&m_taskGroup, pipeline->taskPool(), maxDependencies);

    // Generate task dependencies for inputs
    m_depBasePicture = ldcTaskDependencyAdd(&m_taskGroup); // NOLINT(cppcoreguidelines-prefer-member-initializer)
    m_depOutputPicture = ldcTaskDependencyAdd(&m_taskGroup); // NOLINT(cppcoreguidelines-prefer-member-initializer)
    for (uint8_t i = 0; i < RCMaxPlanes; i++) {
        m_depTemporalBuffer[i] = kTaskDependencyInvalid;
    }
}

// Allocate per frame buffers
//
bool FrameCPU::initialize()
{
    if (!initializeCommandBuffers() || !initializeIntermediateBuffers())
        return false;

    // Figure out dithering strength either from the frame or local config override
    uint8_t strength = 0;
    if (m_pipeline->configuration().ditherEnabled) {
        if (m_pipeline->configuration().ditherOverrideStrength == -1) {
            strength = config.ditherStrength * config.ditherEnabled * (config.ditherType != DTNone);
        } else {
            strength = m_pipeline->configuration().ditherOverrideStrength;
        }
    }

    return ldppDitherFrameInitialise(&m_frameDither, m_pipeline->globalDitherBuffer(), timestamp, strength);
}

// Generate task graph
//
void FrameCPU::generateTasks(uint64_t previousTimestamp)
{
    // Fill out tasks for this frame
    if (m_pipeline->configuration().showTasks) {
        // Don't consume tasks whilst group is generated
        ldcTaskGroupBlock(&m_taskGroup);
    }

    // Choose pass-through or enhancement task graph generation.
    //
    // If the pass through is 'Scaled', then use the enhancement graph, which
    // will just end up doing scaling as there is no enhancement data.
    if (m_passthrough && (m_pipeline->configuration().passthroughMode != PassthroughMode::Scale ||
                          !globalConfig->initialized)) {
        m_pipeline->generateTasksPassthrough(this);
    } else {
        m_pipeline->generateTasksEnhancement(this, previousTimestamp);
    }

    if (m_pipeline->configuration().showTasks) {
#ifdef VN_SDK_LOG_ENABLE_DEBUG
        ldcTaskPoolDump(m_pipeline->taskPool(), &m_taskGroup);
#endif
        ldcTaskGroupUnblock(&m_taskGroup);
    }
}

// Release resources associated with a frame
//
void FrameCPU::release()
{
    // Make sure task group is finished
    ldcTaskGroupWait(&m_taskGroup);
    ldcTaskGroupDestroy(&m_taskGroup);

    VNFree(m_pipeline->allocator(), &m_enhancementData);

    ldeConfigsReleaseFrame(&config);

    releaseCommandBuffers();
    releaseIntermediateBuffers();
}

// Set up command buffers
//
bool FrameCPU::initializeCommandBuffers()
{
    assert(globalConfig->numPlanes <= 3);

    // Quick scan to find how many enhancement tiles are needed
    //
    // Don't include LoQs or planes that don't have enhancement
    uint32_t count = 0;
    for (int8_t loq = LOQ1; loq >= LOQ0; --loq) {
        if (!config.loqEnabled[loq]) {
            continue;
        }
        for (uint8_t plane = 0; plane < numEnhancedPlanes(); ++plane) {
            count += globalConfig->numTiles[plane][loq];
        }
    }

    // Allocate the enhancement tiles
    enhancementTileCount = count;

    if (enhancementTileCount == 0) {
        enhancementTiles = nullptr;
        return true;
    }

    enhancementTiles = VNAllocateArray(m_pipeline->allocator(), &m_enhancementTilesAllocation,
                                       LdpEnhancementTile, enhancementTileCount);
    if (!enhancementTiles) {
        return false;
    }

    LdpEnhancementTile* et = enhancementTiles;

    // Fill in locations for command buffers
    for (int8_t loq = LOQ1; loq >= LOQ0; --loq) {
        if (!config.loqEnabled[loq]) {
            continue;
        }
        const uint8_t numPlanes = std::min(numEnhancedPlanes(), static_cast<uint8_t>(RCMaxPlanes));
        for (uint8_t plane = 0; plane < numPlanes; ++plane) {
            uint16_t planeWidth = 0;
            uint16_t planeHeight = 0;
            ldePlaneDimensionsFromConfig(globalConfig, static_cast<LdeLOQIndex>(loq), plane,
                                         &planeWidth, &planeHeight);
            const uint32_t numPlaneTiles = globalConfig->numTiles[plane][loq];
            for (uint32_t tile = 0; tile < numPlaneTiles; ++tile) {
                VNClear(et);
                et->loq = static_cast<LdeLOQIndex>(loq);
                et->plane = static_cast<uint8_t>(plane);
                et->tile = tile;
                ldeTileDimensionsFromConfig(globalConfig, static_cast<LdeLOQIndex>(loq), plane,
                                            tile, &et->tileWidth, &et->tileHeight);
                ldeTileStartFromConfig(globalConfig, static_cast<LdeLOQIndex>(loq), plane, tile,
                                       &et->tileX, &et->tileY);
                et->planeWidth = planeWidth;
                et->planeHeight = planeHeight;

                if (!ldeCmdBufferCpuInitialize(m_pipeline->allocator(), &et->buffer, 0)) {
                    return false;
                }
                if (!ldeCmdBufferCpuReset(&et->buffer, globalConfig->numLayers)) {
                    return false;
                }
                et++;
            }
        }
    }

    assert((et - enhancementTiles) == enhancementTileCount);
    return true;
}

void FrameCPU::releaseCommandBuffers()
{
    // Release comamnd buffers
    for (uint32_t i = 0; i < enhancementTileCount; ++i) {
        ldeCmdBufferCpuFree(&enhancementTiles[i].buffer);
    }
    VNFree(m_pipeline->allocator(), &m_enhancementTilesAllocation);
}

// Set up intermediate buffers
//
// Allocate intermediate buffers for each LOQ/plane that needs it.
//
bool FrameCPU::initializeIntermediateBuffers()
{
    const LdpColorFormat format = getBaseColorFormat();

    // Allocate buffers starting at LOQ0, down to LOQ2 - As we go down, if there is no scaling
    // between layers, then the buffer will be shared with lower LOQ.
    for (int8_t loq = LOQ0; loq <= LOQ2; loq++) {
        uint16_t width = 0;
        uint16_t height = 0;
        if (globalConfig->initialized) {
            ldePlaneDimensionsFromConfig(globalConfig, static_cast<LdeLOQIndex>(loq), 0, &width, &height);
        } else {
            width = static_cast<uint16_t>(baseWidth);
            height = static_cast<uint16_t>(baseHeight);
        }

        ldpInternalPictureLayoutInitialize(&m_intermediateLayout[loq], format, width, height,
                                           kBufferRowAlignment);

        const uint8_t numPlanes = std::min(ldpPictureLayoutPlanes(&m_intermediateLayout[loq]),
                                           static_cast<uint8_t>(RCMaxPlanes));

        for (uint8_t plane = 0; plane < numPlanes; plane++) {
            if (needsIntermediateBuffer(static_cast<LdeLOQIndex>(loq), plane)) {
                // Create internal buffer for this LoQ/plane
                const uint32_t loqSize = ldpPictureLayoutPlaneSize(&m_intermediateLayout[loq], plane);
                if (!VNAllocateAlignedArray(m_pipeline->allocator(),
                                            &m_intermediateBufferAllocation[plane][loq], uint8_t,
                                            kBufferRowAlignment, loqSize)) {
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

    return true;
}

void FrameCPU::releaseIntermediateBuffers()
{
    // Release intermediate buffers
    for (uint8_t plane = 0; plane < RCMaxPlanes; plane++) {
        for (int8_t loq = LOQ0; loq <= LOQ2; loq++) {
            if (VNIsAllocated(m_intermediateBufferAllocation[plane][loq])) {
                VNFree(m_pipeline->allocator(), &m_intermediateBufferAllocation[plane][loq]);
            }
        }
    }
}

// Return true if frame needs an intermediate buffer for given loq/plane
//
bool FrameCPU::needsIntermediateBuffer(LdeLOQIndex loq, uint8_t plane) const
{
    VNUnused(plane);

    if (loq == LOQ0) {
        return true;
    }

    if (globalConfig->scalingModes[loq - 1] != Scale0D) {
        return true;
    }

    return false;
}

LdcReturnCode FrameCPU::setBase(LdpPicture* picture, uint64_t deadline, void* baseUserData)
{
    // Can only set base once
    if (basePicture != nullptr) {
        return LdcReturnCodeInvalidParam;
    }

    basePicture = picture;

    // Record metadata for output decoder info
    userData = baseUserData;
    baseWidth = ldpPictureLayoutWidth(&basePicture->layout);
    baseHeight = ldpPictureLayoutHeight(&basePicture->layout);
    baseBitdepth = ldpPictureLayoutSampleBits(&basePicture->layout);
    baseFormat = ldpPictureLayoutFormat(&basePicture->layout);

    m_deadline = deadline;

    // Set base and mark dependency as met
    ldcTaskDependencyMet(&m_taskGroup, m_depBasePicture, basePicture);
    return LdcReturnCodeSuccess;
}

void FrameCPU::getBasePlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const
{
    const PictureCPU* picture = static_cast<const PictureCPU*>(basePicture);
    picture->getPlaneDescInternal(plane, planeDesc);
}

void FrameCPU::getOutputPlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const
{
    const PictureCPU* picture = static_cast<const PictureCPU*>(outputPicture);
    picture->getPlaneDescInternal(plane, planeDesc);
}

void FrameCPU::getIntermediatePlaneDesc(uint32_t plane, LdeLOQIndex loq, LdpPicturePlaneDesc& planeDesc) const
{
    assert(loq < LOQMaxCount);
    assert(plane < RCMaxPlanes);
    assert(m_intermediateBufferPtr[plane][loq]);

    planeDesc.firstSample = m_intermediateBufferPtr[plane][loq];
    planeDesc.rowByteStride = ldpPictureLayoutRowStride(&m_intermediateLayout[loq], plane);
}

bool FrameCPU::canComplete() const
{
    if (m_state != FrameStateProcessing) {
        return false;
    }

    // Collect all the input dependencies
    LdcTaskDependency deps[2 + RCMaxPlanes] = {m_depOutputPicture, m_depBasePicture};
    uint32_t depsCount = 2;

    for (uint8_t i = 0; i < RCMaxPlanes; i++) {
        if (m_depTemporalBuffer[i] != kTaskDependencyInvalid) {
            deps[depsCount++] = m_depTemporalBuffer[i];
        }
    }

    return ldcTaskDependencySetIsMet(&m_taskGroup, deps, depsCount);
}

bool FrameCPU::isEnhanced(LdeLOQIndex loq, uint32_t plane) const
{
    return config.frameConfigSet && config.loqEnabled[loq] && plane < globalConfig->numPlanes;
}

// Figure output colour format from frame configuration
LdpColorFormat FrameCPU::getOutputColorFormat() const
{
    if (baseFormat == LdpColorFormatNV12_8 || baseFormat == LdpColorFormatNV21_8) {
        if (globalConfig->enhancedDepth != Depth8) {
            VNLogError("Cannot enhance to > 8bit when using an NV12/21 base");
            return LdpColorFormatUnknown;
        }
        return baseFormat;
    }

    switch (globalConfig->chroma) {
        case CTMonochrome:
            switch (globalConfig->enhancedDepth) {
                case Depth8: return LdpColorFormatGRAY_8;
                case Depth10: return LdpColorFormatGRAY_10_LE;
                case Depth12: return LdpColorFormatGRAY_12_LE;
                case Depth14: return LdpColorFormatGRAY_14_LE;
                default: return LdpColorFormatUnknown;
            }
        case CT420:
            switch (globalConfig->enhancedDepth) {
                case Depth8: return LdpColorFormatI420_8;
                case Depth10: return LdpColorFormatI420_10_LE;
                case Depth12: return LdpColorFormatI420_12_LE;
                case Depth14: return LdpColorFormatI420_14_LE;
                default: return LdpColorFormatUnknown;
            }
        case CT422:
            switch (globalConfig->enhancedDepth) {
                case Depth8: return LdpColorFormatI422_8;
                case Depth10: return LdpColorFormatI422_10_LE;
                case Depth12: return LdpColorFormatI422_12_LE;
                case Depth14: return LdpColorFormatI422_14_LE;
                default: return LdpColorFormatUnknown;
            }
        case CT444:
            switch (globalConfig->enhancedDepth) {
                case Depth8: return LdpColorFormatI444_8;
                case Depth10: return LdpColorFormatI444_10_LE;
                case Depth12: return LdpColorFormatI444_12_LE;
                case Depth14: return LdpColorFormatI444_14_LE;
                default: return LdpColorFormatUnknown;
            }
        default: return LdpColorFormatUnknown;
    }
}

// Figure base colour format from frame configuration
//
// This does not know about interleaving eg: NV12
//
LdpColorFormat FrameCPU::getBaseColorFormat() const
{
    switch (globalConfig->chroma) {
        case CTMonochrome:
            switch (globalConfig->baseDepth) {
                case Depth8: return LdpColorFormatGRAY_8;
                case Depth10: return LdpColorFormatGRAY_10_LE;
                case Depth12: return LdpColorFormatGRAY_12_LE;
                case Depth14: return LdpColorFormatGRAY_14_LE;
                default: return LdpColorFormatUnknown;
            }
        case CT420:
            switch (globalConfig->baseDepth) {
                case Depth8: return LdpColorFormatI420_8;
                case Depth10: return LdpColorFormatI420_10_LE;
                case Depth12: return LdpColorFormatI420_12_LE;
                case Depth14: return LdpColorFormatI420_14_LE;
                default: return LdpColorFormatUnknown;
            }
        case CT422:
            switch (globalConfig->baseDepth) {
                case Depth8: return LdpColorFormatI422_8;
                case Depth10: return LdpColorFormatI422_10_LE;
                case Depth12: return LdpColorFormatI422_12_LE;
                case Depth14: return LdpColorFormatI422_14_LE;
                default: return LdpColorFormatUnknown;
            }
        case CT444:
            switch (globalConfig->baseDepth) {
                case Depth8: return LdpColorFormatI444_8;
                case Depth10: return LdpColorFormatI444_10_LE;
                case Depth12: return LdpColorFormatI444_12_LE;
                case Depth14: return LdpColorFormatI444_14_LE;
                default: return LdpColorFormatUnknown;
            }
        default: return LdpColorFormatUnknown;
    }
}

LdpPictureDesc FrameCPU::getOutputPictureDesc() const
{
    LdpPictureDesc desc;

    assert(baseDataValid());

    if (!m_passthrough) {
        if (globalConfig->initialized) {
            ldpDefaultPictureDesc(&desc, getOutputColorFormat(), globalConfig->width, globalConfig->height);
        } else {
            VNLogWarning("No global configuration");
            ldpDefaultPictureDesc(&desc, baseFormat, baseWidth, baseHeight);
        }
    } else {
        // Pass-through of some sort
        if (m_pipeline->configuration().passthroughMode == PassthroughMode::Scale &&
            globalConfig->initialized) {
            ldpDefaultPictureDesc(&desc, getOutputColorFormat(), globalConfig->width, globalConfig->height);
        } else {
            ldpDefaultPictureDesc(&desc, baseFormat, baseWidth, baseHeight);
        }
    }

    return desc;
}

uint8_t FrameCPU::numImagePlanes() const
{
    switch (globalConfig->chroma) {
        case CTMonochrome: return 1;
        case CT420:
        case CT422:
        case CT444: return 3;
        default: assert(0); return 0;
    }
}

#ifdef VN_SDK_LOG_ENABLE_DEBUG
// Write description of frame into string buffer
// Return number of characters written to buffer
size_t FrameCPU::longDescription(char* buffer, size_t bufferSize) const
{
    return snprintf(buffer, bufferSize,
                    "ts:%" PRIx64 " gc:%p base:%p output:%p etc:%d "
                    "esize:%zd state:%d tg.tc:%d tg.wt:%d tg.dc:%d tg.met:%" PRIx64 " depB:%d "
                    "depO:%d depT:%d tbd:%" PRIx64 ",%d,%d,%d "
                    "tb:%p rdy:%d skp:%d, pass:%d",
                    timestamp, globalConfig, basePicture, outputPicture, enhancementTileCount,
                    m_enhancementData.size, m_state.load(), m_taskGroup.tasksCount,
                    m_taskGroup.waitingTasksCount, m_taskGroup.dependenciesCount,
                    m_taskGroup.dependenciesMet[0], m_depBasePicture, m_depOutputPicture,
                    m_depTemporalBuffer[0], m_temporalBufferDesc[0].timestamp,
                    m_temporalBufferDesc[0].clear, m_temporalBufferDesc[0].width,
                    m_temporalBufferDesc[0].height, m_temporalBuffer, m_ready, m_skip, m_passthrough);
}
#endif

} // namespace lcevc_dec::pipeline_cpu
