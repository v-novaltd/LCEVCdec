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

#ifndef VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_H

#include <LCEVC/pipeline/buffer_base.h>
#include <LCEVC/pipeline/picture_layout.h>
#include <vulkan/vulkan.h>

namespace lcevc_dec::pipeline_vulkan {

// Default minimum row alignment for internal allocations
static const auto kVulkanBufferRowAlignment = 4;

// Abstract interface for Vulkan-backed buffers in the pipeline.
//
// Concrete implementations:
//   - BufferVulkanVma        — VMA-allocated buffer used for normal pipeline
//                                buffers.
//   - BufferVulkanAhbBuffer  — externally-supplied AHardwareBuffer imported
//                                as a VkBuffer (Android).
//   - BufferVulkanAhbImage   — externally-supplied AHardwareBuffer imported
//                                as a 2D VkImage (Android).
class BufferVulkan : public pipeline::BufferBase
{
public:
    BufferVulkan() = default;
    ~BufferVulkan() override = default;

    // Underlying Vulkan buffer handle, or VK_NULL_HANDLE if this object is
    // exposed as a VkImage instead.
    virtual VkBuffer getVkBuffer() const = 0;

    // Underlying Vulkan image handle, or VK_NULL_HANDLE if this object is
    // exposed as a VkBuffer
    virtual VkImage getVkImage() const { return VK_NULL_HANDLE; }

    // Image view for storage/sampled image descriptors, or VK_NULL_HANDLE for
    // buffer-backed storage.
    virtual VkImageView getVkImageView(uint32_t plane = 0) const
    {
        VNUnused(plane);
        return VK_NULL_HANDLE;
    }

    // Picture layout implied by this buffer's contents
    const LdpPictureLayout* getPictureLayout() const override { return nullptr; }

    // Add 'clear to zero' for the buffer to the given Vulkan command buffer via vkCmdFillBuffer.
    virtual void clearCmd(VkCommandBuffer commandBuffer) = 0;

    // vkCmdFillBuffer and barrier
    static void clearWithBarrier(VkCommandBuffer commandBuffer, VkBuffer buffer, uint32_t size);

    //  VkImageMemoryBarrier
    static void imageLayoutTransition(VkCommandBuffer commandBuffer, VkImage image,
                                      VkImageLayout oldLayout, VkImageLayout newLayout,
                                      VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                                      VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

    VNNoCopyNoMove(BufferVulkan);
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_H
