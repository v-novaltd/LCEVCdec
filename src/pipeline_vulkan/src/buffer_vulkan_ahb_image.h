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

#ifndef VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_AHB_IMAGE_H
#define VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_AHB_IMAGE_H

#include <LCEVC/build_config.h>

#if VN_OS(ANDROID)

#include "buffer_vulkan_ahb.h"

namespace lcevc_dec::pipeline_vulkan {

// AHardwareBuffer wrapper that imports the underlying allocation as a 2D VkImage.
//
// Used for RGB AHBs and for 2-plane YUV AHBs (NV12, P010-style) whose imported
// VkFormat exposes per-plane STORAGE-capable views.  For YUV the wrapper
// creates per-plane R8/R8G8 (or R16/R16G16) image views; for single-plane
// images a single colour image view is created.
class BufferVulkanAhbImage : public BufferVulkanAhb
{
public:
    BufferVulkanAhbImage(BackendVulkan& backend, AHardwareBuffer* hardwareBuffer,
                         const AHardwareBuffer_Desc& desc,
                         const VkAndroidHardwareBufferPropertiesANDROID& ahbProps, VkFormat format);
    ~BufferVulkanAhbImage() override;

    VkBuffer getVkBuffer() const override { return VK_NULL_HANDLE; }
    VkImage getVkImage() const override { return m_image; }
    VkImageView getVkImageView(uint32_t plane = 0) const override;

    void clearCmd(VkCommandBuffer commandBuffer) override;

    // True if the constructor managed to import the AHB and create views.
    bool isImported() const { return m_image != VK_NULL_HANDLE; }

    VNNoCopyNoMove(BufferVulkanAhbImage);

private:
    bool importAsImage(const AHardwareBuffer_Desc& desc,
                       const VkAndroidHardwareBufferPropertiesANDROID& ahbProps, VkFormat format);
    void releaseImageState();

    VkImage m_image{VK_NULL_HANDLE};
    VkImageView m_imageView{VK_NULL_HANDLE};
    VkImageView m_planeImageViews[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
};

} // namespace lcevc_dec::pipeline_vulkan

#endif

#endif // VN_LCEVC_PIPELINE_VULKAN_BUFFER_VULKAN_AHB_IMAGE_H
