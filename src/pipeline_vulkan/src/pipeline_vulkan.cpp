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

    for (BufferVulkanVma* buffer : m_cachedBuffers) {
        m_buffersPool.destroy(buffer);
    }
    m_cachedBuffers.clear();

    if (m_initialised) {
        m_backend.destroy();
    }
}

BufferVulkan* PipelineVulkan::allocateBuffer(uint32_t requiredSize, pipeline::BufferUsage usage)
{
    BufferVulkanVma* buffer = nullptr;
    {
        common::ScopedLock lock(m_allocationBuffersMutex);

        // Prefer the smallest cached allocation that already fits. If none fits, reuse and
        // resize another buffer with the same memory usage rather than consuming another pool slot.
        auto selected = m_cachedBuffers.end();
        for (auto it = m_cachedBuffers.begin(); it != m_cachedBuffers.end(); ++it) {
            if ((*it)->usage() != usage) {
                continue;
            }
            if (selected == m_cachedBuffers.end() ||
                ((*it)->size() >= requiredSize &&
                 ((*selected)->size() < requiredSize || (*it)->size() < (*selected)->size()))) {
                selected = it;
            }
        }

        if (selected != m_cachedBuffers.end()) {
            buffer = *selected;
            *selected = m_cachedBuffers.back();
            m_cachedBuffers.pop_back();
        } else {
            buffer = m_buffersPool.make(m_backend, 0, usage);

            // A resolution or usage change can leave every pool slot in the cache. Discard one
            // stale allocation so the fixed-size object pool can adapt to the new workload.
            if (buffer == nullptr && !m_cachedBuffers.empty()) {
                m_buffersPool.destroy(m_cachedBuffers.back());
                m_cachedBuffers.pop_back();
                buffer = m_buffersPool.make(m_backend, 0, usage);
            }
        }
    }

    if (buffer == nullptr) {
        VNLogError("Could not allocate buffer object: size=%u usage=%d", requiredSize,
                   static_cast<int32_t>(usage));
        return nullptr;
    }

    // Vulkan allocation can occasionally block in the driver for tens of milliseconds. Keep it
    // outside the cache mutex so completed frames can return other buffers concurrently.
    if (buffer->size() < requiredSize && !buffer->resize(requiredSize)) {
        common::ScopedLock lock(m_allocationBuffersMutex);
        m_buffersPool.destroy(buffer);
        VNLogError("Could not resize Vulkan buffer: size=%u usage=%d", requiredSize,
                   static_cast<int32_t>(usage));
        return nullptr;
    }
    if (buffer->getVkBuffer() == VK_NULL_HANDLE || buffer->size() == 0) {
        VNLogError("Could not allocate Vulkan buffer: size=%u usage=%d", requiredSize,
                   static_cast<int32_t>(usage));
        common::ScopedLock lock(m_allocationBuffersMutex);
        m_buffersPool.destroy(buffer);
        return nullptr;
    }

    return buffer;
}

void PipelineVulkan::releaseBuffer(pipeline::BufferBase* buffer)
{
    common::ScopedLock lock(m_allocationBuffersMutex);
    assert(buffer);

    m_cachedBuffers.push_back(fromBase(buffer));
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

    // Keep the frame's dither state: the frame is freed when its picture is
    // received, but renderSendPicture() may still be called for it afterwards.
    if (const LdppDitherFrame* dither = frame->dither()) {
        m_freedFrameDither[m_freedFrameDitherNext] = {frame->timestamp, *dither};
        m_freedFrameDitherNext = (m_freedFrameDitherNext + 1) % m_freedFrameDither.size();
    }

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

const LdppDitherFrame* PipelineVulkan::findFreedFrameDither(uint64_t timestamp) const
{
    for (const FreedFrameDither& entry : m_freedFrameDither) {
        if (entry.timestamp == timestamp && entry.dither.strength != 0) {
            return &entry.dither;
        }
    }
    return nullptr;
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
    m_backend.initRemainingRender();

    return LdcReturnCodeSuccess;
}

LdcReturnCode PipelineVulkan::renderSendPicture(uint64_t timestamp, LdpPicture* outputPicture,
                                                const LdpRenderSendInformation* renderSendInformation,
                                                uint64_t delayUs)
{
    VNUnused(timestamp);
    VNUnused(delayUs);

    if (outputPicture == nullptr || renderSendInformation == nullptr) {
        return LdcReturnCodeInvalidParam;
    }

    if (!backend().renderingEnabled() || !backend().getRenderer().windowReady()) {
        return LdcReturnCodeAgain;
    }
    if (delayUs > 0) {
        VNLogWarning("renderSendPicture delayUs is not implemented yet, rendering with zero delay");
    }

#if defined(USE_GLFW)
    glfwPollEvents();
    GLFWwindow* window = backend().getRenderer().getWindow();
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

    PictureVulkan* picture = fromPipeline(outputPicture);

    if (picture == nullptr) {
        VNLogWarning("Null picture");
        return LdcReturnCodeError;
    }

    LdpPictureDesc desc{};
    picture->getDesc(desc);

    const auto pictureWidth = picture->getActiveWidth();
    const auto pictureHeight = picture->getActiveHeight();
    const auto colorFormat = ldpPictureLayoutFormat(&picture->layout);
    const bool nv12 = colorFormat == LdpColorFormatNV12_8;
    const bool bit8 = (colorFormat == LdpColorFormatI420_8 || colorFormat == LdpColorFormatI422_8 ||
                       colorFormat == LdpColorFormatI444_8 || colorFormat == LdpColorFormatNV12_8 ||
                       colorFormat == LdpColorFormatNV21_8 || colorFormat == LdpColorFormatGRAY_8);

    // The frame may already have been freed by receiveDecoderPicture()
    // (receive-then-render ordering) - dither, the only per-frame render
    // state, is stashed at free time.
    const FrameVulkan* frame = fromBase(findFrame(timestamp, true));
    const LdppDitherFrame* frameDither = frame ? frame->dither() : findFreedFrameDither(timestamp);
    backend().getRenderer().setRenderFormat(bit8, nv12);
    backend().getRenderer().setLimitedRange(desc.colorRange == LdpColorRangeLimited);
    if (!backend().getRenderer().ensureTextureResources(pictureWidth, pictureHeight)) {
        VNLogError("Failed to prepare render texture resources");
        return LdcReturnCodeError;
    }
    if (!backend().getRenderer().waitForTextureUpload()) {
        VNLogError("Failed to wait for render textures");
        return LdcReturnCodeError;
    }
    backend().getRenderer().updateDitherState(&m_dither, frameDither);

    auto* buffer = fromPipeline(picture->buffer);
    if (buffer == nullptr) {
        VNLogWarning("Null buffer");
        return LdcReturnCodeError;
    }

    LdpBufferMapping mapping{};
    if (!buffer->map(&mapping, 0, buffer->size(), LdpAccessRead)) {
        VNLogError("Mapping failed");
        return LdcReturnCodeError;
    }
    if (mapping.ptr == nullptr) {
        VNLogError("Mapping returned a null pointer");
        buffer->unmap(&mapping);
        return LdcReturnCodeError;
    }
    const bool texturePlanesMapped = backend().getRenderer().mapTexturePlanes(
        mapping.ptr, picture->layout, pictureWidth, pictureHeight);
    buffer->unmap(&mapping);

    if (!texturePlanesMapped) {
        VNLogError("Failed to map texture planes");
        return LdcReturnCodeError;
    }

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
