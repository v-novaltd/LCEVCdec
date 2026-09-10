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

#include <LCEVC/pipeline/frame_base.h>
//
#include <LCEVC/common/log.h>
#include <LCEVC/common/printf_macros.h>
#include <LCEVC/enhancement/cmdbuffer_cpu.h>
#include <LCEVC/enhancement/cmdbuffer_gpu.h>
#include <LCEVC/enhancement/config_parser.h>
#include <LCEVC/pipeline/buffer_alignment.h>
#include <LCEVC/pipeline/picture_base.h>

namespace lcevc_dec::pipeline {

FrameBase::FrameBase(LdcMemoryAllocator* enhancementAllocator, LdcMemoryAllocator* bufferAllocator,
                     uint64_t timestamp)
    : LdpFrame{}
    , m_enhancementAllocator(enhancementAllocator)
{
    // Only fill in timestamp at this point - full initialization happens after a good config is seen
    this->timestamp = timestamp;
    this->deadline = UINT64_MAX;
}

// Initialize frame, given the resolved configuration.
//
bool FrameBase::initialize(const PipelineConfigBase& configuration, LdcTaskPool* taskPool,
                           LdppDitherGlobal* ditherBuffer)
{
    // Set up the task group
    unsigned maxDependencies = kTaskPoolMaxDependencies;
    ldcTaskGroupInitialize(&m_taskGroup, taskPool, maxDependencies, timestamp);

    // Generate task dependencies for inputs
    m_depBasePicture = ldcTaskDependencyAdd(&m_taskGroup); // NOLINT(cppcoreguidelines-prefer-member-initializer)
    if (basePicture) {
        initializeIntermediateBuffers();
        ldcTaskDependencyMet(&m_taskGroup, m_depBasePicture, basePicture);
    }

    m_depOutputPicture = ldcTaskDependencyAdd(&m_taskGroup); // NOLINT(cppcoreguidelines-prefer-member-initializer)
    if (outputPicture) {
        ldcTaskDependencyMet(&m_taskGroup, m_depOutputPicture, outputPicture);
    }

    if (!initializeCommandBuffers()) {
        return false;
    }

    // Store sharpening override strength from configuration
    m_sharpeningOverrideStrength = configuration.sharpeningOverrideStrength;

    // Figure out dithering strength either from the frame or local config override
    uint8_t strength = 0;
    if (configuration.ditherEnabled) {
        if (configuration.ditherOverrideStrength == -1) {
            strength = static_cast<uint8_t>(config.ditherStrength * config.ditherEnabled *
                                            (config.ditherType != DTNone));
        } else {
            strength = static_cast<uint8_t>(configuration.ditherOverrideStrength);
        }
    }

    return ldppDitherFrameInitialise(&m_frameDither, ditherBuffer, timestamp, strength);
}

// Release resources associated with a frame
//
void FrameBase::release(bool wait)
{
    if (m_taskGroup.pool) {
        if (wait) {
            // Make sure task group is finished
#if VN_SDK_LOG(DEBUG)
            if (m_taskGroup.tasksCount != 0) {
                ldcTaskPoolDump(m_taskGroup.pool, &m_taskGroup);
            }
#endif

            ldcTaskGroupWait(&m_taskGroup);
        }

        ldcTaskGroupDestroy(&m_taskGroup);
    }

    if (VNIsAllocated(m_enhancementData)) {
        VNFree(m_enhancementAllocator, &m_enhancementData);
    }

    ldeConfigsReleaseFrame(&config);
    releaseCommandBuffers();
    releaseIntermediateBuffers();
}

void FrameBase::unblockDependency(LdcTaskDependency dep)
{
    if (dep != kTaskDependencyInvalid) {
        ldcTaskDependencyMet(&m_taskGroup, dep, nullptr);
    }
}

void FrameBase::unblockForSkip()
{
    // Mark the base and output dependencies for this frame as met
    unblockDependency(m_depBasePicture);
    unblockDependency(m_depOutputPicture);
}

void FrameBase::unblockForFlush()
{
    // Mark all dependencies as met
    unblockDependency(m_depBasePicture);
    unblockDependency(m_depOutputPicture);
    for (uint8_t i = 0; i < RCMaxPlanes; ++i) {
        unblockDependency(m_depTemporalBuffer[i]);
    }
}

uint8_t* FrameBase::setEnhancementData(const uint8_t* data, uint32_t byteSize)
{
    if (data == nullptr || byteSize == 0) {
        return nullptr;
    }

    VNAllocateIdArray(m_enhancementAllocator, &m_enhancementData, uint8_t, byteSize,
                      "FrameBase_EnhancementData", timestamp);
    uint8_t* const enhancement{VNAllocationPtr(m_enhancementData, uint8_t)};

    if (!enhancement) {
        return nullptr;
    }

    memcpy(enhancement, data, byteSize);
    return enhancement;
}

// Parse the LCEVC configuration into distinct per-frame data
// Switch to pass-through if configuration parse failed.
bool FrameBase::parseEnhancementData(LdeConfigPool* configPool)
{
    return ldeConfigPoolFrameInsert(configPool, timestamp, VNAllocationPtr(m_enhancementData, uint8_t),
                                    VNAllocationSize(m_enhancementData, uint8_t), &globalConfig, &config);
}

LdcTaskDependency FrameBase::taskAdd(const LdcTaskDependency* inputs, uint32_t inputsCount,
                                     LdcTaskFunction task, const void* data, size_t dataSize,
                                     const char* name)
{
    const LdcTaskDependency output{ldcTaskDependencyAdd(&m_taskGroup)};

    ldcTaskGroupAdd(&m_taskGroup, inputs, inputsCount, output, task, nullptr, 1, 1, dataSize, data, name);

    return output;
}

void FrameBase::taskAddSink(const LdcTaskDependency* inputs, uint32_t inputsCount,
                            LdcTaskFunction task, const void* data, size_t dataSize, const char* name)
{
    ldcTaskGroupAdd(&m_taskGroup, inputs, inputsCount, kTaskDependencyInvalid, task, nullptr, 1, 1,
                    dataSize, data, name);
}
LdcTaskDependency FrameBase::taskDependencyAdd() { return ldcTaskDependencyAdd(&m_taskGroup); }

bool FrameBase::findOutputSetFromBase(LdcTaskDependency* outputs, uint32_t outputsMax,
                                      uint32_t* outputsCount) const
{
    return ldcTaskGroupFindOutputSetFromInput(&m_taskGroup, m_depBasePicture, outputs, outputsMax,
                                              outputsCount);
}

void FrameBase::setOutputPicture(LdpPicture* picture)
{
    assert(m_depOutputPicture != kTaskDependencyInvalid);

    outputPicture = picture;
    ldcTaskDependencyMet(&m_taskGroup, m_depOutputPicture, picture);
}

void FrameBase::waitForTasks() { ldcTaskGroupWait(&m_taskGroup); }

// Set up command buffers
//
bool FrameBase::initializeCommandBuffers()
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

