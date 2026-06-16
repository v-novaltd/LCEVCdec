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

#include "pipeline_vulkan.h"

#include "buffer_vulkan.h"
#include "compute_vulkan.h"
#include "frame_vulkan.h"
#include "from_base.h"
#include "picture_vulkan.h"
#include "pipeline_builder_vulkan.h"
#include "temporal_buffer_vulkan.h"

#include <LCEVC/common/check.h>
#include <LCEVC/common/limit.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/return_code.h>
#include <LCEVC/pipeline/buffer.h>
#include <LCEVC/pipeline/types.h>
#include <LCEVC/pipeline_vulkan/types_vulkan.h>
#include <vulkan/vulkan_core.h>
//
#include <algorithm>
#include <set>

#if VN_OS(ANDROID)
#include <android/hardware_buffer.h>
#endif

namespace lcevc_dec::pipeline_vulkan {

PipelineVulkan::PipelineVulkan(const PipelineBuilderVulkan& builder, pipeline::EventSink* eventSink)
    : PipelineBase(builder, eventSink)
    , m_configuration(builder.configuration())
    , m_buffersPool(builder.configuration().maxLatency * 4, builder.allocator())
    , m_picturesPool(builder.configuration().maxLatency * 4, builder.allocator())
    , m_framesPool(builder.configuration().maxLatency * 2, builder.allocator())
    , m_temporalBuffers(builder.configuration().numTemporalBuffers * RCMaxPlanes, builder.allocator())
    , m_backend(builder.configuration().willRender, builder.configuration().device,
                builder.configuration().validation, builder.configuration().timestampsLimit,
                builder.configuration().contexts)
{
    ldppDitherGlobalInitialize(m_allocator, &m_dither, m_configuration.ditherSeed);

    if (m_configuration.useSystemAllocator == false) {
        // Special allocator for per frame enhancement data
        m_enhancementAllocator = ldcMemorySimpleAllocatorInitialize(&m_simpleAllocator, m_allocator);
    } else {
        // Use system allocator for all allocations
        m_enhancementAllocator = m_allocator;
    }

    TemporalBufferVulkan buf{};
    buf.allocator = m_allocator;
    buf.desc.timestamp = kInvalidTimestamp;
    buf.timestampLimit = kInvalidTimestamp;
    for (uint32_t i = 0; i < m_configuration.numTemporalBuffers * RCMaxPlanes; ++i) {
        buf.desc.plane = i;
        m_temporalBuffers.append(buf);
    }

    // NB:this assumes that the vulkan shaders will be serialized 'enough' to
    // allow a single intermediate buffer to be used.
    //
    m_intermediateUpscalePicture[LOQ1] = allocatePicture();
    m_intermediateUpscalePicture[LOQ0] = allocatePicture();
    m_temporalPicture = allocatePicture();

    if (m_backend.initVulkan()) {
        m_initialised = true;
    }

    m_completionThread = new common::Thread(frameCompletionThreadEntry, this);
}

PipelineVulkan::~PipelineVulkan()
{
    synchronizeDecoder(kInvalidTimestamp, true);

    m_completionRunning.store(false);
    m_completionAdded.signal();

    delete m_completionThread;
    m_completionThread = nullptr;

    if (m_intermediateUpscalePicture[LOQ1]) {
        releasePicture(m_intermediateUpscalePicture[LOQ1]);
        m_intermediateUpscalePicture[LOQ1] = nullptr;
    }
    if (m_intermediateUpscalePicture[LOQ0]) {
        releasePicture(m_intermediateUpscalePicture[LOQ0]);
        m_intermediateUpscalePicture[LOQ0] = nullptr;
    }

    if (m_temporalPicture) {
        releasePicture(m_temporalPicture);
        m_temporalPicture = nullptr;
    }

    for (uint32_t i = 0; i < m_allocatedPictures.size(); ++i) {
        m_picturesPool.destroy(fromBase(m_allocatedPictures[i]));
    }

    for (uint32_t i = 0; i < m_allocatedFrames.size(); ++i) {
        FrameVulkan* frame{fromBase(m_allocatedFrames[i])};
        frame->release(true);
        m_framesPool.destroy(frame);
    }

    for (uint32_t i = 0; i < m_temporalBuffers.size(); ++i) {
        TemporalBufferVulkan* tb = m_temporalBuffers.at(i);
        if (VNIsAllocated(tb->allocation)) {
            VNFree(m_allocator, &tb->allocation);
        }
    }

    ldppDitherGlobalRelease(&m_dither);

    if (m_configuration.useSystemAllocator == false) {
        ldcMemorySimpleAllocatorDestroy(&m_simpleAllocator);
    }

    if (m_initialised) {
        m_backend.destroy();
    }
}

BufferVulkan* PipelineVulkan::allocateBuffer(uint32_t requiredSize, pipeline::BufferUsage usage)
{
    common::ScopedLock lock(m_allocationBuffersMutex);

    BufferVulkan* const buffer = m_buffersPool.make(m_backend, requiredSize, usage);

    if (!buffer) {
        VNLogFatal("Could not allocate buffer");
        return nullptr;
    }
    return buffer;
}

void PipelineVulkan::releaseBuffer(pipeline::BufferBase* buffer)
{
    common::ScopedLock lock(m_allocationBuffersMutex);
    assert(buffer);

    m_buffersPool.destroy(fromBase(buffer));
}

PictureVulkan* PipelineVulkan::allocatePicture()
{
    common::ScopedLock lock(m_allocationPicturesMutex);

    PictureVulkan* picture{m_picturesPool.make(*this, m_allocator)};
    if (!picture) {
        VNLogError("Could not allocate picture");
        return nullptr;
    }

    m_allocatedPictures.append(picture);
    return picture;
}

void PipelineVulkan::releasePicture(pipeline::PictureBase* picture)
{
    common::ScopedLock lock(m_allocationPicturesMutex);

    uint32_t idx = findAllocatedPicture(picture);
    if (idx == UINT32_MAX) {
        return;
    }

    m_allocatedPictures.removeReorderIndex(idx);

    picture->unbindBuffer();
    m_picturesPool.destroy(fromBase(picture));
}

LdcReturnCode PipelineVulkan::storeAccelBuffer(pipeline::PictureBase* picture,
                                               const LdpPictureDesc& desc, LdpAccelBuffer* accelBuffer)
{
#if VN_OS(ANDROID)
    VNUnused(desc);
    // accelBuffer is an AHardwareBuffer* that was cast into LCEVC_AccelBufferHandle by the caller.
    auto* ahb = reinterpret_cast<AHardwareBuffer*>(accelBuffer); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* pictureVulkan = fromBase(picture);
    if (!pictureVulkan->setAhbBuffer(*this, ahb)) {
        return LdcReturnCodeError;
    }
    return LdcReturnCodeSuccess;
#else
    VNUnused(picture);
    VNUnused(desc);
    VNUnused(accelBuffer);
    VNLogVerbose("storeAccelBuffer: AHardwareBuffer support requires Android");
    return LdcReturnCodeNotSupported;
#endif
}

FrameVulkan* PipelineVulkan::allocateFrame(uint64_t timestamp)
{
    assert(findFrame(timestamp, true) == nullptr);

    FrameVulkan* frame = m_framesPool.make(m_allocator, m_allocator, timestamp);
    if (!frame) {
        VNLogError("Could not allocate frame");
        return nullptr;
    }

    m_allocatedFrames.append(frame);

    return frame;
}

void PipelineVulkan::freeFrame(pipeline::FrameBase* frame)
{
    uint32_t idx = findAllocatedFrame(frame);
    if (idx == UINT32_MAX) {
        return;
    }

    m_allocatedFrames.removeReorderIndex(idx);

    frame->release(true);
    m_framesPool.destroy(fromBase(frame));

    if (m_allocatedFrames.isEmpty()) {
        VNLogDebug("Reset limits");
        m_sendLimit = kInvalidTimestamp;
        m_processingLimit = kInvalidTimestamp;
        m_skipLimit = kInvalidTimestamp;
        m_flushLimit = kInvalidTimestamp;
    }
}

pipeline::TemporalBufferBase* PipelineVulkan::getTemporalBuffer(uint32_t n)
{
    if (n >= m_temporalBuffers.size()) {
        return nullptr;
    }
    return m_temporalBuffers.at(n);
}

// Render
LdcReturnCode PipelineVulkan::renderSetWindow(void* window, bool secure)
{
    VNUnused(secure);
#if VN_OS(ANDROID)
    m_backend.getRenderer().setAndroidWindow(window);
#else
    VNUnused(window);
#endif
    m_backend.initRemaningRender();

    return LdcReturnCodeSuccess;
}

LdcReturnCode PipelineVulkan::renderSendPicture(uint64_t timestamp, LdpPicture* outputPicture,
                                                const LdpRenderSendInformation* renderSendInformation,
                                                uint64_t delayUs)
{
    VNUnused(timestamp);
    VNUnused(delayUs);

    if (!backend().renderingEnabled() || !backend().getRenderer().windowReady()) {
        return LdcReturnCodeAgain;
    }
    if (delayUs > 0) {
        VNLogWarning("renderSendPicture is not implemented yet, rendering with zero delay");
    }

#if defined(USE_GLFW)
    glfwPollEvents();
    auto* window = backend().getRenderer().getWindow();
    if (window == nullptr) {
        return LdcReturnCodeAgain;
    }
    if (glfwWindowShouldClose(window) || glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        backend().getRenderer().releaseSurfaceAndSwapChain();
        backend().getRenderer().closeWindow();
        return LdcReturnCodeAgain;
    }
#endif

    backend().getRenderer().setRotation(renderSendInformation->rotation);

    auto* picture = fromPipeline(outputPicture);

    LdpPictureDesc desc{};
    picture->getDesc(desc);

    const auto pictureWidth = picture->getActiveWidth();
    const auto pictureHeight = picture->getActiveHeight();
    const auto colorFormat = ldpPictureLayoutFormat(&picture->layout);
    const bool nv12 = colorFormat == LdpColorFormatNV12_8;
    const bool bit8 = (colorFormat == LdpColorFormatI420_8 || colorFormat == LdpColorFormatI422_8 ||
                       colorFormat == LdpColorFormatI444_8 || colorFormat == LdpColorFormatNV12_8 ||
                       colorFormat == LdpColorFormatNV21_8 || colorFormat == LdpColorFormatGRAY_8);

    auto* frame = findFrame(timestamp, true);
    const LdppDitherFrame* frameDither = frame ? frame->dither() : nullptr;
    backend().getRenderer().setRenderFormat(bit8, nv12);
    backend().getRenderer().setLimitedRange(desc.colorRange == LdpColorRangeLimited);
    backend().getRenderer().ensureTextureResources(pictureWidth, pictureHeight);
    backend().getRenderer().updateDitherState(&m_dither, frameDither);

    VkImageSubresource subresource{};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkSubresourceLayout layout0;
    vkGetImageSubresourceLayout(backend().getDevice(), backend().getRenderer().textureImage[0],
                                &subresource, &layout0);

    VkSubresourceLayout layout1;
    vkGetImageSubresourceLayout(backend().getDevice(), backend().getRenderer().textureImage[1],
                                &subresource, &layout1);

    VkSubresourceLayout layout2;
    vkGetImageSubresourceLayout(backend().getDevice(), backend().getRenderer().textureImage[2],
                                &subresource, &layout2);

    auto* buffer = fromPipeline(picture->buffer);
    LdpBufferMapping mapping{};
    if (!buffer->map(&mapping, 0, buffer->size(), LdpAccessRead)) {
        VNLogError("Mapping failed");
    }

    const auto* db = mapping.ptr;

    void* data0 = nullptr;
    void* data1 = nullptr;
    void* data2 = nullptr;

    vkMapMemory(backend().getDevice(), backend().getRenderer().textureImageMemory[0], 0,
                layout0.size, 0, &data0);
    vkMapMemory(backend().getDevice(), backend().getRenderer().textureImageMemory[1], 0,
                layout1.size, 0, &data1);
    vkMapMemory(backend().getDevice(), backend().getRenderer().textureImageMemory[2], 0,
                layout2.size, 0, &data2);

    const uint8_t* srcY = db + picture->layout.planeOffsets[0];
    uint8_t* dstY = static_cast<uint8_t*>(data0);

    for (uint32_t y = 0; y < pictureHeight; ++y) {
        memcpy(dstY + y * layout0.rowPitch, srcY + y * picture->layout.rowStrides[0], pictureWidth);
    }

    if (nv12) {
        const uint8_t* uvSrc = db + picture->layout.planeOffsets[1];
        for (uint32_t y = 0; y < pictureHeight / 2; ++y) {
            const uint8_t* rowSrc = uvSrc + y * picture->layout.rowStrides[1];
            uint8_t* rowU = static_cast<uint8_t*>(data1) + y * layout1.rowPitch;
            uint8_t* rowV = static_cast<uint8_t*>(data2) + y * layout2.rowPitch;

            for (uint32_t x = 0; x < pictureWidth / 2; ++x) {
                rowU[x] = rowSrc[2 * x + 0];
                rowV[x] = rowSrc[2 * x + 1];
            }
        }
    } else {
        for (uint32_t y = 0; y < pictureHeight / 2; ++y) {
            memcpy(static_cast<uint8_t*>(data1) + y * layout1.rowPitch,
                   db + picture->layout.planeOffsets[1] + y * picture->layout.rowStrides[1],
                   picture->layout.rowStrides[1]);
        }

        for (uint32_t y = 0; y < pictureHeight / 2; ++y) {
            memcpy(static_cast<uint8_t*>(data2) + y * layout2.rowPitch,
                   db + picture->layout.planeOffsets[2] + y * picture->layout.rowStrides[2],
                   picture->layout.rowStrides[2]);
        }
    }

    vkUnmapMemory(backend().getDevice(), backend().getRenderer().textureImageMemory[2]);
    vkUnmapMemory(backend().getDevice(), backend().getRenderer().textureImageMemory[1]);
    vkUnmapMemory(backend().getDevice(), backend().getRenderer().textureImageMemory[0]);

    buffer->unmap(&mapping);

    backend().getRenderer().drawFrame();

    return LdcReturnCodeSuccess;
}

//
//
void PipelineVulkan::addCompletionContext(VulkanFrameContext* context)
{
    common::ScopedLock lock(m_completionMutex);
    m_completionContexts.push_back(context);
    m_completionAdded.signal();
}

void PipelineVulkan::waitCompletionContextIdle()
{
    while (true) {
        common::ScopedLock lock(m_completionMutex);
        // No completion - done
        if (m_completionContexts.empty()) {
            return;
        }
        // Wait for something to be removed
        m_completionRemoved.wait(lock);

        // Check again
        if (m_completionContexts.empty()) {
            return;
        }
    }
}

// Thread that waits on fences from submitted command buffers
//
intptr_t PipelineVulkan::frameCompletionThreadEntry(void* arg)
{
    static_cast<PipelineVulkan*>(arg)->frameCompletionThread();
    return 0;
}

void PipelineVulkan::frameCompletionThread()
{
    while (m_completionRunning) {
        // Local snapshot of fences and context to wait for
        std::vector<VkFence> fences;
        std::vector<VulkanFrameContext*> contexts;

        {
            // Wait for there to be submitted contexts, then lock and
            // make a copy of fences and pointers
            common::ScopedLock lock(m_completionMutex);
            if (m_completionContexts.empty()) {
                const uint64_t deadline = threadTimeMicroseconds(50 * 1000);
                bool signalled = m_completionAdded.waitDeadline(lock, deadline);
                if (!signalled && !m_completionContexts.empty()) {
                    // This should 'never' happen
                    VNLogWarning("timeout when contexts available");
                }
            }

            if (!m_completionContexts.empty()) {
                for (auto* c : m_completionContexts) {
                    fences.push_back(c->getFence());
                    contexts.push_back(c);
                }
            }
        }

        if (!fences.empty()) {
            // If there are active submissions - wait
            //
            // Use a 100ms timeout in case of shaders taking too long, or blocking
            //
            constexpr uint64_t TIMEOUT_NANOSECS = (uint64_t)100 * 1000 * 1000;

            VkResult r = vkWaitForFences(m_backend.getDevice(), static_cast<uint32_t>(fences.size()),
                                         fences.data(), false, TIMEOUT_NANOSECS);
            if (r == VK_SUCCESS) {
                // At least one of the fences is signalled - update pending vector
                //
                // Also, keep a record of the completed contexts for completion outside lock
                //
                common::ScopedLock lock(m_completionMutex);

                for (size_t i = 0; i < fences.size(); ++i) {
                    VkResult fr = vkGetFenceStatus(m_backend.getDevice(), fences[i]);
                    if (fr == VK_SUCCESS) {
                        // Remove context from pending  set
                        m_completionContexts.erase(find(m_completionContexts.begin(),
                                                        m_completionContexts.end(), contexts[i]));

                        // Release
                        finishCompletionContext(contexts[i]);

                        m_completionRemoved.broadcast();
                    }
                }
            } else if (r == VK_TIMEOUT) {
                VNLogWarning("Completion wait timeout");
            } else {
                VNLogError("vkWaitForFences failed: %d", (int32_t)r);
            }
        }
    }
}

void PipelineVulkan::finishCompletionContext(VulkanFrameContext* context)
{
    // Frame finishing
    //
    if (FrameVulkan* frame = context->getFrame()) {
        frame->computeTaskFinish(this);
    }

    // Debugging shader timing
    context->getTimestamps().report();

    m_backend.compute().releaseFrameContext(context);
}

void PipelineVulkan::prepareApplyCommonArgs(VulkanApplyCommonArgs& args, PictureVulkan* picture,
                                            LdpEnhancementTile* enhancementTile,
                                            VulkanFrameContext* context, bool applyDirect)
{
    args.picture = applyDirect ? picture : nullptr;

    int widthShift{};
    int heightShift{};
    if (enhancementTile->plane != 0) {
        getSubsamplingShifts(m_chroma, widthShift, heightShift);
    }
    args.chroma = m_chroma;
    args.planeWidth = picture->getActiveWidth() >> widthShift;
    args.planeHeight = picture->getActiveHeight() >> heightShift;
    args.plane = enhancementTile->plane;
    args.loq = enhancementTile->loq;
    args.dds = context->getFrame()->globalConfig->numLayers == 16;
    args.tuRasterOrder = !context->getFrame()->globalConfig->temporalEnabled &&
                         context->getFrame()->globalConfig->tileDimensions == TDTNone;
    args.context = context;
}

void PipelineVulkan::prepareApplyTileArgs(VulkanApplyTileArgs& args, PictureVulkan* picture,
                                          LdpEnhancementTile* enhancementTile,
                                          VulkanFrameContext* context, bool applyDirect)
{
    args.picture = applyDirect ? picture : nullptr;

    int widthShift{};
    int heightShift{};
    if (enhancementTile->plane != 0) {
        getSubsamplingShifts(m_chroma, widthShift, heightShift);
    }
    args.planeWidth = picture->getActiveWidth() >> widthShift;
    args.planeHeight = picture->getActiveHeight() >> heightShift;
    args.plane = enhancementTile->plane;
    args.bufferGpu = enhancementTile->bufferGpu;
    args.tileX = enhancementTile->tileX;
    args.tileY = enhancementTile->tileY;
    args.tileWidth = enhancementTile->tileWidth;
    args.loq = enhancementTile->loq;
    args.highlightResiduals = m_configuration.highlightResiduals;
    args.dds = context->getFrame()->globalConfig->numLayers == 16;
    args.tuRasterOrder = !context->getFrame()->globalConfig->temporalEnabled &&
                         context->getFrame()->globalConfig->tileDimensions == TDTNone;
    args.context = context;
}

void PipelineVulkan::getSubsamplingShifts(LdeChroma chroma, int& widthShift, int& heightShift)
{
    widthShift = 0;
    heightShift = 0;
    switch (chroma) {
        case LdeChroma::CT420: heightShift = 1; [[fallthrough]];
        case LdeChroma::CT422: widthShift = 1; break;
        default: break;
    }
}

LdpColorFormat PipelineVulkan::chromaToColorFormat(LdeChroma chroma)
{
    switch (chroma) {
        case LdeChroma::CTMonochrome: return LdpColorFormatGRAY_16_LE;
        case LdeChroma::CT420: return LdpColorFormatI420_16_LE;
        case LdeChroma::CT422: return LdpColorFormatI422_16_LE;
        case LdeChroma::CT444: return LdpColorFormatI444_16_LE;
        default: return LdpColorFormatUnknown;
    }
}

} // namespace lcevc_dec::pipeline_vulkan
