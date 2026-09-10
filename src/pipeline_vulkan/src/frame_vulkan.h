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

#ifndef VN_LCEVC_PIPELINE_VULKAN_FRAME_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_FRAME_VULKAN_H

#include <LCEVC/common/task_pool.h>
#include <LCEVC/pipeline/frame_base.h>

#include <atomic>
#include <cassert>
#include <vector>

namespace lcevc_dec::pipeline_vulkan {

class PipelineVulkan;
class PictureVulkan;

enum class VulkanComputeOutcome : uint8_t
{
    Pending,
    Succeeded,
    Cancelled,
    Failed,
};

class FrameVulkan : public pipeline::FrameBase
{
public:
    FrameVulkan(LdcMemoryAllocator* enhancementAllocator, LdcMemoryAllocator* bufferAllocator,
                uint64_t timestamp);
    ~FrameVulkan() override;

    void generateTasks(pipeline::PipelineBase* pipeline, uint64_t previousTimestamp) override;

    // Get plane descriptions for each of the buffers
    void getBasePlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const override;
    void getOutputPlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const override;
    void getIntermediatePlaneDesc(uint32_t plane, LdeLOQIndex loq,
                                  LdpPicturePlaneDesc& planeDesc) const override;
    void getTemporalBufferPlaneDesc(uint32_t plane, LdpPicturePlaneDesc& planeDesc) const override;

    bool initializeIntermediateBuffers() override;
    void releaseIntermediateBuffers() override;
    bool needsIntermediateBuffer(LdeLOQIndex loq, uint8_t plane) const override;
    const LdpPictureLayout* getIntermediateLayout(LdeLOQIndex loq) const override;

    bool needsGPUCommandBuffers() const override { return true; };

    LdcReturnCode decodeResult() const override
    {
        return computeOutcome() == VulkanComputeOutcome::Failed ? LdcReturnCodeError : LdcReturnCodeSuccess;
    }

    void setIntermediatePicture(uint8_t loq, PictureVulkan* picture)
    {
        assert(loq < LOQMaxCount);
        m_intermediatePicture[loq] = picture;
    }
    PictureVulkan* getIntermediatePicture(uint8_t loq) const
    {
        assert(loq < LOQMaxCount);
        return m_intermediatePicture[loq];
    }

    void addIntermediatePicture(LdpPicture* picture)
    {
        m_intermediatePicturePool.push_back(picture);
    }
    void freeIntermediatePictures(PipelineVulkan* pipeline);

    uint32_t getTotalGpuCommandBufferSize(LdeLOQIndex loq) const;

    void setComputeTaskDone(LdcTaskDependency dep) { m_computeTaskDone = dep; }

    VulkanComputeOutcome computeOutcome() const
    {
        return m_computeOutcome.load(std::memory_order_acquire);
    }

    bool setComputeOutcome(VulkanComputeOutcome outcome);

    /// Signal compute completion after publishing an outcome.
    void signalComputeTaskDone();

    void computeTaskFinish(PipelineVulkan* pipeline);

    VNNoCopyNoMove(FrameVulkan);

private:
    PictureVulkan* m_intermediatePicture[LOQMaxCount]{};

    LdcTaskDependency m_computeTaskDone{kTaskDependencyInvalid};
    std::atomic<VulkanComputeOutcome> m_computeOutcome{VulkanComputeOutcome::Pending};

    std::vector<LdpPicture*> m_intermediatePicturePool;
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_FRAME_VULKAN_H
