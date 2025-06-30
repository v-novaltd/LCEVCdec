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

#include "buffer_vulkan.h"

#include <LCEVC/common/log.h>

namespace lcevc_dec::pipeline_vulkan {

namespace {
    extern const LdpBufferFunctions kBufferVulkanFunctions;
}

BufferVulkan::BufferVulkan(PipelineVulkan& pipeline, uint32_t size)
    : LdpBuffer{&kBufferVulkanFunctions}
    , m_pipeline(pipeline)
{
    if (size > 0) {
        createBufferAndMemory(size);
    }
}

BufferVulkan::~BufferVulkan() { destroy(); }

void BufferVulkan::destroy()
{
    VkDevice& device = m_pipeline.getDevice();
    if (VNIsAllocated(m_allocation)) {
        vkUnmapMemory(device, m_memory);
        vkFreeMemory(device, m_memory, nullptr);
        vkDestroyBuffer(device, m_buffer, nullptr);
    }
}

bool BufferVulkan::createBufferAndMemory(uint32_t size)
{
    VkDevice& device = m_pipeline.getDevice();
    if (!device) {
        VNLogError("No Vulkan device");
        return false;
    }
    const auto queueFamilyIndex = m_pipeline.getQueueFamilyIndex();

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.queueFamilyIndexCount = 1;
    bufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.size = static_cast<VkDeviceSize>(size);
    if (vkCreateBuffer(device, &bufferCreateInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        VNLogError("failed to create VkBuffer");
        return false;
    }

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkMemoryRequirements bufferMemoryRequirements;
    vkGetBufferMemoryRequirements(device, m_buffer, &bufferMemoryRequirements);

    allocateInfo.allocationSize = bufferMemoryRequirements.size;
    allocateInfo.memoryTypeIndex = m_pipeline.findMemoryType(
        bufferMemoryRequirements.memoryTypeBits,
        static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    if (vkAllocateMemory(device, &allocateInfo, nullptr, &m_memory) != VK_SUCCESS) {
        VNLogError("failed to allocate VkDeviceMemory");
        return false;
    }

    if (vkBindBufferMemory(device, m_buffer, m_memory, 0) != VK_SUCCESS) {
        VNLogError("failed to bind VkBuffer to VkDeviceMemory");
        return false;
    }

    void* mappedPtr{};
    if (vkMapMemory(device, m_memory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&mappedPtr)) !=
        VK_SUCCESS) {
        VNLogError("failed to map VkDeviceMemory");
        return false;
    }

    m_allocation.ptr = mappedPtr;
    m_allocation.size = size;
    m_allocation.alignment = 0;
    m_allocation.allocatorData = 0;

    return true;
}

bool BufferVulkan::map(LdpBufferMapping* mapping, int32_t offset, uint32_t mapSize, LdpAccess access)
{
    // Is this mapped already?
    if (m_mapped) {
        return false;
    }

    // Does requested window fit in buffer?
    if (offset < 0 || (offset + mapSize) > size()) {
        return false;
    }

    // Record details
    mapping->buffer = this;
    mapping->offset = offset;
    mapping->size = mapSize;
    mapping->ptr = VNAllocationPtr(m_allocation, uint8_t) + offset;
    mapping->access = access;
    mapping->userData = (void*)this;

    m_mapped = true;
    return true;
}

void BufferVulkan::unmap(const LdpBufferMapping* mapping)
{
    VNUnused(mapping);

    assert(mapping->userData == (void*)this);
    m_mapped = false;
}

void BufferVulkan::clear() // NOLINT(readability-make-member-function-const)
{
    if (!VNIsAllocated(m_allocation)) {
        return;
    }

    memset(VNAllocationPtr(m_allocation, uint8_t), 0, VNAllocationSize(m_allocation, uint8_t));
}

uint8_t* BufferVulkan::ptr() const { return VNAllocationPtr(m_allocation, uint8_t); }

uint32_t BufferVulkan::size() const
{
    return static_cast<uint32_t>(VNAllocationSize(m_allocation, uint8_t));
}

bool BufferVulkan::resize(uint32_t size)
{
    destroy();
    return createBufferAndMemory(size);
}

// C function table to connect to C++ class
//
namespace {
    bool map(struct LdpBuffer* buffer, LdpBufferMapping* mapping, int32_t offset, uint32_t size,
             LdpAccess access)
    {
        return static_cast<BufferVulkan*>(buffer)->map(mapping, offset, size, access);
    }
    void unmap(struct LdpBuffer* buffer, const LdpBufferMapping* mapping)
    {
        static_cast<BufferVulkan*>(buffer)->unmap(mapping);
    }

    const LdpBufferFunctions kBufferVulkanFunctions = {
        map,
        unmap,
    };
} // namespace

} // namespace lcevc_dec::pipeline_vulkan
