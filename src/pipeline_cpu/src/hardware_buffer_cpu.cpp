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
#include <vector>

#if VN_OS(ANDROID)

// AHardwareBuffer_lockPlanes is API 29; weak_api29.h keeps it a weak symbol
// so the library still loads on older devices (checked before use below).
#include "weak_api29.h"

#include <android/hardware_buffer.h>

namespace lcevc_dec::pipeline_cpu {

BufferAHardware::~BufferAHardware() = default;

bool BufferAHardware::map(LdpBufferMapping* mapping, int32_t offset, uint32_t size, LdpAccess access)
{
    // Best effort: luma base only. Use the picture's plane descriptors for
    // plane-accurate access.
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

    // Request read as well as write: bufferAHardwareDestroy() reads this
    // mapping back to swap chroma order before unlocking.
    AHardwareBuffer_Planes planes{};
    if (AHardwareBuffer_lockPlanes(hardwareBuffer,
                                   AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN | AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,
                                   -1, nullptr, &planes) != 0 ||
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
    // NV21 device layout (Cr first): swapped back to NV12 before unlock.
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

    // Swap Cb/Cr for NV21 devices via three bulk passes (read to scratch, swap,
    // write back) rather than in-place read-modify-write: interleaving reads
    // and writes to the same address on write-combined memory forces a flush
    // per byte pair, which is far slower than separate sequential passes.
    if (buffer->m_chromaSwap && buffer->m_chromaBase != nullptr) {
        const size_t rowBytes = buffer->m_chromaRowBytes;
        const size_t rowStride = buffer->m_chromaRowStride;
        const size_t planeSize = rowStride * buffer->m_chromaRows;
        std::vector<uint8_t> scratch(planeSize);
        for (uint32_t row = 0; row < buffer->m_chromaRows; ++row) {
            std::memcpy(scratch.data() + row * rowStride, buffer->m_chromaBase + row * rowStride, rowBytes);
        }
        for (uint32_t row = 0; row < buffer->m_chromaRows; ++row) {
            uint8_t* rowPtr = scratch.data() + row * rowStride;
            for (uint32_t x = 0; x + 1 < rowBytes; x += 2) {
                std::swap(rowPtr[x], rowPtr[x + 1]);
            }
        }
        for (uint32_t row = 0; row < buffer->m_chromaRows; ++row) {
            std::memcpy(buffer->m_chromaBase + row * rowStride, scratch.data() + row * rowStride, rowBytes);
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