    VNAllocateIdArray(m_enhancementAllocator, &m_enhancementTilesAllocation, LdpEnhancementTile,
                      enhancementTileCount, "FrameBase_EnhancementTiles", timestamp);
    enhancementTiles = VNAllocationPtr(m_enhancementTilesAllocation, LdpEnhancementTile);
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
                et->plane = plane;
                et->tile = tile;
                ldeTileDimensionsFromConfig(globalConfig, static_cast<LdeLOQIndex>(loq), plane,
                                            tile, &et->tileWidth, &et->tileHeight);
                ldeTileStartFromConfig(globalConfig, static_cast<LdeLOQIndex>(loq), plane, tile,
                                       &et->tileX, &et->tileY);
                et->planeWidth = planeWidth;
                et->planeHeight = planeHeight;

                // XXX Not loving this  should refactior CPU/GPU command buffer choices
                if (needsGPUCommandBuffers()) {
                    if (!ldeCmdBufferGpuInitialize(m_enhancementAllocator, &et->bufferGpu,
                                                   &et->bufferGpuBuilder)) {
                        return false;
                    }
                    if (!ldeCmdBufferGpuReset(&et->bufferGpu, &et->bufferGpuBuilder,
                                              globalConfig->numLayers)) {
                        return false;
                    }
                } else {
                    if (!ldeCmdBufferCpuInitializeId(m_enhancementAllocator, &et->buffer, 0, timestamp)) {
                        return false;
                    }
                    if (!ldeCmdBufferCpuReset(&et->buffer, globalConfig->numLayers)) {
                        return false;
                    }
                }
                et++;
            }
        }
    }

    assert((et - enhancementTiles) == enhancementTileCount);
    return true;
}

