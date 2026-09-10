/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

#include "hardware_buffer_cpu.h"

#include <LCEVC/common/log.h>
#include <LCEVC/pipeline/picture_layout.h>

#include <algorithm>
#include <cstring>
#include <new>

#if VN_OS(ANDROID)

// AHardwareBuffer_lockPlanes is API 29: weak_api29.h keeps the reference weak
// regardless of the platform level this translation unit is compiled for, so
// the library still loads on older devices; the call site below checks the
// symbol is present before using it.
#include "weak_api29.h"

#include <android/hardware_buffer.h>

namespace lcevc_dec::pipeline_cpu {

BufferAHardware::~BufferAHardware() = default;

bool BufferAHardware::map(LdpBufferMapping* mapping, int32_t offset, uint32_t size, LdpAccess access)
{
    // Best effort: expose the luma plane base. Plane-accurate access should go
    // through the picture's plane descriptors.
    if (!mapping || offset < 0 || offset + size > m_size) {
        return false;
    }
    mapping->buffer = this;
    mapping->ptr = m_lumaBase + offset;
    mapping->offset = offset;
    mapping->size = size;
    mapping->access = access;
    return true;
}

void BufferAHardware::unmap(const LdpBufferMapping* mapping) { VNUnused(mapping); }

void BufferAHardware::copyIn(uint32_t dstOffset, const void* src, uint32_t size)
{
    VNUnused(dstOffset);
    VNUnused(src);
    VNUnused(size);
    VNLogError("BufferAHardware: copyIn is not supported");
}

void BufferAHardware::copyOut(void* dst, uint32_t srcOffset, uint32_t size)
{
    VNUnused(dst);
    VNUnused(srcOffset);
    VNUnused(size);
    VNLogError("BufferAHardware: copyOut is not supported");
}

BufferAHardware* bufferAHardwareCreate(LdcMemoryAllocator* allocator, AHardwareBuffer* hardwareBuffer,
                                       const LdpPictureDesc& desc, LdpPicturePlaneDesc planesOut[])
{
    if (hardwareBuffer == nullptr) {
        return nullptr;
    }

    // AHardwareBuffer_lockPlanes requires API 29
    if (AHardwareBuffer_lockPlanes == nullptr) {
        VNLogDebug("bufferAHardwareCreate: AHardwareBuffer_lockPlanes requires API 29");
        return nullptr;
    }

    // Only 8-bit NV12 output is currently supported for direct decode
    if (desc.colorFormat != LdpColorFormatNV12_8) {
        VNLogWarning("bufferAHardwareCreate: unsupported picture format %d - only NV12_8 is "
                     "supported for direct decode",
                     static_cast<int>(desc.colorFormat));
        return nullptr;
    }

    AHardwareBuffer_Desc bufferDesc{};
    AHardwareBuffer_describe(hardwareBuffer, &bufferDesc);
    if (bufferDesc.format != AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420 || bufferDesc.width < desc.width ||
        bufferDesc.height < desc.height) {
        VNLogWarning("bufferAHardwareCreate: buffer %ux%u format 0x%x cannot back a %ux%u NV12 "
                     "picture",
                     bufferDesc.width, bufferDesc.height, bufferDesc.format, desc.width, desc.height);
        return nullptr;
    }

    AHardwareBuffer_acquire(hardwareBuffer);

    // Write-only lock (matching the buffer's allocation usage): the NV21 chroma
    // swap before unlock reads back through this mapping, which is fine for the
    // linear CPU-writable layouts this path accepts.
    AHardwareBuffer_Planes planes{};
    if (AHardwareBuffer_lockPlanes(hardwareBuffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1,
                                   nullptr, &planes) != 0 ||
        planes.planeCount < 3) {
        VNLogWarning("bufferAHardwareCreate: AHardwareBuffer_lockPlanes failed (planes=%u)",
                     planes.planeCount);
        AHardwareBuffer_release(hardwareBuffer);
        return nullptr;
    }

    const AHardwareBuffer_Plane& luma = planes.planes[0];
    const AHardwareBuffer_Plane& cb = planes.planes[1];
    const AHardwareBuffer_Plane& cr = planes.planes[2];

    // The decoder writes interleaved NV12 chroma, so the device layout must be
    // semi-planar with Cb and Cr adjacent.
    const auto* cbPtr = static_cast<const uint8_t*>(cb.data);
    const auto* crPtr = static_cast<const uint8_t*>(cr.data);
    const bool semiPlanar =
        cb.pixelStride == 2 && cr.pixelStride == 2 && (crPtr == cbPtr + 1 || cbPtr == crPtr + 1);
    if (!semiPlanar) {
        VNLogWarning("bufferAHardwareCreate: buffer chroma layout is not semi-planar "
                     "(pixelStride=%u/%u) - falling back to copied render",
                     cb.pixelStride, cr.pixelStride);
        AHardwareBuffer_unlock(hardwareBuffer, nullptr);
        AHardwareBuffer_release(hardwareBuffer);
        return nullptr;
    }
    // NV21 device layout (Cr first): the decoder writes NV12 (Cb first), so the
    // chroma pairs are swapped in place before the buffer is unlocked.
    const bool chromaSwap = (cbPtr == crPtr + 1);

    planesOut[0] = {static_cast<uint8_t*>(luma.data), luma.rowStride};
    planesOut[1] = {const_cast<uint8_t*>(std::min(cbPtr, crPtr)), cb.rowStride};

    auto* lumaBase = static_cast<uint8_t*>(luma.data);
    const uint32_t size = luma.rowStride * desc.height + cb.rowStride * (desc.height / 2);

    LdcMemoryAllocation allocation{};
    VNAllocate(allocator, &allocation, BufferAHardware, "BufferAHardware");
    auto* buffer = new (VNAllocationPtr(allocation, BufferAHardware))
        BufferAHardware(hardwareBuffer, lumaBase, size); // NOLINT(cppcoreguidelines-owning-memory)
    buffer->m_allocation = allocation;
    buffer->m_chromaBase = planesOut[1].firstSample;
    buffer->m_chromaRowStride = cb.rowStride;
    buffer->m_chromaRowBytes = desc.width; // interleaved pairs: width samples = width bytes
    buffer->m_chromaRows = desc.height / 2;
    buffer->m_chromaSwap = chromaSwap;

    VNLogDebug("bufferAHardwareCreate: locked %ux%u buffer (lumaStride=%u chromaStride=%u%s)",
               bufferDesc.width, bufferDesc.height, luma.rowStride, cb.rowStride,
               chromaSwap ? " NV21" : "");
    return buffer;
}

void bufferAHardwareDestroy(LdcMemoryAllocator* allocator, BufferAHardware* buffer)
{
    if (buffer == nullptr) {
        return;
    }

    // NV21-layout buffer: the decoder wrote NV12 chroma pairs, so swap Cb/Cr in
    // place while the buffer is still CPU-locked.
    if (buffer->m_chromaSwap && buffer->m_chromaBase != nullptr) {
        for (uint32_t row = 0; row < buffer->m_chromaRows; ++row) {
            uint8_t* rowPtr = buffer->m_chromaBase + static_cast<size_t>(row) * buffer->m_chromaRowStride;
            for (uint32_t x = 0; x + 1 < buffer->m_chromaRowBytes; x += 2) {
                std::swap(rowPtr[x], rowPtr[x + 1]);
            }
        }
    }

    AHardwareBuffer* hardwareBuffer = buffer->m_hardwareBuffer;
    LdcMemoryAllocation allocation = buffer->m_allocation;

    buffer->~BufferAHardware();
    VNFree(allocator, &allocation);

    if (hardwareBuffer != nullptr) {
        AHardwareBuffer_unlock(hardwareBuffer, nullptr);
        AHardwareBuffer_release(hardwareBuffer);
    }
}

} // namespace lcevc_dec::pipeline_cpu

