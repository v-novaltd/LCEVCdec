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

#include "picture_vulkan.h"

#include "from_base.h"
#include "picture_lock_vulkan.h"
#include "pipeline_vulkan.h"

#include <LCEVC/common/log.h>
#include <LCEVC/pipeline/types.h>

#if VN_OS(ANDROID)
#include "buffer_vulkan_ahb.h"
#endif

namespace lcevc_dec::pipeline_vulkan {

PictureVulkan::PictureVulkan(PipelineVulkan& pipeline, LdcMemoryAllocator* allocator)
    : PictureBase(pipeline)
    , m_allocator(allocator ? allocator : pipeline.staticAllocator())
{}

PictureVulkan::~PictureVulkan()
{
#if VN_OS(ANDROID)
    if (m_ahbBuffer) {
        buffer = nullptr;
        m_hasExternalBuffer = false;
        m_ahbBuffer.reset();
    }
#endif
}

bool PictureVulkan::lock(LdpAccess access, LdpPictureLock*& lockOut)
{
    if (isLocked()) {
        return false;
    }

    if (access != LdpAccessRead && access != LdpAccessModify && access != LdpAccessWrite) {
        return false;
    }

    if (buffer == nullptr && !m_hasExternalPlanes) {
        // Unbound - make buffer with usage according to first access
        // This is a little bit sketchy - infers how picture is used from it's
        // first access, but in practice it seems sufficient.
        switch (access) {
            case LdpAccessRead: bindBuffer(pipeline::BufferUsageIn); break;
            case LdpAccessWrite: bindBuffer(pipeline::BufferUsageOut); break;
            default: return false;
        }
        if (buffer == nullptr) {
            // Bind failed
            return false;
        }
    }

    VNAllocate(m_allocator, &m_lockAllocation, PictureLockVulkan, "PictureVulkan_Lock");
    PictureLockVulkan* pictureLock = VNAllocationPtr(m_lockAllocation, PictureLockVulkan);
    m_pictureLock = pictureLock;

    lockOut = new (pictureLock) PictureLockVulkan(this, access); // NOLINT(cppcoreguidelines-owning-memory)

    return true;
}

bool PictureVulkan::unlock(const LdpPictureLock* lock)
{
    if (!isLocked()) {
        return false;
    }

    if (lock != VNAllocationPtr(m_lockAllocation, PictureLockVulkan)) {
        return false;
    }

    if (VNIsAllocated(m_lockAllocation)) {
        VNFree(m_allocator, &m_lockAllocation);
    }

    m_pictureLock = nullptr;

    return true;
}

void PictureVulkan::getPlaneDescInternal(uint32_t plane, LdpPicturePlaneDesc& desc) const
{
    assert(plane < kLdpPictureMaxNumPlanes);

    if (!m_hasExternalPlanes) {
        assert(buffer);
        const BufferVulkanVma* bufferVulkan = fromPipeline(buffer);
        if (!isLocked() || !bufferVulkan || !bufferVulkan->m_mapped) {
            VNLogError("Can't get PlaneDesc for unlocked picture.");
            assert(0);
            return;
        }

        desc.firstSample = bufferVulkan->m_ptr + ldpPictureLayoutPlaneOffset(&layout, plane);
        desc.rowByteStride = ldpPictureLayoutRowStride(&layout, plane);
    } else {
        desc = m_externalPlaneDescs[plane];
    }
}

#if VN_OS(ANDROID)
bool PictureVulkan::setAhbBuffer(PipelineVulkan& pipeline, AHardwareBuffer* ahb)
{
    if (!ahb) {
        VNLogError("PictureVulkan::setAhbBuffer: null AHardwareBuffer");
        return false;
    }

    auto ahbBuffer = BufferVulkanAhb::create(pipeline.backend(), ahb);
    if (!ahbBuffer || ahbBuffer->size() == 0) {
        VNLogError("PictureVulkan::setAhbBuffer: failed to import AHardwareBuffer");
        return false;
    }

    // If the AHB has an implicit picture layout, PictureBase::setExternalBuffer()
    // will make the picture desc use that layout.  Dimension mismatches are
    // warnings rather than failures; colour format mismatches are silently
    // overridden by the hardware buffer's implicit format.

    // Clean up any previous AHB buffer
    if (m_ahbBuffer) {
        buffer = nullptr;
        m_hasExternalBuffer = false;
        m_ahbBuffer.reset();
    }

    m_ahbBuffer = std::move(ahbBuffer);
    setExternalBuffer(m_ahbBuffer.get());
    return true;
}
#endif

} // namespace lcevc_dec::pipeline_vulkan
