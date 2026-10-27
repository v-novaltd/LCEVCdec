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

#ifndef VN_LCEVC_PIPELINE_CPU_BUFFER_CPU_AHB_H
#define VN_LCEVC_PIPELINE_CPU_BUFFER_CPU_AHB_H

#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/pipeline/buffer_base.h>
#include <LCEVC/pipeline/types.h>

struct AHardwareBuffer;

namespace lcevc_dec::pipeline_cpu {

// BufferCPUAhb
//
// BufferBase wrapper around a CPU-locked AHardwareBuffer, backing a decode
// output picture so that the pipeline writes straight into the gralloc memory
// (Android only). Created by PipelineCPU::storeAccelBuffer(); the wrapper owns
// a reference on the AHardwareBuffer and the CPU lock, both dropped by
// destroy().
//
// The planes of the locked buffer are not necessarily contiguous, so flat
// buffer access is limited: map() exposes the luma plane base only, and
// resize/copy operations are unsupported. Picture data access goes through
// the picture's external plane descriptors instead.
class BufferCPUAhb : public pipeline::BufferBase
{
public:
    // Acquire and CPU-lock the AHardwareBuffer for writing, validating it against
    // the picture description (currently 8-bit NV12 only), and fill in the plane
    // descriptors pointing into the locked memory. Returns nullptr if the device
    // or buffer cannot support direct decode (caller should fall back to an
    // internally-allocated picture).
    static BufferCPUAhb* create(LdcMemoryAllocator* allocator, AHardwareBuffer* hardwareBuffer,
                                const LdpPictureDesc& desc, LdpPicturePlaneDesc planesOut[]);

    // Unlock and release the AHardwareBuffer and free the wrapper.
    static void destroy(LdcMemoryAllocator* allocator, BufferCPUAhb* buffer);

    ~BufferCPUAhb() override;

    uint32_t size() const override { return m_size; }
    pipeline::BufferUsage usage() const override { return pipeline::BufferUsageOut; }

    bool resize(uint32_t size) override { return false; }
    bool map(LdpBufferMapping* mapping, int32_t offset, uint32_t size, LdpAccess access) override;
    void unmap(const LdpBufferMapping* mapping) override;
    void clear() override {}
    void copyIn(uint32_t dstOffset, const void* src, uint32_t size) override;
    void copyOut(void* dst, uint32_t srcOffset, uint32_t size) override;

    VNNoCopyNoMove(BufferCPUAhb);

private:
    BufferCPUAhb(AHardwareBuffer* hardwareBuffer, uint8_t* lumaBase, uint32_t size)
        : m_hardwareBuffer(hardwareBuffer)
        , m_lumaBase(lumaBase)
        , m_size(size)
    {}

    AHardwareBuffer* m_hardwareBuffer{};
    uint8_t* m_lumaBase{};
    uint32_t m_size{};

    // Chroma plane of the locked buffer. When the device layout is NV21,
    // destroy() swaps the decoder's NV12 chroma pairs (via a scratch copy)
    // before unlocking.
    uint8_t* m_chromaBase{};
    uint32_t m_chromaRowStride{};
    uint32_t m_chromaRowBytes{};
    uint32_t m_chromaRows{};
    bool m_chromaSwap{false};

    // The allocation holding this object
    LdcMemoryAllocation m_allocation{};
};

} // namespace lcevc_dec::pipeline_cpu

#endif // VN_LCEVC_PIPELINE_CPU_BUFFER_CPU_AHB_H
