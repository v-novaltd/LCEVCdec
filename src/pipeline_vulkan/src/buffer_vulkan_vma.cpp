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

#include "buffer_vulkan_vma.h"

#include "backend_vulkan.h"

#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/pipeline/buffer_base.h>
#include <LCEVC/pipeline/types.h>
#include <vulkan/vulkan_core.h>

#include <limits>

namespace lcevc_dec::pipeline_vulkan {

namespace {
    VkBufferUsageFlags toVkBufferUsageFlags(BufferVulkanBinding binding)
    {
        switch (binding) {
            case BufferVulkanBinding::Storage: return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            case BufferVulkanBinding::Uniform: return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }
        assert(false);
        return 0;
    }
} // namespace

BufferVulkanVma::BufferVulkanVma(BackendVulkan& pipeline, uint32_t size,
                                 pipeline::BufferUsage usage, BufferVulkanBinding binding)
    : m_pipeline(pipeline)
    , m_usage(usage)
    , m_binding(binding)
{
    if (size > 0) {
        createBufferAndMemory(size);
    }
}

BufferVulkanVma::~BufferVulkanVma() { destroy(); }

void BufferVulkanVma::destroy()
{
    VNTraceScopedNamedArgs("vulkan:BufferVulkanVma::destroy", "bytes", m_size);
    assert(m_mapped == false);

    if (m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_pipeline.getVmaAllocator(), m_buffer, m_vmaAllocation);
        m_buffer = VK_NULL_HANDLE;
    }
    m_vmaAllocation = {};
    m_size = 0;
}

pipeline::BufferUsage BufferVulkanVma::usage() const { return m_usage; }

uint32_t BufferVulkanVma::size() const { return m_size; }

bool BufferVulkanVma::createBufferAndMemory(uint32_t size)
{
    VNTraceScopedNamedArgs("vulkan:BufferVulkanVma::createBufferAndMemory", "bytes", size, "usage",
                           static_cast<uint32_t>(m_usage));
    assert(m_ptr == nullptr);
    assert(m_size == 0);
    assert(m_buffer == VK_NULL_HANDLE);

    VkDevice& device = m_pipeline.getDevice();
    if (!device) {
        VNLogError("No Vulkan device");
        return false;
    }
    const QueueFamilyIndices indices = m_pipeline.getQueueFamilies();
    if (!indices.computeFamily) {
        VNLogError("failed to get compute family index");
        return false;
    }
    const uint32_t queueFamilyIndex = indices.computeFamily.value();

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.queueFamilyIndexCount = 1;
    bufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;
    bufferCreateInfo.usage = toVkBufferUsageFlags(m_binding);
    bufferCreateInfo.size = static_cast<VkDeviceSize>(size);

    VmaAllocationCreateInfo allocInfo = {};

    // Choose allocation usage and flags based on buffer usage
    //
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT is not set - there is explicit
    // invalidation and flushing when buffers are mapped and unmapped.
    //
    switch (m_usage) {
        case pipeline::BufferUsageIn:
            bufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            break;
        case pipeline::BufferUsageOut:
            bufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            break;
        case pipeline::BufferUsageInternal:
            bufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            allocInfo.flags = 0;
            break;
        default: assert(0); break;
    }

    const VkResult result = vmaCreateBuffer(m_pipeline.getVmaAllocator(), &bufferCreateInfo,
                                            &allocInfo, &m_buffer, &m_vmaAllocation, nullptr);
    if (result != VK_SUCCESS) {
        VNLogError("vmaCreateBuffer failed: %d", static_cast<int32_t>(result));
        m_buffer = VK_NULL_HANDLE;
        m_vmaAllocation = {};
        return false;
    }

    m_size = size;

    return true;
}

bool BufferVulkanVma::map(LdpBufferMapping* mapping, int32_t offset, uint32_t mapSize, LdpAccess access)
{
    // Device-local buffers cannot be mapped
    if (m_usage == pipeline::BufferUsageInternal) {
        VNLogError("Cannot map a device-local buffer");
        assert(0);
        return false;
    }

    // Is this mapped already?
    if (m_mapped) {
        assert(0);
        return false;
    }

    // Does requested window fit in buffer?
    if (offset < 0 || static_cast<uint32_t>(offset) > size() ||
        mapSize > size() - static_cast<uint32_t>(offset)) {
        assert(0);
        return false;
    }

    // Do the mapping
    void* mappedPtr{};
    const VkResult mapResult = vmaMapMemory(m_pipeline.getVmaAllocator(), m_vmaAllocation, &mappedPtr);
    if (mapResult != VK_SUCCESS) {
        VNLogError("vmaMapMemory failed: %d", static_cast<int32_t>(mapResult));
        return false;
    }
    if (mappedPtr == nullptr) {
        VNLogError("vmaMapMemory returned a null pointer");
        vmaUnmapMemory(m_pipeline.getVmaAllocator(), m_vmaAllocation);
        return false;
    }

    // Make GPU writes visible before host reads. VMA atom-aligns the requested range.
    if (access != LdpAccessWrite) {
        const VkResult invalidateResult = vmaInvalidateAllocation(
            m_pipeline.getVmaAllocator(), m_vmaAllocation, static_cast<VkDeviceSize>(offset),
            static_cast<VkDeviceSize>(mapSize));
        if (invalidateResult != VK_SUCCESS) {
            VNLogError("vmaInvalidateAllocation failed: %d", static_cast<int32_t>(invalidateResult));
            vmaUnmapMemory(m_pipeline.getVmaAllocator(), m_vmaAllocation);
            return false;
        }
    }

    // Record details
    m_ptr = static_cast<uint8_t*>(mappedPtr);
    mapping->buffer = this;
    mapping->offset = offset;
    mapping->size = mapSize;
    mapping->ptr = m_ptr + offset;
    mapping->access = access;
    mapping->userData = (void*)this;

    m_mapped = true;
    return true;
}

