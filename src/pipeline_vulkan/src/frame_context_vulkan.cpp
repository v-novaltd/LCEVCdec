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

#include "frame_context_vulkan.h"

#include "backend_vulkan.h"
#include "buffer_vulkan.h"
#include "buffer_vulkan_vma.h"

#include <LCEVC/common/log.h>
#include <LCEVC/common/threads.hpp>
#include <LCEVC/enhancement/bitstream_types.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cassert>
#include <new>

namespace lcevc_dec::pipeline_vulkan {

struct DescriptorSetSpec
{
    VkDescriptorSetLayout layout;
    VkDescriptorSet* destination;
    uint32_t storageBuffers;
    uint32_t storageImages;
    uint32_t uniformBuffers;
};

VulkanFrameContext::~VulkanFrameContext() { freeTmpCmdBuffers(); }

bool VulkanFrameContext::init()
{
    assert(m_backend);

    VkDevice device = m_backend->getDevice();

    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = m_backend->getQueueFamilies().computeFamily.value();
    if (vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        VNLogError("failed to create command pool for context");
        return false;
    }

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = m_commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &m_commandBuffer) != VK_SUCCESS) {
        VNLogError("failed to allocate command buffer");
        destroy();
        return false;
    }

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(device, &fenceInfo, nullptr, &m_fence) != VK_SUCCESS) {
        VNLogError("failed to create fence for context");
        destroy();
        return false;
    }

    VkEventCreateInfo eventInfo{};
    eventInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;

    if (vkCreateEvent(device, &eventInfo, nullptr, &m_event) != VK_SUCCESS) {
        VNLogError("failed to create event for context");
        destroy();
        return false;
    }

    VkTimeDomainEXT timeDomain =
#if VN_OS(WINDOWS)
        VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT;
#else
        VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT;
#endif
    m_timestamps = TimestampVulkan(device, m_backend->getPhysicalDevice(), m_backend->getInstance(),
                                   m_backend->getTimestampsLimit(), timeDomain);

    return true;
}