#else // !VN_OS(ANDROID)

namespace lcevc_dec::pipeline_cpu {

BufferAHardware::~BufferAHardware() = default;

bool BufferAHardware::map(LdpBufferMapping* mapping, int32_t offset, uint32_t size, LdpAccess access)
{
    VNUnused(mapping);
    VNUnused(offset);
    VNUnused(size);
    VNUnused(access);
    // Reference the Android-only fields so they aren't flagged as unused here
    VNUnused(m_hardwareBuffer);
    VNUnused(m_lumaBase);
    VNUnused(m_allocation);
    VNUnused(m_chromaBase);
    VNUnused(m_chromaRowStride);
    VNUnused(m_chromaRowBytes);
    VNUnused(m_chromaRows);
    VNUnused(m_chromaSwap);
    return false;
}

void BufferAHardware::unmap(const LdpBufferMapping* mapping) { VNUnused(mapping); }

void BufferAHardware::copyIn(uint32_t dstOffset, const void* src, uint32_t size)
{
    VNUnused(dstOffset);
    VNUnused(src);
    VNUnused(size);
}

void BufferAHardware::copyOut(void* dst, uint32_t srcOffset, uint32_t size)
{
    VNUnused(dst);
    VNUnused(srcOffset);
    VNUnused(size);
}

BufferAHardware* bufferAHardwareCreate(LdcMemoryAllocator* allocator, AHardwareBuffer* hardwareBuffer,
                                       const LdpPictureDesc& desc, LdpPicturePlaneDesc planesOut[])
{
    VNUnused(allocator);
    VNUnused(hardwareBuffer);
    VNUnused(desc);
    VNUnused(planesOut);
    return nullptr;
}

void bufferAHardwareDestroy(LdcMemoryAllocator* allocator, BufferAHardware* buffer)
{
    VNUnused(allocator);
    VNUnused(buffer);
}

} // namespace lcevc_dec::pipeline_cpu

#endif
