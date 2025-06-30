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

#include "picture_copy.h"
//
#include "picture.h"
//
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

// fastCopy is just an optimised memcpy (since memcpy is surprisingly unoptimized). Note that
// optimizations are compiler-specific. For example, this function has an 8-at-a-time loop, to
// encourage loop-unrolling, but some compilers may already be doing that or something similar.
uint8_t* fastCopy(uint8_t* dest, const uint8_t* src, size_t size)
{
    const auto* src64 = static_cast<const uint64_t*>(static_cast<const void*>(src));
    auto* dest64 = static_cast<uint64_t*>(static_cast<void*>(dest));

    // first the bulk of the copy
    size_t num = size / (sizeof(uint64_t) * 8);
    size_t offset = 0;
    for (size_t n = 0; n < num; ++n) {
        offset = n * 8;
        for (size_t i = 0; i < 8; i++) {
            dest64[offset] = src64[offset];
            offset++;
        }
    }

    // Now the remainder of the copy
    offset = num * (sizeof(uint64_t) * 8);
    num = size % (sizeof(uint64_t) * 8);
    for (size_t n = 0; n < num; n++) {
        dest[offset] = src[offset];
        offset++;
    }
    return &dest[offset];
}

void simpleCopyPlaneBuffer(const uint8_t* srcData, uint32_t srcStride, uint32_t srcByteWidth,
                           uint32_t srcHeight, uint32_t srcSize, uint8_t* destData, uint32_t destStride,
                           uint32_t destByteWidth, uint32_t destHeight, uint32_t destSize)
{
    if ((srcStride == destStride) && (srcByteWidth == destByteWidth)) {
        // Source and destination have the same widths AND strides, so copy all at once.
        const size_t size = std::min(srcSize, destSize);
        fastCopy(destData, srcData, size);
        return;
    }

    // Either width or stride is different, so copy 1 width at a time, and increment 1 stride
    // at a time. This protects us from (a) copying FROM source's past-the-end pixels into dest
    // (which would mean copying junk, or part of a different plane), or (b) copying from
    // source INTO dest's past-the-end pixels (which would copy the start of row 2 into row 1).
    const uint32_t width = std::min(srcByteWidth, destByteWidth);
    const uint32_t height = std::min(srcHeight, destHeight);
    for (size_t j = 0; j < height; ++j) {
        fastCopy(destData + (j * static_cast<size_t>(destStride)),
                 srcData + (j * static_cast<size_t>(srcStride)), width);
    }
}

