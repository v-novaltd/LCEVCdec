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

#include "buffer_vulkan_ahb.h"

#if VN_OS(ANDROID)

#include "backend_vulkan.h"
#include "buffer_vulkan_ahb_buffer.h"
#include "buffer_vulkan_ahb_image.h"

#include <dlfcn.h>
#include <LCEVC/common/log.h>
#include <LCEVC/pipeline/buffer_base.h>
#include <LCEVC/pipeline/picture_layout.h>

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

BufferVulkanAhb::BufferVulkanAhb(BackendVulkan& backend, AHardwareBuffer* hardwareBuffer)
    : m_backend(backend)
    , m_hardwareBuffer(hardwareBuffer)
{
    if (m_hardwareBuffer != nullptr) {
        AHardwareBuffer_acquire(m_hardwareBuffer);
    }
}

BufferVulkanAhb::~BufferVulkanAhb()
{
    // Subclasses are responsible for tearing down their own VkBuffer/VkImage
    // (and any image views) before the base destructor frees the device
    // memory and releases the AHardwareBuffer reference.
    VkDevice device = m_backend.getDevice();
    if (m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    if (m_hardwareBuffer != nullptr) {
        AHardwareBuffer_release(m_hardwareBuffer);
        m_hardwareBuffer = nullptr;
    }
}

void BufferVulkanAhb::populateLayout(const AHardwareBuffer_Desc& desc)
{
    LdpColorFormat format = LdpColorFormatUnknown;
    switch (desc.format) {
        case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM: format = LdpColorFormatRGBA_8; break;
        case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM: format = LdpColorFormatRGBA_10_2_LE; break;
        case AHARDWAREBUFFER_FORMAT_YCbCr_P010: format = LdpColorFormatP010_10; break;
        case AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420: {
            // This is Android's flexible 8-bit 4:2:0 format.  The real memory
            // layout is signalled by lockPlanes(): it may be planar I420, NV12,
            // or NV21.  Pick the decoder layout that matches the actual chroma
            // pixel stride and Cb/Cr pointer order.
            AHardwareBuffer_Planes planes{};
            if (lockHardwareBufferPlanes(m_hardwareBuffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
                                         &planes) == 0 &&
                planes.planeCount >= 3) {
                const auto& cb = planes.planes[1];
                const auto& cr = planes.planes[2];
                if (cb.pixelStride == 1 && cr.pixelStride == 1) {
                    format = LdpColorFormatI420_8;
                } else if (cb.pixelStride == 2 && cr.pixelStride == 2) {
                    const auto* cbp = static_cast<const uint8_t*>(cb.data);
                    const auto* crp = static_cast<const uint8_t*>(cr.data);
                    if (crp == cbp + 1) {
                        format = LdpColorFormatNV12_8;
                    } else if (cbp == crp + 1) {
                        format = LdpColorFormatNV21_8;
                    }
                }
                VNLogDebug("BufferVulkanAhb: Y8Cb8Cr8_420 planes=%u Y[rs=%u ps=%u] "
                           "Cb[rs=%u ps=%u] Cr[rs=%u ps=%u] -> format %d",
                           planes.planeCount, planes.planes[0].rowStride,
                           planes.planes[0].pixelStride, cb.rowStride, cb.pixelStride, cr.rowStride,
                           cr.pixelStride, static_cast<int>(format));
                AHardwareBuffer_unlock(m_hardwareBuffer, nullptr);
            }
            if (format == LdpColorFormatUnknown) {
                VNLogWarning(
                    "BufferVulkanAhb: could not detect Y8Cb8Cr8_420 layout; assuming I420");
                format = LdpColorFormatI420_8;
            }
            break;
        }
        default:
            // No picture-layout mapping for this AHB format (incl. BLOB).
            m_hasLayout = false;
            return;
    }

    ldpPictureLayoutInitialize(&m_layout, format, desc.width, desc.height, kVulkanBufferRowAlignment);
    m_hasLayout = true;
}

bool BufferVulkanAhb::resize(uint32_t /*size*/)
{
    VNLogError("BufferVulkanAhb: cannot resize an externally-imported buffer");
    return false;
}

bool BufferVulkanAhb::map(LdpBufferMapping* /*mapping*/, int32_t /*offset*/, uint32_t /*size*/,
                          LdpAccess /*access*/)
{
    VNLogError("BufferVulkanAhb: host map not supported; use AHardwareBuffer_lock");
    return false;
}

void BufferVulkanAhb::unmap(const LdpBufferMapping* /*mapping*/)
{
    VNLogError("BufferVulkanAhb: host unmap not supported; use AHardwareBuffer_unlock");
}

void BufferVulkanAhb::clear()
{
    VNLogError("BufferVulkanAhb: host clear not supported; use clearCmd on a command buffer");
}

void BufferVulkanAhb::copyIn(uint32_t /*dstOffset*/, const void* /*src*/, uint32_t /*size*/)
{
    VNLogError("BufferVulkanAhb: host copyIn not supported");
}

void BufferVulkanAhb::copyOut(void* /*dst*/, uint32_t /*srcOffset*/, uint32_t /*size*/)
{
    VNLogError("BufferVulkanAhb: host copyOut not supported");
}

std::unique_ptr<BufferVulkanAhb> BufferVulkanAhb::create(BackendVulkan& backend, AHardwareBuffer* hardwareBuffer)
{
    if (hardwareBuffer == nullptr) {
        VNLogError("BufferVulkanAhb::create: null AHardwareBuffer");
        return nullptr;
    }

    VkDevice device = backend.getDevice();
    if (device == VK_NULL_HANDLE) {
        VNLogError("BufferVulkanAhb::create: no Vulkan device");
        return nullptr;
    }

    const auto getAhbProps = reinterpret_cast<PFN_vkGetAndroidHardwareBufferPropertiesANDROID>(
        vkGetDeviceProcAddr(device, "vkGetAndroidHardwareBufferPropertiesANDROID"));
    if (getAhbProps == nullptr) {
        VNLogError("BufferVulkanAhb::create: VK_ANDROID_external_memory_android_hardware_buffer "
                   "not enabled");
        return nullptr;
    }

    AHardwareBuffer_Desc desc{};
    AHardwareBuffer_describe(hardwareBuffer, &desc);

    // Ask Vulkan for the AHB's memory requirements; for image-format AHBs we
    // also chain in VkAndroidHardwareBufferFormatPropertiesANDROID so we can
    // pick the right VkFormat for the imported VkImage.
    VkAndroidHardwareBufferFormatPropertiesANDROID formatProps{};
    formatProps.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;

    VkAndroidHardwareBufferPropertiesANDROID ahbProps{};
    ahbProps.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
    if (desc.format != AHARDWAREBUFFER_FORMAT_BLOB) {
        ahbProps.pNext = &formatProps;
    }
    if (getAhbProps(device, hardwareBuffer, &ahbProps) != VK_SUCCESS) {
        VNLogError("BufferVulkanAhb::create: vkGetAndroidHardwareBufferPropertiesANDROID failed");
        return nullptr;
    }

    auto makeBuffer = [&](VkDeviceSize size) -> std::unique_ptr<BufferVulkanAhb> {
        auto wrapper =
            std::make_unique<BufferVulkanAhbBuffer>(backend, hardwareBuffer, desc, ahbProps, size);
        if (!wrapper->isImported()) {
            return nullptr;
        }
        return wrapper;
    };
    auto makeImage = [&](VkFormat format) -> std::unique_ptr<BufferVulkanAhb> {
        auto wrapper =
            std::make_unique<BufferVulkanAhbImage>(backend, hardwareBuffer, desc, ahbProps, format);
        if (!wrapper->isImported()) {
            return nullptr;
        }
        return wrapper;
    };

    if (desc.format == AHARDWAREBUFFER_FORMAT_BLOB) {
        // For BLOB format the byte size is `width` (height is required to be 1).
        return makeBuffer(desc.width);
    }

    // For YUV AHBs prefer the dedicated-image import path (required by
    // VUID-VkMemoryAllocateInfo-pNext-02384: importing a non-BLOB AHB demands a
    // VkMemoryDedicatedAllocateInfo with a non-null image).  We support this
    // for two-plane YUV formats whose imported VkFormat exposes per-plane
    // STORAGE on R8/R8G8 / R16/R16G16 views.  For everything else (3-plane
    // I420, NV21, external-format YUVs) we fall back to importing the raw
    // allocation as a VkBuffer; that violates VUID-02384 but is functional on
    // Android drivers and was the historical behaviour.
    const bool isYuv = desc.format == AHARDWAREBUFFER_FORMAT_YCbCr_P010 ||
                       desc.format == AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420;
    if (isYuv) {
        const bool imageCandidate =
            formatProps.format == VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 ||
            formatProps.format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
        if (imageCandidate) {
            if (auto image = makeImage(formatProps.format)) {
                return image;
            }
        }
        if (formatProps.format == VK_FORMAT_UNDEFINED) {
            VNLogWarning("BufferVulkanAhb::create: YUV AHB (format 0x%x) has no importable "
                         "VkFormat on this driver",
                         desc.format);
            return nullptr;
        }
        VNLogDebug("BufferVulkanAhb::create: importing YUV AHB as VkBuffer (format 0x%x, "
                   "VkFormat %d); VUID-VkMemoryAllocateInfo-pNext-02384 will fire under validation",
                   desc.format, static_cast<int>(formatProps.format));
        return makeBuffer(ahbProps.allocationSize);
    }

    return makeImage(formatProps.format);
}

} // namespace lcevc_dec::pipeline_vulkan

#endif
