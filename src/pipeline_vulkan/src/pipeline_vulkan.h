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

#ifndef VN_LCEVC_PIPELINE_VULKAN_PIPELINE_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_PIPELINE_VULKAN_H

#include "backend_vulkan.h"
#include "buffer_vulkan.h"
#include "buffer_vulkan_vma.h"
#include "frame_vulkan.h"
#include "picture_vulkan.h"
#include "pipeline_builder_vulkan.h"
#include "pipeline_config_vulkan.h"
#include "temporal_buffer_vulkan.h"

#include <LCEVC/common/free_pool.hpp>
#include <LCEVC/common/rolling_arena.h>
#include <LCEVC/common/threads.hpp>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/pipeline/enhancement_tile.h>
#include <LCEVC/pipeline/pipeline_base.h>

#include <array>
#include <atomic>
#include <deque>
#include <vector>

namespace lcevc_dec::pipeline_vulkan {

class BufferVulkan;
class FrameVulkan;
class PictureVulkan;
class PipelineBuilderVulkan;
struct VulkanApplyCommonArgs;
struct VulkanApplyTileArgs;

struct VulkanConversionArgs;
struct VulkanUpscaleArgs;

class PipelineVulkan : public pipeline::PipelineBase
{
public:
    PipelineVulkan(const PipelineBuilderVulkan& builder, pipeline::EventSink* eventSink);
    ~PipelineVulkan() override;

    LdcReturnCode renderInit() override { return LdcReturnCodeSuccess; }
    LdcReturnCode renderSendPicture(uint64_t timestamp, LdpPicture* outputPicture,
                                    const LdpRenderSendInformation* renderSendInformation,
                                    uint64_t delayUs) override;
    LdpPicture* renderReceivePicture(LdpRenderReceiveInformation& renderReceiveInformation) override
    {
        VNUnused(renderReceiveInformation);
        return nullptr;
    }
    LdcReturnCode renderSetWindow(void* window, bool secure) override;

    const PipelineConfigVulkan& configuration() const override { return m_configuration; }

    BufferVulkan* allocateBuffer(uint32_t requiredSize, pipeline::BufferUsage usage) override;
    void releaseBuffer(pipeline::BufferBase* buffer) override;

    PictureVulkan* allocatePicture() override;
    void releasePicture(pipeline::PictureBase* picture) override;

    LdcReturnCode storeAccelBuffer(pipeline::PictureBase* picture, const LdpPictureDesc& desc,
                                   LdpAccelBuffer* accelBuffer) override;

    FrameVulkan* allocateFrame(uint64_t timestamp) override;
    void freeFrame(pipeline::FrameBase* frame) override;

    pipeline::TemporalBufferBase* getTemporalBuffer(uint32_t n) override;
    LdppDitherGlobal* ditherGlobal() override { return &m_dither; }

    BackendVulkan& backend() { return m_backend; }
    bool isInitialised() const { return m_initialised; }
    void prepareApplyCommonArgs(VulkanApplyCommonArgs& args, PictureVulkan* picture,
                                LdpEnhancementTile* enhancementTile, VulkanFrameContext* context,
                                bool applyDirect);
    void prepareApplyTileArgs(VulkanApplyTileArgs& args, PictureVulkan* picture,
                              LdpEnhancementTile* enhancementTile, VulkanFrameContext* context,
                              bool applyDirect);

    void getSubsamplingShifts(LdeChroma chroma, int& widthShift, int& heightShift);
    LdpColorFormat chromaToColorFormat(LdeChroma chroma);

    // XXX This is wrong - per frame chrom is not a global setting
    LdeChroma getChroma() const { return m_chroma; }
    void setChroma(LdeChroma chroma) { m_chroma = chroma; }

    void addCompletionContext(VulkanFrameContext* context);
    void waitCompletionContextIdle();
    void frameCompletionThread();
    static intptr_t frameCompletionThreadEntry(void* arg);

    void finishCompletionContext(VulkanFrameContext* context);

    // Called in frame task-generation order. Each dependency belongs to its own frame's group.
    LdcTaskDependency registerSubmissionTurn(FrameVulkan* frame);
    // Hand off after submission (or cancellation/failure), independently of GPU completion.
    void completeSubmissionTurn(FrameVulkan* frame);

    VNNoCopyNoMove(PipelineVulkan);

    PictureVulkan* m_temporalPicture{};

private:
    friend PipelineBuilderVulkan;

    void checkCompletion();

    const LdppDitherFrame* findFreedFrameDither(uint64_t timestamp) const;

    PipelineConfigVulkan m_configuration;

    // Fast allocator for enhancement
    LdcMemorySimpleAllocator m_simpleAllocator{};

    // Protection for allocations that can happen from tasks
    common::Mutex m_allocationBuffersMutex;
    common::Mutex m_allocationPicturesMutex;

    common::FreePool<BufferVulkanVma> m_buffersPool;
    std::vector<BufferVulkanVma*> m_cachedBuffers;
    common::FreePool<PictureVulkan> m_picturesPool;
    common::FreePool<FrameVulkan> m_framesPool;

    lcevc_dec::common::Vector<TemporalBufferVulkan> m_temporalBuffers;

    LdppDitherGlobal m_dither{};

    // Dither state of recently freed frames: a frame is freed when its picture
    // is received, but renderSendPicture() may legitimately be called after
    // that (receive-then-render ordering), and dither is the only per-frame
    // state rendering needs.
    struct FreedFrameDither
    {
        uint64_t timestamp;
        LdppDitherFrame dither;
    };
    std::array<FreedFrameDither, 8> m_freedFrameDither{};
    uint32_t m_freedFrameDitherNext = 0;

    BackendVulkan m_backend;

    bool m_initialised = false;

    LdeChroma m_chroma = LdeChroma::CT420;

    common::Thread* m_completionThread = nullptr;
    std::atomic<bool> m_completionRunning = true;

    common::Mutex m_completionMutex;
    common::CondVar m_completionAdded;
    common::CondVar m_completionRemoved;
    std::vector<VulkanFrameContext*> m_completionContexts;
    uint32_t m_completionsFinishing = 0;

    struct SubmissionTurn
    {
        FrameVulkan* frame;
        LdcTaskDependency ready;
    };
    common::Mutex m_submissionOrderMutex;
    std::deque<SubmissionTurn> m_submissionOrder;
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_PIPELINE_VULKAN_H
