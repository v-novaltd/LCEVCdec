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

#ifndef VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_VMA_H
#define VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_VMA_H

#include "backend_vulkan.h"
#include "buffer_vulkan.h"

#include <vk_mem_alloc.h>

namespace lcevc_dec::pipeline_vulkan {

class PictureVulkan;
class BackendVulkan;

enum class BufferVulkanBinding : uint32_t
{
    Storage,
    Uniform
};

// VMA-backed implementation of the BufferVulkan interface. This is the default
// allocator for all internal pipeline buffers.
class BufferVulkanVma : public BufferVulkan
{
public:
    BufferVulkanVma(BackendVulkan& pipeline, uint32_t size, pipeline::BufferUsage usage,
                    BufferVulkanBinding binding);
    ~BufferVulkanVma() override;

    uint32_t size() const override;
    pipeline::BufferUsage usage() const override;

    bool resize(uint32_t size) override;

    bool map(LdpBufferMapping* mapping, int32_t offset, uint32_t size, LdpAccess access) override;
    void unmap(const LdpBufferMapping* mapping) override;

    void clear() override;
    void copyIn(uint32_t dstOffset, const void* src, uint32_t size) override;
    void copyOut(void* dst, uint32_t srcOffset, uint32_t size) override;

    VkBuffer getVkBuffer() const override { return m_buffer; }
    void clearCmd(VkCommandBuffer commandBuffer) override;

    VNNoCopyNoMove(BufferVulkanVma);

private:
    friend PictureVulkan;
    bool createBufferAndMemory(uint32_t size);
    void destroy();

    BackendVulkan& m_pipeline;

    uint32_t m_size{};
    pipeline::BufferUsage m_usage{};
    BufferVulkanBinding m_binding{};

    VkBuffer m_buffer{VK_NULL_HANDLE};
    VmaAllocation m_vmaAllocation{};

    bool m_mapped{false};

    // Mapped pointer (only valid for host-visible (UsageIn/UsageOut) buffers)
    uint8_t* m_ptr{};
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_VMA_H
