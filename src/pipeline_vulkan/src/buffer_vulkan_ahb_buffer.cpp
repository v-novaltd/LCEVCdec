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

#include "buffer_vulkan_ahb_buffer.h"

#if VN_OS(ANDROID)

#include "backend_vulkan.h"

#include <android/hardware_buffer.h>
#include <dlfcn.h>
#include <LCEVC/common/log.h>
#include <LCEVC/pipeline/picture_layout.h>
#include <vulkan/vulkan.h>

namespace lcevc_dec::pipeline_vulkan {

namespace {
    using LockPlanesFn = int (*)(AHardwareBuffer*, uint64_t, int32_t, const ARect*,
                                 AHardwareBuffer_Planes*);

    int lockHardwareBufferPlanes(AHardwareBuffer* buffer, uint64_t usage, AHardwareBuffer_Planes* planes)
    {
        static auto* lockPlanes =
            reinterpret_cast<LockPlanesFn>(dlsym(RTLD_DEFAULT, "AHardwareBuffer_lockPlanes"));
        if (!lockPlanes) {
            return -1;
        }
        return lockPlanes(buffer, usage, -1, nullptr, planes);
    }
} // namespace

BufferVulkanAhbBuffer::BufferVulkanAhbBuffer(BackendVulkan& backend, AHardwareBuffer* hardwareBuffer,
                                             const AHardwareBuffer_Desc& desc,
                                             const VkAndroidHardwareBufferPropertiesANDROID& ahbProps,
                                             VkDeviceSize bufferSize)
    : BufferVulkanAhb(backend, hardwareBuffer)
{
    if (m_hardwareBuffer == nullptr) {
        return;
    }

    populateLayout(desc);

    if (!importAsBuffer(bufferSize, ahbProps)) {
        // Tear down anything we partially created so the base destructor doesn't
        // try to use it.
        VkDevice device = m_backend.getDevice();
        if (m_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, m_buffer, nullptr);
            m_buffer = VK_NULL_HANDLE;
        }
    }
}

BufferVulkanAhbBuffer::~BufferVulkanAhbBuffer()
{
    if (m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_backend.getDevice(), m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }
}

void BufferVulkanAhbBuffer::populateLayout(const AHardwareBuffer_Desc& desc)
{
    // Start from the base mapping (format + tightly-packed strides).
    BufferVulkanAhb::populateLayout(desc);

    if (!m_hasLayout) {
        return;
    }

    if (desc.format != AHARDWAREBUFFER_FORMAT_YCbCr_P010 &&
        desc.format != AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420) {
        return;
    }

    // Override default tightly-packed strides/offsets with the actual AHB
    // plane layout.  The Vulkan buffer fallback addresses the imported AHB
    // allocation linearly, so shader offsets must match lockPlanes().
    AHardwareBuffer_Planes planes{};
    if (lockHardwareBufferPlanes(m_hardwareBuffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, &planes) != 0 ||
        planes.planeCount < 3) {
        return;
    }

    const auto* y = static_cast<const uint8_t*>(planes.planes[0].data);
    const auto* cb = static_cast<const uint8_t*>(planes.planes[1].data);
    const auto* cr = static_cast<const uint8_t*>(planes.planes[2].data);
    const auto* c = cb < cr ? cb : cr;
    const LdpColorFormat format = ldpPictureLayoutFormat(&m_layout);

    m_layout.rowStrides[0] = planes.planes[0].rowStride;
    m_layout.planeOffsets[0] = 0;
    if (format == LdpColorFormatI420_8) {
        m_layout.rowStrides[1] = planes.planes[1].rowStride;
        m_layout.rowStrides[2] = planes.planes[2].rowStride;
        m_layout.planeOffsets[1] = static_cast<uint32_t>(cb - y);
        m_layout.planeOffsets[2] = static_cast<uint32_t>(cr - y);
        const uint32_t yEnd = m_layout.rowStrides[0] * desc.height;
        const uint32_t cbEnd =
            m_layout.planeOffsets[1] + m_layout.rowStrides[1] * ((desc.height + 1) >> 1);
        const uint32_t crEnd =
            m_layout.planeOffsets[2] + m_layout.rowStrides[2] * ((desc.height + 1) >> 1);
        m_layout.size = yEnd > cbEnd ? yEnd : cbEnd;
        if (crEnd > m_layout.size) {
            m_layout.size = crEnd;
        }
    } else {
        m_layout.rowStrides[1] = planes.planes[1].rowStride;
        m_layout.planeOffsets[1] = static_cast<uint32_t>(c - y);
        const uint32_t yEnd = m_layout.rowStrides[0] * desc.height;
        const uint32_t cEnd =
            m_layout.planeOffsets[1] + m_layout.rowStrides[1] * ((desc.height + 1) >> 1);
        m_layout.size = yEnd > cEnd ? yEnd : cEnd;
    }
    AHardwareBuffer_unlock(m_hardwareBuffer, nullptr);
}

bool BufferVulkanAhbBuffer::importAsBuffer(VkDeviceSize bufferSize,
                                           const VkAndroidHardwareBufferPropertiesANDROID& ahbProps)
{
    VkDevice device = m_backend.getDevice();

    const QueueFamilyIndices indices = m_backend.getQueueFamilies();
    if (!indices.computeFamily) {
        VNLogError("BufferVulkanAhbBuffer: no compute queue family");
        return false;
    }
    const uint32_t queueFamilyIndex = indices.computeFamily.value();

    VkExternalMemoryBufferCreateInfo extInfo{};
    extInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
    extInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = &extInfo;
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.queueFamilyIndexCount = 1;
    bufferCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;

    if (vkCreateBuffer(device, &bufferCreateInfo, nullptr, &m_buffer) != VK_SUCCESS) {
        VNLogError("BufferVulkanAhbBuffer: vkCreateBuffer failed");
        m_buffer = VK_NULL_HANDLE;
        return false;
    }

    // AHardwareBuffer-backed allocations must be dedicated.
    VkImportAndroidHardwareBufferInfoANDROID importInfo{};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    importInfo.buffer = m_hardwareBuffer;

    VkMemoryDedicatedAllocateInfo dedicatedInfo{};
    dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedInfo.buffer = m_buffer;
    dedicatedInfo.pNext = &importInfo;

    uint32_t memoryTypeIndex{};
    if (!m_backend.findMemoryType(ahbProps.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  memoryTypeIndex)) {
        VNLogError("BufferVulkanAhbBuffer: no suitable memory type");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &dedicatedInfo;
    allocInfo.allocationSize = ahbProps.allocationSize;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        VNLogError("BufferVulkanAhbBuffer: vkAllocateMemory failed");
        return false;
    }

    if (vkBindBufferMemory(device, m_buffer, m_memory, 0) != VK_SUCCESS) {
        VNLogError("BufferVulkanAhbBuffer: vkBindBufferMemory failed");
        return false;
    }

    m_size = static_cast<uint32_t>(bufferSize);
    return true;
}

void BufferVulkanAhbBuffer::clearCmd(VkCommandBuffer commandBuffer)
{
    clearWithBarrier(commandBuffer, m_buffer, m_size);
}

} // namespace lcevc_dec::pipeline_vulkan

#endif
