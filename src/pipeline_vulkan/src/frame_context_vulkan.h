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

#ifndef VN_LCEVC_PIPELINE_VULKAN_FRAME_CONTEXT_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_FRAME_CONTEXT_VULKAN_H

#include "compute_vulkan.h"
#include "timestamp_vulkan.h"

#include <LCEVC/enhancement/bitstream_types.h>
#include <vulkan/vulkan.h>
//
#include <cstdint>
#include <memory>

namespace lcevc_dec::pipeline_vulkan {

class BackendVulkan;
class BufferVulkan;
class FrameVulkan;

struct ComputePipelineLayout;
struct ComputePipeline;
struct DescriptorSetLayouts;

class VulkanFrameContext
{
public:
    VulkanFrameContext() = default;
    ~VulkanFrameContext();
    VulkanFrameContext(VulkanFrameContext&&) noexcept = default;
    VulkanFrameContext& operator=(VulkanFrameContext&&) noexcept = default;
    VulkanFrameContext(const VulkanFrameContext&) = delete;
    VulkanFrameContext& operator=(const VulkanFrameContext&) = delete;

    // Initialization and clean-up
    bool init(VkCommandPool commandPool);
    void destroy(VkCommandPool commandPool);

    // Lifecycle
    bool begin(const DescriptorSetLayouts& layouts, uint32_t numTiles);
    bool end(VkQueue computeQueue);

    // Pre-allocate GPU command buffer for LOQ apply.
    void prepareCommandBuffer(LdeLOQIndex loq, uint32_t size);

    // Bind a pipeline and layout
    void bind(const ComputePipelineLayout* computePipelineLayout,
              const ComputePipeline* computePipeline, VkDescriptorSet descriptorSet);

    // Dispatch
    void dispatch(uint32_t width, uint32_t height, const char* label);

    // Events - used to sequence the temporal apply stage - there is a single temporal
    // buffer that is evolved by each frame.
    void waitEvent();
    void setEvent();

    void insertComputeBarrier();

    void freeTmpCmdBuffers();

    // Accessors
    VkCommandBuffer getCommandBuffer() const { return m_commandBuffer; }
    VkFence getFence() const { return m_fence; }
    FrameVulkan* getFrame() const { return m_frame; }
    TimestampVulkan& getTimestamps() { return m_timestamps; }
    const TimestampVulkan& getTimestamps() const { return m_timestamps; }

private:
    friend class ComputeVulkan;

    bool allocateDescriptorSets(const DescriptorSetLayouts& layouts, uint32_t totalNumTiles);

    BackendVulkan* m_backend{};

    VkCommandBuffer m_commandBuffer{};
    VkFence m_fence{};
    FrameVulkan* m_frame{};

    bool m_descriptorSetsAllocated{};
    VkDescriptorPool m_descriptorPool{};

    //    VkDescriptorSet m_descriptorSets[];
    VkDescriptorSet m_descriptorSetSrcMidLoq1{}; // 2 buffers: 1.base -> VERTICAL -> 2.output
    VkDescriptorSet m_descriptorSetMidDstLoq1{}; // 3 buffers: 1.vertical + 2.base -> HORIZONTAL -> 3.output
    VkDescriptorSet m_descriptorSetSrcMidLoq0{}; // 2 buffers: 1.base -> VERTICAL -> 2.output
    VkDescriptorSet m_descriptorSetMidDstLoq0{}; // 3 buffers: 1.vertical + 2.base -> HORIZONTAL -> 3.output
    VkDescriptorSet m_descriptorSetConversionTo{}; // 2 buffers: 1.input -> CONVERSION -> 2.output plane
    VkDescriptorSet m_descriptorSetConversionFrom{}; // 2 buffers: 1.input -> CONVERSION -> 2.output plane
    VkDescriptorSet m_descriptorSetConversionFromImage{}; // 1 buffer + 1 storage image output
    VkDescriptorSet m_descriptorSetAdd{}; // 2 buffers: 1.input -> ADD -> 2.output plane
    VkDescriptorSet m_descriptorSetApply[LOQEnhancedCount]{}; // LOQ0: 1.merged commands -> APPLY -> 2.output plane

    BufferVulkan* m_gpuCommandBuffer[LOQEnhancedCount] = {}; // merged command buffers for LOQ apply tiles
    uint32_t m_gpuCommandBufferOffset[LOQEnhancedCount] = {}; // current write offset (bytes) into LOQ apply cmd buffer

    // Temporal buffer sequencing
    VkEvent m_event{};
    VkEvent m_eventPrev{};

    // Currently bound pipeline and layout
    const ComputePipelineLayout* m_computePipelineLayout{};
    const ComputePipeline* m_computePipeline{};

    // shader profiling
    TimestampVulkan m_timestamps;
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_FRAME_CONTEXT_VULKAN_H