void FrameBase::releaseCommandBuffers()
{
    // Release command buffers
    for (uint32_t i = 0; i < enhancementTileCount; ++i) {
        if (needsGPUCommandBuffers()) {
            ldeCmdBufferGpuFree(&enhancementTiles[i].bufferGpu, &enhancementTiles[i].bufferGpuBuilder);
        } else {
            ldeCmdBufferCpuFree(&enhancementTiles[i].buffer);
        }
    }
    enhancementTileCount = 0;

    if (VNIsAllocated(m_enhancementTilesAllocation)) {
        VNFree(m_enhancementAllocator, &m_enhancementTilesAllocation);
    }
}

// Return true if frame needs an intermediate buffer for given loq/plane
//
bool FrameBase::needsIntermediateBuffer(LdeLOQIndex loq, uint8_t plane) const
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

LdcReturnCode FrameBase::setBasePicture(LdpPicture* picture, uint64_t frameDeadline, void* baseUserData)
{
    // Can only set base once
    if (basePicture != nullptr) {
        return LdcReturnCodeInvalidParam;
    }

    basePicture = picture;

    // Record metadata for output decoder info
    userData = baseUserData;

    if (basePicture) {
        baseWidth = ldpPictureLayoutWidth(&basePicture->layout);
        baseHeight = ldpPictureLayoutHeight(&basePicture->layout);
        baseBitdepth = ldpPictureLayoutSampleBits(&basePicture->layout);
        baseFormat = ldpPictureLayoutFormat(&basePicture->layout);
        baseSarNum = basePicture->sampleAspectRatio.numerator;
        baseSarDen = basePicture->sampleAspectRatio.denominator;
    }
    deadline = frameDeadline;

    if (m_state != FrameStateReorder) {
        initializeIntermediateBuffers();
    }

    // Mark dependency as met if task group is initialised
    if (m_depBasePicture != kTaskDependencyInvalid) {
        assert(m_taskGroup.pool);
        ldcTaskDependencyMet(&m_taskGroup, m_depBasePicture, basePicture);
    }
    return LdcReturnCodeSuccess;
}

bool FrameBase::canComplete() const
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

LdcTaskDependency FrameBase::needsTemporalBuffer(uint64_t previousTimestamp, uint32_t plane)
{
    const auto [width, height] = temporalDimensions(plane);

    // Fill in requirements
    m_temporalBufferDesc[plane].timestamp = previousTimestamp;
    m_temporalBufferDesc[plane].clear = config.nalType == NTIDR || config.temporalRefresh;
    m_temporalBufferDesc[plane].width = width;
    m_temporalBufferDesc[plane].height = height;
    m_temporalBufferDesc[plane].plane = plane;

    VNLogDebug("needsTemporalBuffer: %" PRIx64 " wants %" PRIx64 " plane %" PRIu32 " (%d %dx%d)",
               timestamp, previousTimestamp, plane, m_temporalBufferDesc[plane].clear, width, height);

    const LdcTaskDependency dep{ldcTaskDependencyAdd(&m_taskGroup)};
    m_depTemporalBuffer[plane] = dep;
    return dep;
}

