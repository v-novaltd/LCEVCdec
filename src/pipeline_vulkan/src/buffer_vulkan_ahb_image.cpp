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

#include "buffer_vulkan_ahb_image.h"

#if VN_OS(ANDROID)

#include "backend_vulkan.h"

#include <LCEVC/common/log.h>

namespace lcevc_dec::pipeline_vulkan {

BufferVulkanAhbImage::BufferVulkanAhbImage(BackendVulkan& backend, AHardwareBuffer* hardwareBuffer,
                                           const AHardwareBuffer_Desc& desc,
                                           const VkAndroidHardwareBufferPropertiesANDROID& ahbProps,
                                           VkFormat format)
    : BufferVulkanAhb(backend, hardwareBuffer)
{
    if (m_hardwareBuffer == nullptr) {
        return;
    }

    populateLayout(desc);

    if (!importAsImage(desc, ahbProps, format)) {
        // Tear down anything we partially created so the base destructor doesn't
        // see stale handles.
        releaseImageState();
    }
}

BufferVulkanAhbImage::~BufferVulkanAhbImage() { releaseImageState(); }

void BufferVulkanAhbImage::releaseImageState()
{
    VkDevice device = m_backend.getDevice();

    for (VkImageView& planeView : m_planeImageViews) {
        if (planeView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, planeView, nullptr);
            planeView = VK_NULL_HANDLE;
        }
    }
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
}

bool BufferVulkanAhbImage::importAsImage(const AHardwareBuffer_Desc& desc,
                                         const VkAndroidHardwareBufferPropertiesANDROID& ahbProps,
                                         VkFormat format)
{
    VkDevice device = m_backend.getDevice();

    if (format == VK_FORMAT_UNDEFINED) {
        // Formats with no equivalent VkFormat (e.g. some YUV layouts) require
        // an "external format" image plus a sampler ycbcr conversion. That
        // path isn't implemented here yet.
        VNLogError("BufferVulkanAhbImage: AHB format 0x%x has no VkFormat equivalent "
                   "(external-format images are not supported)",
                   desc.format);
        return false;
    }

    VkExternalMemoryImageCreateInfo extInfo{};
    extInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    extInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = &extInfo;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = {desc.width, desc.height, 1};
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = desc.layers > 0 ? desc.layers : 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageCreateInfo, nullptr, &m_image) != VK_SUCCESS) {
        VNLogError("BufferVulkanAhbImage: vkCreateImage failed");
        m_image = VK_NULL_HANDLE;
        return false;
    }

    VkImportAndroidHardwareBufferInfoANDROID importInfo{};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    importInfo.buffer = m_hardwareBuffer;

    VkMemoryDedicatedAllocateInfo dedicatedInfo{};
    dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedInfo.image = m_image;
    dedicatedInfo.pNext = &importInfo;

    const uint32_t memoryTypeIndex =
        m_backend.findMemoryType(ahbProps.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &dedicatedInfo;
    allocInfo.allocationSize = ahbProps.allocationSize;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
        VNLogError("BufferVulkanAhbImage: vkAllocateMemory (image) failed");
        return false;
    }

    if (vkBindImageMemory(device, m_image, m_memory, 0) != VK_SUCCESS) {
        VNLogError("BufferVulkanAhbImage: vkBindImageMemory failed");
        return false;
    }

    const bool twoPlaneYuv = format == VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 ||
                             format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
    if (twoPlaneYuv) {
        const VkFormat lumaViewFormat =
            (format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM) ? VK_FORMAT_R8_UNORM : VK_FORMAT_R16_UNORM;
        const VkFormat chromaViewFormat = (format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM)
                                              ? VK_FORMAT_R8G8_UNORM
                                              : VK_FORMAT_R16G16_UNORM;

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        viewInfo.format = lumaViewFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT;
        if (vkCreateImageView(device, &viewInfo, nullptr, &m_planeImageViews[0]) != VK_SUCCESS) {
            VNLogError("BufferVulkanAhbImage: vkCreateImageView failed for luma plane");
            return false;
        }

        viewInfo.format = chromaViewFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
        if (vkCreateImageView(device, &viewInfo, nullptr, &m_planeImageViews[1]) != VK_SUCCESS) {
            VNLogError("BufferVulkanAhbImage: vkCreateImageView failed for chroma plane");
            return false;
        }
    } else {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
            VNLogError("BufferVulkanAhbImage: vkCreateImageView failed");
            return false;
        }
    }

    // Image-flavoured imports don't have a meaningful linear byte size for the
    // pipeline's BufferBase API; report the GPU allocation size for diagnostics.
    m_size = static_cast<uint32_t>(ahbProps.allocationSize);
    return true;
}

VkImageView BufferVulkanAhbImage::getVkImageView(uint32_t plane) const
{
    if (plane < 2 && m_planeImageViews[plane] != VK_NULL_HANDLE) {
        return m_planeImageViews[plane];
    }
    return m_imageView;
}

void BufferVulkanAhbImage::clearCmd(VkCommandBuffer /*commandBuffer*/)
{
    // Image-flavoured imports have no underlying VkBuffer to fill; clearing
    // the image must go through a layout-transition + clear-color path and
    // is not used by the pipeline today.
}

} // namespace lcevc_dec::pipeline_vulkan

#endif
