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

#ifndef VN_LCEVC_PIPELINE_VULKAN_PICTURE_VULKAN_H
#define VN_LCEVC_PIPELINE_VULKAN_PICTURE_VULKAN_H

#include <LCEVC/build_config.h>
#include <LCEVC/pipeline/picture_base.h>

#if VN_OS(ANDROID)
#include <memory>
struct AHardwareBuffer;
#endif

namespace lcevc_dec::pipeline_vulkan {

#if VN_OS(ANDROID)
class BufferVulkanAhb;
#endif
class PipelineVulkan;

class PictureVulkan : public pipeline::PictureBase
{
public:
    PictureVulkan(PipelineVulkan& pipeline, LdcMemoryAllocator* allocator = nullptr);
    ~PictureVulkan();

    bool lock(LdpAccess access, LdpPictureLock*& lockOut) override;
    bool unlock(const LdpPictureLock* lock) override;

    void getPlaneDescInternal(uint32_t plane, LdpPicturePlaneDesc& desc) const override;

#if VN_OS(ANDROID)
    // Wrap an externally-supplied AHardwareBuffer and attach it as this
    // picture's backing buffer.  Returns false on failure.
    bool setAhbBuffer(PipelineVulkan& pipeline, AHardwareBuffer* ahb);
#endif

    VNNoCopyNoMove(PictureVulkan);

private:
    LdcMemoryAllocator* m_allocator{};

    // Allocation for any current lock
    LdcMemoryAllocation m_lockAllocation = {};

#if VN_OS(ANDROID)
    // Owned AHB wrapper (non-null when the picture wraps an AHardwareBuffer)
    std::unique_ptr<BufferVulkanAhb> m_ahbBuffer;
#endif
};

} // namespace lcevc_dec::pipeline_vulkan

#endif // VN_LCEVC_PIPELINE_VULKAN_PICTURE_VULKAN_H