void VulkanFrameContext::destroy()
{
    assert(m_backend);

    VkDevice device = m_backend->getDevice();

    if (m_fence) {
        vkDestroyFence(device, m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;
    }

    if (m_event) {
        vkDestroyEvent(device, m_event, nullptr);
        m_event = VK_NULL_HANDLE;
    }

    m_timestamps = TimestampVulkan();

    if (m_descriptorPool) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    if (m_commandBuffer) {
        vkFreeCommandBuffers(device, m_commandPool, 1, &m_commandBuffer);
        m_commandBuffer = VK_NULL_HANDLE;
    }

    if (m_commandPool) {
        vkDestroyCommandPool(device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    freeTmpCmdBuffers();
}

bool VulkanFrameContext::allocateDescriptorSets(const DescriptorSetLayouts& layouts)
{
    const std::array specs{
        DescriptorSetSpec{layouts.upscale, &m_descriptorSetUpscaleLoq1, 3, 0, 0},
        DescriptorSetSpec{layouts.upscale, &m_descriptorSetUpscaleLoq0, 3, 0, 0},
        DescriptorSetSpec{layouts.conversion, &m_descriptorSetConversionTo, 3, 0, 0},
        DescriptorSetSpec{layouts.conversion, &m_descriptorSetConversionFrom, 3, 0, 0},
        DescriptorSetSpec{layouts.convertOutImage, &m_descriptorSetConversionFromImage, 2, 2, 0},
        DescriptorSetSpec{layouts.apply, &m_descriptorSetApply[LOQ0], 2, 0, 0},
        DescriptorSetSpec{layouts.apply, &m_descriptorSetApply[LOQ1], 2, 0, 0},
        DescriptorSetSpec{layouts.dequantTransform, &m_descriptorSetDequantTransform, 2, 0, 1},
    };

    std::array<VkDescriptorSetLayout, specs.size()> setLayouts{};
    std::array poolSizes{
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0},
    };
    for (size_t i = 0; i < specs.size(); ++i) {
        setLayouts[i] = specs[i].layout;
        poolSizes[0].descriptorCount += specs[i].storageBuffers;
        poolSizes[1].descriptorCount += specs[i].storageImages;
        poolSizes[2].descriptorCount += specs[i].uniformBuffers;
    }

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets = static_cast<uint32_t>(specs.size());
    descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
    if (vkCreateDescriptorPool(m_backend->getDevice(), &descriptorPoolCreateInfo, nullptr,
                               &m_descriptorPool) != VK_SUCCESS) {
        VNLogError("failed to create descriptor pool");
        return false;
    }

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = static_cast<uint32_t>(specs.size());
    descriptorSetAllocateInfo.pSetLayouts = setLayouts.data();

    std::array<VkDescriptorSet, specs.size()> descriptorSets{};
    if (vkAllocateDescriptorSets(m_backend->getDevice(), &descriptorSetAllocateInfo,
                                 descriptorSets.data()) != VK_SUCCESS) {
        VNLogError("failed to allocate descriptor sets");
        return false;
    }

    for (size_t i = 0; i < specs.size(); ++i) {
        *specs[i].destination = descriptorSets[i];
    }

    return true;
}

bool VulkanFrameContext::begin(const DescriptorSetLayouts& layouts)
{
    assert(m_backend);

    m_eventPrevAtBegin = m_eventPrev;

    if (!m_descriptorSetsAllocated) {
        if (!allocateDescriptorSets(layouts)) {
            VNLogError("Failed to allocate descriptor sets!");
            return false;
        }
        m_descriptorSetsAllocated = true;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (const VkResult result = vkResetCommandBuffer(m_commandBuffer, 0); result != VK_SUCCESS) {
        VNLogError("Failed to reset compute command buffer: %d", static_cast<int32_t>(result));
        return false;
    }

    if (const VkResult result = vkBeginCommandBuffer(m_commandBuffer, &beginInfo); result != VK_SUCCESS) {
        VNLogError("Failed to begin compute command buffer: %d", static_cast<int32_t>(result));
        return false;
    }

    m_timestamps.reset(m_commandBuffer);

    return true;
}

bool VulkanFrameContext::end(VkQueue computeQueue)
{
    if (VkResult r = vkEndCommandBuffer(m_commandBuffer); r != VK_SUCCESS) {
        VNLogError("failed to end command buffer: %d", (int32_t)r);
        return false;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;

    if (VkResult r = m_backend->submitQueue(computeQueue, 1, &submitInfo, m_fence); r != VK_SUCCESS) {
        VNLogError("vkQueueSubmit failed: %d", (int32_t)r);
        return false;
    }

    m_computePipeline = nullptr;
    m_computePipelineLayout = nullptr;

    return true;
}

void VulkanFrameContext::abort()
{
    if (VkResult r = vkResetCommandBuffer(m_commandBuffer, 0); r != VK_SUCCESS) {
        VNLogError("failed to reset aborted command buffer: %d", static_cast<int32_t>(r));
    }

    m_computePipeline = nullptr;
    m_computePipelineLayout = nullptr;
    m_eventPrev = m_eventPrevAtBegin;
}

void VulkanFrameContext::bind(const ComputePipelineLayout* computePipelineLayout,
                              const ComputePipeline* computePipeline, VkDescriptorSet descriptorSet)
{
    m_computePipeline = computePipeline;
    m_computePipelineLayout = computePipelineLayout;

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline->pipeline);

    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_computePipelineLayout->pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
}

void VulkanFrameContext::dispatch(uint32_t width, uint32_t height, const char* label)
{
    assert(m_computePipeline);

    const uint32_t bw = m_computePipeline->workgroup.w * m_computePipeline->packing.w;
    const uint32_t bh = m_computePipeline->workgroup.h * m_computePipeline->packing.h;
    assert(VNIsPowerOfTwo(bw));
    assert(VNIsPowerOfTwo(bh));
    const uint32_t numGroupsX = (width + (bw - 1)) / bw;
    const uint32_t numGroupsY = (height + (bh - 1)) / bh;

    m_timestamps.begin(m_commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, label);
    vkCmdDispatch(m_commandBuffer, numGroupsX, numGroupsY, 1);
    m_timestamps.end(m_commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, label);
}

bool VulkanFrameContext::prepareCommandBuffer(LdeLOQIndex loq, uint32_t size)
{
    assert(m_backend);
    assert(loq < LOQEnhancedCount);

    m_gpuCommandBufferOffset[loq] = 0;
    if (size == 0) {
        return true;
    }

    // GPUCommandBuffer grows as necessary.
    // Could add some sort of shrink mechanism eg: if < 1/2 for 100 consecutive frames?
    if (!m_gpuCommandBuffer[loq] || m_gpuCommandBuffer[loq]->getVkBuffer() == VK_NULL_HANDLE ||
        m_gpuCommandBuffer[loq]->size() == 0 || size > m_gpuCommandBuffer[loq]->size()) {
        auto* replacement = new (std::nothrow)
            BufferVulkanVma(*m_backend, size, pipeline::BufferUsageIn, BufferVulkanBinding::Storage);
        if (replacement == nullptr) {
            VNLogError("Failed to allocate command buffer object: loq=%u size=%u",
                       static_cast<uint32_t>(loq), size);
            return false;
        }
        if (replacement->getVkBuffer() == VK_NULL_HANDLE || replacement->size() == 0) {
            VNLogError("Failed to allocate Vulkan command buffer: loq=%u size=%u",
                       static_cast<uint32_t>(loq), size);
            delete replacement;
            return false;
        }

        delete m_gpuCommandBuffer[loq];
        m_gpuCommandBuffer[loq] = replacement;
    }

    return true;
}

void VulkanFrameContext::freeTmpCmdBuffers()
{
    if (m_gpuCommandBuffer[LOQ0]) {
        delete m_gpuCommandBuffer[LOQ0];
        m_gpuCommandBuffer[LOQ0] = nullptr;
    }

    if (m_gpuCommandBuffer[LOQ1]) {
        delete m_gpuCommandBuffer[LOQ1];
        m_gpuCommandBuffer[LOQ1] = nullptr;
    }

    m_gpuCommandBufferOffset[LOQ0] = 0;
    m_gpuCommandBufferOffset[LOQ1] = 0;
}

void VulkanFrameContext::waitEvent()
{
    if (m_eventPrev != VK_NULL_HANDLE) {
        vkCmdWaitEvents(m_commandBuffer, 1, &m_eventPrev, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, nullptr, 0, nullptr, 0, nullptr);
    }
}

void VulkanFrameContext::setEvent()
{
    vkCmdSetEvent(m_commandBuffer, m_event, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    m_eventPrev = m_event;
}

void VulkanFrameContext::insertComputeBarrier(const char* label)
{
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    m_timestamps.begin(m_commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, label);
    vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    m_timestamps.end(m_commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, label);
}

} // namespace lcevc_dec::pipeline_vulkan