void BufferVulkanVma::unmap(const LdpBufferMapping* mapping)
{
    assert(mapping->userData == (void*)this);

    // Make host writes visible to the GPU. VMA atom-aligns the requested range.
    if (mapping->access != LdpAccessRead) {
        const VkResult flushResult = vmaFlushAllocation(m_pipeline.getVmaAllocator(), m_vmaAllocation,
                                                        static_cast<VkDeviceSize>(mapping->offset),
                                                        static_cast<VkDeviceSize>(mapping->size));
        if (flushResult != VK_SUCCESS) {
            VNLogError("vmaFlushAllocation failed: %d", static_cast<int32_t>(flushResult));
        }
    }

    vmaUnmapMemory(m_pipeline.getVmaAllocator(), m_vmaAllocation);
    m_mapped = false;
    m_ptr = nullptr;
}

void BufferVulkanVma::clear() // NOLINT(readability-make-member-function-const)
{
    if (m_size == 0) {
        return;
    }

    // Device-local buffers cannot be cleared from the host. Use clearCmd()
    // instead within a command buffer recording.
    //
    // This is usually caused by a test trying to manipulate buffers that
    // should have been created as BufferUsageIn for the test.
    if (m_usage == pipeline::BufferUsageInternal) {
        VNLogError("Clearing device local buffer - use BufferUsageInput");
        return;
    }

    assert(m_buffer != VK_NULL_HANDLE);

    const bool alreadyMapped = m_mapped;
    LdpBufferMapping mapping{};
    if (!alreadyMapped && !map(&mapping, 0, m_size, LdpAccessWrite)) {
        return;
    }

    assert(m_ptr != nullptr);
    memset(m_ptr, 0, m_size);

    if (!alreadyMapped) {
        unmap(&mapping);
    } else {
        const VkResult flushResult =
            vmaFlushAllocation(m_pipeline.getVmaAllocator(), m_vmaAllocation, 0, m_size);
        if (flushResult != VK_SUCCESS) {
            VNLogError("vmaFlushAllocation failed: %d", static_cast<int32_t>(flushResult));
        }
    }
}

void BufferVulkanVma::clearCmd(VkCommandBuffer commandBuffer)
{
    if (m_size == 0) {
        return;
    }
    assert(m_buffer != VK_NULL_HANDLE);
    clearWithBarrier(commandBuffer, m_buffer, m_size);
}

void BufferVulkanVma::copyIn(uint32_t dstOffset, const void* src, uint32_t size)
{
    assert(m_usage != pipeline::BufferUsageInternal);
    assert(dstOffset + size <= m_size);
    assert(m_buffer != VK_NULL_HANDLE);

    if (m_size == 0) {
        return;
    }

    const bool alreadyMapped = m_mapped;
    LdpBufferMapping mapping{};
    if (!alreadyMapped) {
        assert(dstOffset <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));
        if (!map(&mapping, static_cast<int32_t>(dstOffset), size, LdpAccessWrite)) {
            return;
        }
    }

    assert(m_ptr != nullptr);
    memcpy(m_ptr + dstOffset, src, size);

    if (!alreadyMapped) {
        unmap(&mapping);
    } else {
        const VkResult flushResult =
            vmaFlushAllocation(m_pipeline.getVmaAllocator(), m_vmaAllocation,
                               static_cast<VkDeviceSize>(dstOffset), static_cast<VkDeviceSize>(size));
        if (flushResult != VK_SUCCESS) {
            VNLogError("vmaFlushAllocation failed: %d", static_cast<int32_t>(flushResult));
        }
    }
}

void BufferVulkanVma::copyOut(void* dst, uint32_t srcOffset, uint32_t size)
{
    assert(m_usage != pipeline::BufferUsageInternal);
    assert(srcOffset + size <= m_size);
    assert(m_buffer != VK_NULL_HANDLE);

    if (m_size == 0) {
        return;
    }

    const bool alreadyMapped = m_mapped;
    LdpBufferMapping mapping{};
    if (!alreadyMapped) {
        assert(srcOffset <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));
        if (!map(&mapping, static_cast<int32_t>(srcOffset), size, LdpAccessRead)) {
            return;
        }
    } else {
        // Preserve any writes through the active mapping before invalidating another range.
        VkResult result = vmaFlushAllocation(m_pipeline.getVmaAllocator(), m_vmaAllocation, 0, m_size);
        if (result != VK_SUCCESS) {
            VNLogError("vmaFlushAllocation failed: %d", static_cast<int32_t>(result));
            return;
        }
        result = vmaInvalidateAllocation(m_pipeline.getVmaAllocator(), m_vmaAllocation,
                                         static_cast<VkDeviceSize>(srcOffset),
                                         static_cast<VkDeviceSize>(size));
        if (result != VK_SUCCESS) {
            VNLogError("vmaInvalidateAllocation failed: %d", static_cast<int32_t>(result));
            return;
        }
    }

    assert(m_ptr != nullptr);
    memcpy(dst, m_ptr + srcOffset, size);

    if (!alreadyMapped) {
        unmap(&mapping);
    }
}

bool BufferVulkanVma::resize(uint32_t size)
{
    destroy();
    return createBufferAndMemory(size);
}

} // namespace lcevc_dec::pipeline_vulkan