bool FrameBase::tryAttachTemporalBuffer(uint32_t plane, TemporalBufferBase* temporalBuffer)
{
    assert(temporalBuffer);
    assert(plane < VNArraySize(m_temporalBuffer));
    assert(m_temporalBuffer[plane] == nullptr);
    assert(temporalBuffer->frame == nullptr);

    if (m_depTemporalBuffer[plane] == kTaskDependencyInvalid) {
        return false;
    }

    // Plane must match
    if (temporalBuffer->desc.plane != plane) {
        return false;
    }

    // Match and connect:
    //  Either:  A 'temporal clear' with invalid temporal buffer
    //  Or:      Active temporal buffer with required timestamp
    //
    if ((m_temporalBufferDesc[plane].clear && temporalBuffer->desc.timestamp == kInvalidTimestamp) ||
        (temporalBuffer->desc.timestamp == m_temporalBufferDesc[plane].timestamp)) {
        m_temporalBuffer[plane] = temporalBuffer;
        temporalBuffer->frame = this;
        return true;
    }

    return false;
}

// Make sure temporal buffer match required dimensions, and mark it's dependency as met
void FrameBase::updateTemporalBuffer(uint32_t plane)
{
    assert(plane < VNArraySize(m_temporalBuffer));

    if (!m_temporalBuffer[plane]) {
        return;
    }

    m_temporalBuffer[plane]->update(m_temporalBufferDesc[plane]);

    ldcTaskDependencyMet(&m_taskGroup, m_depTemporalBuffer[plane], m_temporalBuffer[plane]);
}

TemporalBufferBase* FrameBase::detachTemporalBuffer(uint32_t plane)
{
    assert(plane < VNArraySize(m_temporalBuffer));

    TemporalBufferBase* const tb = m_temporalBuffer[plane];
    if (!tb) {
        return nullptr;
    }

    m_temporalBuffer[plane] = nullptr;
    tb->frame = nullptr;

    // Fill in timestamp
    tb->desc.timestamp = timestamp;
    return tb;
}

bool FrameBase::isPlaneEnhanced(LdeLOQIndex loq, uint32_t plane) const
{
    return config.frameConfigSet && config.loqEnabled[loq] && plane < globalConfig->numPlanes;
}

bool FrameBase::sharpeningEnabled() const
{
    if (m_sharpeningOverrideStrength > 0) {
        return true;
    }
    return globalConfig->sharpenType != STDisabled && globalConfig->sharpenStrength != 0.0f;
}

float FrameBase::sharpeningStrength(uint32_t plane) const
{
    if (plane > 0) {
        return 0.0f;
    }
    if (m_sharpeningOverrideStrength > 0) {
        return m_sharpeningOverrideStrength;
    }
    if (globalConfig->sharpenType != STDisabled) {
        return globalConfig->sharpenStrength;
    }
    return 0.0f;
}