void copyNV12ToI420Buffers(std::array<const uint8_t*, 2> srcBufs, std::array<uint32_t, 2> srcPlaneByteStrides,
                           std::array<uint32_t, 2> srcPlaneByteWidths, uint32_t srcYMemorySize,
                           std::array<uint8_t*, 3> destBufs, std::array<uint32_t, 3> destPlaneByteStrides,
                           std::array<uint32_t, 3> destPlaneByteWidths, uint32_t destYMemorySize,
                           uint32_t height)
{
    // Luma is a straight block copy.
    simpleCopyPlaneBuffer(srcBufs[0], srcPlaneByteStrides[0], srcPlaneByteWidths[0], height,
                          srcYMemorySize, destBufs[0], destPlaneByteStrides[0],
                          destPlaneByteWidths[0], height, destYMemorySize);

    // Chroma needs to be copied more carefully.
    const uint32_t chromaHeight = height / 2;
    const uint8_t* srcUV = srcBufs[1];
    uint8_t* destU = destBufs[1];
    uint8_t* destV = destBufs[2];
    const uint32_t destUWidth = destPlaneByteWidths[1];
    const uint32_t srcUVWidth = srcPlaneByteWidths[1];
    const uint32_t destUStride = destPlaneByteStrides[1];
    const uint32_t srcUVStride = srcPlaneByteStrides[1];
    if ((destUStride * 2 != srcUVStride) || (destUWidth * 2 != srcUVWidth)) {
        // need to copy 1 pixel at a time
        for (uint32_t i = 0; i < chromaHeight; ++i) {
            uint32_t srcIdx = 0;
            uint32_t destIdx = 0;
            do {
                destU[destIdx] = srcUV[srcIdx++];
                destV[destIdx] = srcUV[srcIdx++];
                destIdx += 1;
            } while ((destIdx < destUWidth) && (srcIdx < srcUVWidth));
            // Adjust the pointers by the appropriate strides, to get to the next row
            srcUV += srcUVStride;
            destU += destUStride;
            destV += destUStride;
        }
    } else {
        // This assumes src & dest are the same width, and that their width equals their stride.
        const uint32_t chromaWidth = srcPlaneByteWidths[0] / 2;
        const uint32_t chromaSize = chromaWidth * chromaHeight;
#ifdef PORT_NEON
        uint32_t i = chromaSize;
        uint8x16x2_t res[4];
        while (i > 64) {
            // uint8x16x2_t  vld2q_u8(__transfersize(32) uint8_t const * ptr); // VLD2.8 {d0, d2}, [r0]
            res[0] = vld2q_u8(srcUV);
            srcUV += 32;
            res[1] = vld2q_u8(srcUV);
            srcUV += 32;
            res[2] = vld2q_u8(srcUV);
            srcUV += 32;
            res[3] = vld2q_u8(srcUV);
            srcUV += 32;

            // void  vst1q_u8(__transfersize(16) uint8_t * ptr, uint8x16_t val); // VST1.8 {d0, d1}, [r0]
            vst1q_u8(destU, res[0].val[0]);
            destU += 16;
            vst1q_u8(destV, res[0].val[1]);
            destV += 16;
            vst1q_u8(destU, res[1].val[0]);
            destU += 16;
            vst1q_u8(destV, res[1].val[1]);
            destV += 16;
            vst1q_u8(destU, res[2].val[0]);
            destU += 16;
            vst1q_u8(destV, res[2].val[1]);
            destV += 16;
            vst1q_u8(destU, res[3].val[0]);
            destU += 16;
            vst1q_u8(destV, res[3].val[1]);
            destV += 16;

            i -= 64;
        }

        while (i > 16) {
            res[0] = vld2q_u8(srcUV);
            srcUV += 32;
            vst1q_u8(destU, res[0].val[0]);
            destU += 16;
            vst1q_u8(destV, res[0].val[1]);
            destV += 16;
            i -= 16;
        }

        while (i > 0) {
            *destU++ = *srcUV++;
            *destV++ = *srcUV++;
            i--;
        }
#else
        // just a simple copy
        for (size_t i = 0; i < chromaSize; ++i) {
            *destU++ = *srcUV++;
            *destV++ = *srcUV++;
        }
#endif
    }
}

void copyNV12ToI420Picture(const Picture& src, Picture& dest)
{
    const uint32_t height = std::min(src.getHeight(), dest.getHeight());
    const std::array<const uint8_t*, 2> srcBufs = {
        src.getPlaneFirstSample(0),
        src.getPlaneFirstSample(1),
    };
    const std::array<uint8_t*, 3> destBufs = {
        dest.getPlaneFirstSample(0),
        dest.getPlaneFirstSample(1),
        dest.getPlaneFirstSample(2),
    };
    const uint32_t srcYMemorySize = src.getPlaneMemorySize(0);
    const uint32_t destYMemorySize = dest.getPlaneMemorySize(0);
    const std::array<uint32_t, 2> srcPlaneByteStrides = {src.getPlaneByteStride(0),
                                                         src.getPlaneByteStride(1)};
    const std::array<uint32_t, 3> destPlaneByteStrides = {
        dest.getPlaneByteStride(0), dest.getPlaneByteStride(1), dest.getPlaneByteStride(2)};
    const std::array<uint32_t, 2> srcPlaneByteWidths = {src.getPlaneWidthBytes(0),
                                                        src.getPlaneWidthBytes(1)};
    const std::array<uint32_t, 3> destPlaneByteWidths = {
        dest.getPlaneWidthBytes(0), dest.getPlaneWidthBytes(1), dest.getPlaneWidthBytes(2)};

    copyNV12ToI420Buffers(srcBufs, srcPlaneByteStrides, srcPlaneByteWidths, srcYMemorySize, destBufs,
                          destPlaneByteStrides, destPlaneByteWidths, destYMemorySize, height);
}

void copyPictureToPicture(const Picture& src, Picture& dest)
{
    // We copy 1 plane at a time, in case the images have matching formats but mismatching layouts.
    const uint32_t numPlanes = std::min(src.getNumPlanes(), dest.getNumPlanes());
    for (uint32_t i = 0; i < numPlanes; ++i) {
        simpleCopyPlaneBuffer(src.getPlaneFirstSample(i), src.getPlaneByteStride(i),
                              src.getPlaneWidthBytes(i), src.getPlaneHeight(i),
                              src.getPlaneMemorySize(i), dest.getPlaneFirstSample(i),
                              dest.getPlaneByteStride(i), dest.getPlaneWidthBytes(i),
                              dest.getPlaneHeight(i), dest.getPlaneMemorySize(i));
    }
}

} // namespace lcevc_dec::decoder
