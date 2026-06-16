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

#ifndef VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_AHB_BUFFER_H
#define VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_AHB_BUFFER_H

#include <LCEVC/build_config.h>

#if VN_OS(ANDROID)

#include "buffer_vulkan_ahb.h"

namespace lcevc_dec::pipeline_vulkan {

// AHardwareBuffer wrapper that imports the underlying allocation as a VkBuffer.
//
// Used for two paths:
//   - BLOB-format AHBs (raw byte storage).
//   - YUV AHBs that the image-import path can't (or can't safely) handle, where
//     shaders read the AHB as a linear storage buffer.  In that case the
//     populateLayout() override fills in the actual per-plane row strides /
//     offsets reported by AHardwareBuffer_lockPlanes() so shader offsets line
//     up with the underlying memory.
class BufferVulkanAhbBuffer : public BufferVulkanAhb
{
public:
    BufferVulkanAhbBuffer(BackendVulkan& backend, AHardwareBuffer* hardwareBuffer,
                          const AHardwareBuffer_Desc& desc,
                          const VkAndroidHardwareBufferPropertiesANDROID& ahbProps,
                          VkDeviceSize bufferSize);
    ~BufferVulkanAhbBuffer() override;

    VkBuffer getVkBuffer() const override { return m_buffer; }
    void clearCmd(VkCommandBuffer commandBuffer) override;

    // True if the constructor managed to import the AHB and bind memory.
    bool isImported() const { return m_buffer != VK_NULL_HANDLE; }

    VNNoCopyNoMove(BufferVulkanAhbBuffer);

protected:
    void populateLayout(const AHardwareBuffer_Desc& desc) override;

private:
    bool importAsBuffer(VkDeviceSize bufferSize, const VkAndroidHardwareBufferPropertiesANDROID& ahbProps);

    VkBuffer m_buffer{VK_NULL_HANDLE};
};

} // namespace lcevc_dec::pipeline_vulkan

#endif

#endif // VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_AHB_BUFFER_H