LdpColorFormat FrameBase::getUpscaleColorFormat() const
{
    if ((baseFormat == LdpColorFormatNV12_8 || baseFormat == LdpColorFormatNV21_8) &&
        ((globalConfig->scalingModes[LOQ1] != Scale0D) != (globalConfig->scalingModes[LOQ0] != Scale0D))) {
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

// Figure output colour format from frame configuration
LdpColorFormat FrameBase::getOutputColorFormat() const
{
    if (baseFormat == LdpColorFormatNV12_8 || baseFormat == LdpColorFormatNV21_8) {
        if (globalConfig->enhancedDepth != Depth8) {
            VNLogError("Cannot enhance to > 8bit when using an NV12/21 base");
            return LdpColorFormatUnknown;
        }
        return baseFormat;
    }

    return getUpscaleColorFormat();
}

// Figure base colour format from frame configuration
//
// This does not know about interleaving eg: NV12
//
LdpColorFormat FrameBase::getBaseColorFormat() const
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

LdpPictureDesc FrameBase::getOutputPictureDesc(PassthroughMode passthroughMode) const
{
    assert(baseDataValid());

    LdpPictureDesc desc;
    ldpDefaultPictureDesc(&desc, baseFormat, baseWidth, baseHeight);
    desc.sampleAspectRatioNum = baseSarNum;
    desc.sampleAspectRatioDen = baseSarDen;

    // If frame was passed through without modification, report base frame description.
    if (m_passthrough && (passthroughMode != PassthroughMode::Scale)) {
        return desc;
    }

    if (!globalConfig->initialized) {
        VNLogWarning("No global configuration, returning base picture desc");
        return desc;
    }

    // Update desc with post-enhancement description.
    desc.width = globalConfig->width;
    desc.height = globalConfig->height;
    desc.colorFormat = getOutputColorFormat();
    desc.sampleAspectRatioNum = globalConfig->vuiInfo.sarWidth;
    desc.sampleAspectRatioDen = globalConfig->vuiInfo.sarHeight;

    return desc;
}

uint8_t FrameBase::numImagePlanes() const
{
    switch (globalConfig->chroma) {
        case CTMonochrome: return 1;
        case CT420:
        case CT422:
        case CT444: return 3;
        default: assert(0); return 0;
    }
}

std::pair<uint32_t, uint32_t> FrameBase::temporalDimensions(uint32_t plane) const
{
    if (plane == 0) {
        return {globalConfig->width, globalConfig->height};
    }

    switch (globalConfig->chroma) {
        case CT420: return {globalConfig->width >> 1, globalConfig->height >> 1};
        case CT422: return {globalConfig->width >> 1, globalConfig->height};
        case CT444: return {globalConfig->width, globalConfig->height};
        default: assert(0); return {globalConfig->width, globalConfig->height};
    }
}

#if VN_SDK_LOG(DEBUG)
// Write description of frame into string buffer
// Return number of characters written to buffer

namespace {
    const char* frameStateName(FrameState state)
    {
        switch (state) {
            case FrameStateUnknown: return "Unkwn";
            case FrameStateReorder: return "ReOrd";
            case FrameStateProcessing: return "Prcss";
            case FrameStateDone: return "Done ";
            case FrameStateFlush:
                return "Flush";
                //    default:
        }
        return "Unknown";
    }
} // namespace

size_t FrameBase::longDescription(char* buffer, size_t bufferSize) const
{
    return snprintf(buffer, bufferSize,
                    "ts:%" PRIx64 " st:%s pass:%d"
                    " gc:%p base:%p output:%p etc:%d "
                    "esize:%zd tg.tc:%d tg.wt:%d tg.dc:%d tg.met:%" PRIx64 " depB:%d "
                    "depO:%d depT:%d tbd:%" PRIx64 ",%d,%d,%d "
                    "tb:[%p,%p,%p]",
                    timestamp, frameStateName(m_state.load()), m_passthrough, globalConfig,
                    basePicture, outputPicture, enhancementTileCount, m_enhancementData.size,
                    m_taskGroup.tasksCount, m_taskGroup.waitingTasksCount, m_taskGroup.dependenciesCount,
                    m_taskGroup.dependenciesMet ? m_taskGroup.dependenciesMet[0] : 0,
                    m_depBasePicture, m_depOutputPicture, m_depTemporalBuffer[0],
                    m_temporalBufferDesc[0].timestamp, m_temporalBufferDesc[0].clear,
                    m_temporalBufferDesc[0].width, m_temporalBufferDesc[0].height,
                    m_temporalBuffer[0], m_temporalBuffer[1], m_temporalBuffer[2]);
}

void FrameBase::dumpTasks(LdcTaskPool* taskPool) const { ldcTaskPoolDump(taskPool, &m_taskGroup); }
#endif

} // namespace lcevc_dec::pipeline
