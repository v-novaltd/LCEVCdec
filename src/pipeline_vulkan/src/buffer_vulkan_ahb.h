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

#ifndef VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_AHB_H
#define VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_AHB_H

#include <LCEVC/build_config.h>

#if VN_OS(ANDROID)

#include "buffer_vulkan.h"

#include <android/hardware_buffer.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

#include <memory>

namespace lcevc_dec::pipeline_vulkan {

class BackendVulkan;

// Abstract base for BufferVulkan implementations that wrap an externally-supplied
// AHardwareBuffer using VK_ANDROID_external_memory_android_hardware_buffer.
//
// Two concrete flavours derive from this base; pick one with create():
//   - BufferVulkanAhbBuffer : imports the AHB allocation as a VkBuffer
//                              (BLOB and YUV-as-storage-buffer paths).
//   - BufferVulkanAhbImage  : imports the AHB allocation as a 2D VkImage
//                              (RGB and 2-plane YUV image paths).
//
// The wrapper is treated as device-local from the host's perspective: host
// access (map/unmap/clear/copyIn/copyOut) is not supported through this object;
// the caller manages CPU access via AHardwareBuffer_lock/unlock directly on
// the underlying AHardwareBuffer.
class BufferVulkanAhb : public BufferVulkan
{
public:
    // Inspect the AHardwareBuffer and instantiate the appropriate concrete
    // flavour.  Returns nullptr on failure.
    static std::unique_ptr<BufferVulkanAhb> create(BackendVulkan& backend, AHardwareBuffer* hardwareBuffer);

    ~BufferVulkanAhb() override;

    uint32_t size() const override { return m_size; }
    pipeline::BufferUsage usage() const override { return pipeline::BufferUsageInternal; }

    // External memory cannot be resized or host-mapped through this object.
    bool resize(uint32_t size) override;
    bool map(LdpBufferMapping* mapping, int32_t offset, uint32_t size, LdpAccess access) override;
    void unmap(const LdpBufferMapping* mapping) override;
    void clear() override;
    void copyIn(uint32_t dstOffset, const void* src, uint32_t size) override;
    void copyOut(void* dst, uint32_t srcOffset, uint32_t size) override;

    const LdpPictureLayout* getPictureLayout() const override
    {
        return m_hasLayout ? &m_layout : nullptr;
    }

    AHardwareBuffer* getHardwareBuffer() const { return m_hardwareBuffer; }

    VNNoCopyNoMove(BufferVulkanAhb);

protected:
    BufferVulkanAhb(BackendVulkan& backend, AHardwareBuffer* hardwareBuffer);

    // Default implementation: derive an LdpPictureLayout with tightly-packed
    // strides from the AHardwareBuffer description (only known formats).
    // Subclasses may override to apply concrete-flavour-specific adjustments.
    virtual void populateLayout(const AHardwareBuffer_Desc& desc);

    BackendVulkan& m_backend;
    AHardwareBuffer* m_hardwareBuffer{nullptr};
    VkDeviceMemory m_memory{VK_NULL_HANDLE};
    uint32_t m_size{0};

    LdpPictureLayout m_layout{};
    bool m_hasLayout{false};
};

} // namespace lcevc_dec::pipeline_vulkan

#endif

#endif // VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_AHB_H
