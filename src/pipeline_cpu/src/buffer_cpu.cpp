/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#include "buffer_cpu.h"

#include "pipeline_cpu.h"

#include <LCEVC/common/memory.h>

namespace lcevc_dec::pipeline_cpu {

namespace {
    extern const LdpBufferFunctions kBufferCPUFunctions;
}

BufferCPU::BufferCPU(PipelineCPU& pipeline, uint32_t size)
    : LdpBuffer{&kBufferCPUFunctions}
    , m_pipeline(pipeline)
{
    // Allocate the bytes
    if (size > 0) {
        VNAllocateAlignedArray(m_pipeline.allocator(), &m_allocation, uint8_t, kBufferRowAlignment, size);
    }
}

BufferCPU::~BufferCPU()
{
    if (VNIsAllocated(m_allocation)) {
        VNFree(m_pipeline.allocator(), &m_allocation);
    }
}

bool BufferCPU::map(LdpBufferMapping* mapping, int32_t offset, uint32_t mapSize, LdpAccess access)
{
    // Is this mapped already?
    if (m_mapped) {
        return false;
    }

    // Does requested window fit in buffer?
    if (offset < 0 || (offset + mapSize) > size()) {
        return false;
    }

    // Record details
    mapping->buffer = this;
    mapping->offset = offset;
    mapping->size = mapSize;
    mapping->ptr = VNAllocationPtr(m_allocation, uint8_t) + offset;
    mapping->access = access;
    mapping->userData = (void*)this;

    m_mapped = true;
    return true;
}

void BufferCPU::unmap(const LdpBufferMapping* mapping)
{
    VNUnused(mapping);

    assert(mapping->userData == (void*)this);
    m_mapped = false;
}

void BufferCPU::clear() // NOLINT(readability-make-member-function-const)
{
    if (!VNIsAllocated(m_allocation)) {
        return;
    }

    memset(VNAllocationPtr(m_allocation, uint8_t), 0, VNAllocationSize(m_allocation, uint8_t));
}

uint8_t* BufferCPU::ptr() const { return VNAllocationPtr(m_allocation, uint8_t); }

uint32_t BufferCPU::size() const
{
    return static_cast<uint32_t>(VNAllocationSize(m_allocation, uint8_t));
}

bool BufferCPU::resize(uint32_t size)
{
    const uint8_t* ptr = VNReallocateArray(m_pipeline.allocator(), &m_allocation, uint8_t, size);
    return ptr != nullptr;
}

// C function table to connect to C++ class
//
namespace {
    bool map(struct LdpBuffer* buffer, LdpBufferMapping* mapping, int32_t offset, uint32_t size,
             LdpAccess access)
    {
        return static_cast<BufferCPU*>(buffer)->map(mapping, offset, size, access);
    }
    void unmap(struct LdpBuffer* buffer, const LdpBufferMapping* mapping)
    {
        static_cast<BufferCPU*>(buffer)->unmap(mapping);
    }

    const LdpBufferFunctions kBufferCPUFunctions = {
        map,
        unmap,
    };
} // namespace

} // namespace lcevc_dec::pipeline_cpu
