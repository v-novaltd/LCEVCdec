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

#include "picture_cpu.h"

#include "from_base.h"
#include "picture_lock_cpu.h"

namespace lcevc_dec::pipeline_cpu {

PictureCPU::PictureCPU(PipelineCPU& pipeline, LdcMemoryAllocator* allocator)
    : PictureBase(pipeline)
    , m_allocator(allocator ? allocator : pipeline.staticAllocator())
{}

PictureCPU::~PictureCPU() {}

bool PictureCPU::lock(LdpAccess access, LdpPictureLock*& lockOut)
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

    // Allocate lock object, and in-place construct
    VNAllocate(m_allocator, &m_lockAllocation, PictureLockCPU, "PictureCPU_Lock");
    PictureLockCPU* pictureLock = VNAllocationPtr(m_lockAllocation, PictureLockCPU);
    m_pictureLock = pictureLock;

    lockOut = new (pictureLock) PictureLockCPU(this, access); // NOLINT(cppcoreguidelines-owning-memory)

    return true;
}

bool PictureCPU::unlock(const LdpPictureLock* lock)
{
    if (!isLocked()) {
        return false;
    }

    if (lock != VNAllocationPtr(m_lockAllocation, PictureLockCPU)) {
        return false;
    }

    // Release the lock object
    if (VNIsAllocated(m_lockAllocation)) {
        VNFree(m_allocator, &m_lockAllocation);
    }

    m_pictureLock = nullptr;

    return true;
}

void PictureCPU::getPlaneDescInternal(uint32_t plane, LdpPicturePlaneDesc& desc) const
{
    assert(plane < kLdpPictureMaxNumPlanes);

    if (!m_hasExternalPlanes) {
        assert(buffer);
        const BufferCPU* bufferCPU = fromPipeline(buffer);
        desc.firstSample = VNAllocationPtr(bufferCPU->m_allocation, uint8_t) +
                           ldpPictureLayoutPlaneOffset(&layout, plane);
        desc.rowByteStride = ldpPictureLayoutRowStride(&layout, plane);
    } else {
        desc = m_externalPlaneDescs[plane];
    }
}

} // namespace lcevc_dec::pipeline_cpu
