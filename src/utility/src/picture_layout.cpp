/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

// Functions for common Picture operations.
//
#include "LCEVC/utility/check.h"
#include "math_utils.h"

#if __MINGW32__ && (__cplusplus >= 201907L)
#include <fmt/core.h>
#endif
#include <LCEVC/utility/picture_layout.h>

#include <cassert>

namespace lcevc_dec::utility {

// Various constants per color format used to work out actual sizes, offsets & strides, and file names
const struct PictureLayout::Info PictureLayout::kPictureLayoutInfo[] = {
    // clang-format off
    //                      colorSpace
    //                      |          planes
    //                      |          |  validWidthMask
    //                      |          |  |  validHeightMask
    //                      |          |  |  |  planeWidthShift
    //                      |          |  |  |  |             planeHeightShift
    //                      |          |  |  |  |             |             alignment
    //                      |          |  |  |  |             |             |             interleave
    //                      |          |  |  |  |             |             |             |             offset
    //                      |          |  |  |  |             |             |             |             |             bits
    //                      |          |  |  |  |             |             |             |             |             |   suffix
    {LCEVC_I420_8,          YUV,       3, 1, 1, {0, 1, 1},    {0, 1, 1},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    8,  "_p420.yuv"},
    {LCEVC_I420_10_LE,      YUV,       3, 1, 1, {0, 1, 1},    {0, 1, 1},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    10, "_10bit_p420.yuv"},
    {LCEVC_I420_12_LE,      YUV,       3, 1, 1, {0, 1, 1},    {0, 1, 1},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    12, "_12bit_p420.yuv"},
    {LCEVC_I420_14_LE,      YUV,       3, 1, 1, {0, 1, 1},    {0, 1, 1},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    14, "_14bit_p420.yuv"},
    {LCEVC_I420_16_LE,      YUV,       3, 1, 1, {0, 1, 1},    {0, 1, 1},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    16, "_16bit_p420.yuv"},

    {LCEVC_I422_8,          YUV,       3, 1, 0, {0, 1, 1},    {0, 0, 0},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    8,  "_p422.yuv"},
    {LCEVC_I422_10_LE,      YUV,       3, 1, 0, {0, 1, 1},    {0, 0, 0},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    10, "_10bit_p422.yuv"},
    {LCEVC_I422_12_LE,      YUV,       3, 1, 0, {0, 1, 1},    {0, 0, 0},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    12, "_12bit_p422.yuv"},
    {LCEVC_I422_14_LE,      YUV,       3, 1, 0, {0, 1, 1},    {0, 0, 0},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    14, "_14bit_p422.yuv"},
    {LCEVC_I422_16_LE,      YUV,       3, 1, 0, {0, 1, 1},    {0, 0, 0},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    16, "_16bit_p422.yuv"},

    {LCEVC_I444_8,          YUV,       3, 0, 0, {0, 0, 0},    {0, 0, 0},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    8,  "_p444.yuv"},
    {LCEVC_I444_10_LE,      YUV,       3, 0, 0, {0, 0, 0},    {0, 0, 0},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    10, "_10bit_p444.yuv"},
    {LCEVC_I444_12_LE,      YUV,       3, 0, 0, {0, 0, 0},    {0, 0, 0},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    12, "_12bit_p444.yuv"},
    {LCEVC_I444_14_LE,      YUV,       3, 0, 0, {0, 0, 0},    {0, 0, 0},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    14, "_14bit_p444.yuv"},
    {LCEVC_I444_16_LE,      YUV,       3, 0, 0, {0, 0, 0},    {0, 0, 0},    {0, 0, 0},    {1, 1, 1},    {0, 0, 0},    16, "_16bit_p444.yuv"},

    {LCEVC_NV12_8,          YUV,       3, 1, 1, {0, 1, 1},    {0, 1, 1},    {0, 0, 0},    {1, 2, 2},    {0, 0, 1},    8,  ".nv12"},
    {LCEVC_NV21_8,          YUV,       3, 1, 1, {0, 1, 1},    {0, 1, 1},    {0, 0, 0},    {1, 2, 2},    {0, 1, 0},    8,  ".nv21"},
    
    {LCEVC_RGB_8,           RGB,       3, 0, 0, {0, 0, 0},    {0, 0, 0},    {0, 0, 0},    {3, 3, 3},    {0, 1, 2},    8,  ".rgb"},
    {LCEVC_BGR_8,           RGB,       3, 0, 0, {0, 0, 0},    {0, 0, 0},    {0, 0, 0},    {3, 3, 3},    {2, 1, 0},    8,  ".bgr"},
    {LCEVC_RGBA_8,          RGB,       4, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {4, 4, 4, 4}, {0, 1, 2, 3}, 8,  ".rgba"},
    {LCEVC_BGRA_8,          RGB,       4, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {4, 4, 4, 4}, {2, 1, 0, 3}, 8,  ".bgra"},
    {LCEVC_ARGB_8,          RGB,       4, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {4, 4, 4, 4}, {3, 0, 1, 2}, 8,  ".argb"},
    {LCEVC_ABGR_8,          RGB,       4, 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {4, 4, 4, 4}, {3, 2, 1, 0}, 8,  ".abgr"},
    
    {LCEVC_GRAY_8,          Greyscale, 1, 0, 0, {0},          {0},          {0},          {1},          {0},          8,  ".y"},
    {LCEVC_GRAY_10_LE,      Greyscale, 1, 0, 0, {0},          {0},          {0},          {1},          {0},          10, "_10bit.y"},
    {LCEVC_GRAY_12_LE,      Greyscale, 1, 0, 0, {0},          {0},          {0},          {1},          {0},          12, "_12bit.y"},
    {LCEVC_GRAY_14_LE,      Greyscale, 1, 0, 0, {0},          {0},          {0},          {1},          {0},          14, "_14bit.y"},
    {LCEVC_GRAY_16_LE,      Greyscale, 1, 0, 0, {0},          {0},          {0},          {1},          {0},          16, "_16bit.y"},
    // clang-format on
};

namespace {

    // Make a PictureDesc given the common parameters - format, width and height
    LCEVC_PictureDesc defaultPictureDesc(LCEVC_ColorFormat format, uint32_t width, uint32_t height)
    {
        LCEVC_PictureDesc desc = {0};
        LCEVC_DefaultPictureDesc(&desc, format, width, height);
        return desc;
    }

    LCEVC_PictureDesc getPictureDesc(LCEVC_DecoderHandle decoder, LCEVC_PictureHandle picture)
    {
        LCEVC_PictureDesc desc = {0};
        VN_LCEVC_CHECK(LCEVC_GetPictureDesc(decoder, picture, &desc));
        return desc;
    }
} // namespace

// LayoutInfo for unknown formats
const PictureLayout::Info PictureLayout::kPictureLayoutInfoUnknown{LCEVC_ColorFormat_Unknown};

// Find the layout info for a given format
const PictureLayout::Info& PictureLayout::findLayoutInfo(LCEVC_ColorFormat format)
{
    for (const auto& li : kPictureLayoutInfo) {
        if (li.format == format) {
            return li;
        }
    }

    return kPictureLayoutInfoUnknown;
}

uint8_t PictureLayout::getBitsPerSample(LCEVC_ColorFormat format)
{
    return findLayoutInfo(format).bits;
}

uint8_t PictureLayout::getPlaneWidthShift(LCEVC_ColorFormat format, uint32_t planeIdx)
{
    return findLayoutInfo(format).planeWidthShift[planeIdx];
}

uint8_t PictureLayout::getPlaneHeightShift(LCEVC_ColorFormat format, uint32_t planeIdx)
{
    return findLayoutInfo(format).planeHeightShift[planeIdx];
}

bool PictureLayout::checkValidStrides(const LCEVC_PictureDesc& pictureDesc,
                                      const uint32_t rowStrides[kMaxPlanes])
{
    auto layout = PictureLayout(pictureDesc);
    for (uint32_t plane = 0; plane < layout.planes(); plane++) {
        if (rowStrides[plane] < layout.defaultRowStride(plane)) {
            return false;
        }
    }
    return true;
}

bool PictureLayout::getPaddedStrides(const LCEVC_PictureDesc& pictureDesc, uint32_t rowStrides[kMaxPlanes])
{
    auto layout = PictureLayout(pictureDesc);
    for (uint32_t plane = 0; plane < layout.planes(); plane++) {
        rowStrides[plane] = nextPow2(layout.defaultRowStride(plane));
    }
    return true;
}

PictureLayout::PictureLayout()
    : m_layoutInfo(&kPictureLayoutInfoUnknown)
{}

PictureLayout::PictureLayout(const LCEVC_PictureDesc& pictureDesc)
    : PictureLayout(pictureDesc, findLayoutInfo(pictureDesc.colorFormat))
{}

PictureLayout::PictureLayout(const LCEVC_PictureDesc& pictureDesc, const uint32_t rowStrides[kMaxPlanes])
    : PictureLayout(pictureDesc, findLayoutInfo(pictureDesc.colorFormat), rowStrides)
{}

PictureLayout::PictureLayout(LCEVC_ColorFormat format, uint32_t width, uint32_t height)
    : PictureLayout(defaultPictureDesc(format, width, height))
{}

PictureLayout::PictureLayout(LCEVC_ColorFormat format, uint32_t width, uint32_t height,
                             const uint32_t rowStrides[kMaxPlanes])
    : PictureLayout(defaultPictureDesc(format, width, height), rowStrides)
{}

PictureLayout::PictureLayout(LCEVC_DecoderHandle decoderHandle, LCEVC_PictureHandle pictureHandle)
    : PictureLayout(getPictureDesc(decoderHandle, pictureHandle))
{}

PictureLayout::PictureLayout(const LCEVC_PictureDesc& pictureDesc, const Info& layoutInfo)
    : m_layoutInfo(&layoutInfo)
    , m_width(pictureDesc.width)
    , m_height(pictureDesc.height)
{
    // Figure out per plane strides
    for (uint32_t plane = 0; plane < m_layoutInfo->planes; ++plane) {
        m_rowStrides[plane] = defaultRowStride(plane);
    }

    generateOffsets();
};

PictureLayout::PictureLayout(const LCEVC_PictureDesc& pictureDesc, const Info& layoutInfo,
                             const uint32_t strides[kMaxPlanes])
    : m_layoutInfo(&layoutInfo)
    , m_width(pictureDesc.width)
    , m_height(pictureDesc.height)
{
    // Fill in supplied strides
    for (uint32_t plane = 0; plane < m_layoutInfo->planes; ++plane) {
        assert(strides[plane] >= defaultRowStride(plane));
        m_rowStrides[plane] = strides[plane];
    }

    generateOffsets();
}

// Fill in plane offsets based on stride and interleave
void PictureLayout::generateOffsets()
{
    // Work out per plane offsets
    uint32_t offset = 0;
    uint32_t interleave = 1;
    for (uint32_t plane = 0; plane < m_layoutInfo->planes; ++plane) {
        m_planeOffsets[plane] = offset;

        // Count across interleaved planes
        if (interleave > 1) {
            --interleave;
        } else {
            interleave = m_layoutInfo->interleave[plane];
        }

        // Accumulate offset after any interleaving
        if (interleave == 1) {
            offset += m_rowStrides[plane] * (m_height >> m_layoutInfo->planeHeightShift[plane]);
        }
    }

    // Store final offset as total size
    m_size = offset;
}

// Work out minimum stride from width
uint32_t PictureLayout::defaultRowStride(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);
    const uint32_t align = m_layoutInfo->alignment[plane];
    const uint32_t defaultStride = (rowSize(plane) + align) & ~align;

    // Default stride
    return defaultStride;
}

// Return true if the layouts are compatible
bool PictureLayout::isCompatible(const PictureLayout& other) const
{
    // Must be same dimension
    if (m_width != other.m_width || m_height != other.m_height) {
        return false;
    }

    // If it is exactly the same format - it is compatible
    if (m_layoutInfo->format == other.m_layoutInfo->format) {
        return true;
    }

    // Sample bit depths must match
    if (m_layoutInfo->bits != other.m_layoutInfo->bits) {
        return false;
    }

    // If it has same number of planes and they have the same shifts - call it good
    if (m_layoutInfo->planes != other.m_layoutInfo->planes) {
        return false;
    }

    for (uint32_t plane = 0; plane < m_layoutInfo->planes; ++plane) {
        if (m_layoutInfo->planeWidthShift[plane] != other.m_layoutInfo->planeWidthShift[plane] ||
            m_layoutInfo->planeHeightShift[plane] != other.m_layoutInfo->planeHeightShift[plane]) {
            return false;
        }
    }

    return true;
}

bool PictureLayout::isInterleaved() const
{
    for (auto i : m_layoutInfo->interleave) {
        if (i > 1) {
            return true;
        }
    }
    return false;
}

uint32_t PictureLayout::planeGroups() const
{
    uint32_t interleaveSum = 0;
    uint32_t plane = 1;
    for (auto i : m_layoutInfo->interleave) {
        interleaveSum += i;
        if (interleaveSum >= m_layoutInfo->planes) {
            return plane;
        }
        plane++;
    }
    return m_layoutInfo->planes;
}

// Construct a vooya/YUView style filename from base
std::string PictureLayout::makeRawFilename(std::string_view name) const
{
#if __MINGW32__ && (__cplusplus >= 201907L)
    return fmt::format("{}_{}x{}{}", name, width(), height(), m_layoutInfo->suffix);
#else
    std::string s = {name.begin(), name.end()};
    return std::string(s+"_"+std::to_string(width())+"x"+\
                       std::to_string(height())+m_layoutInfo->suffix);
#endif
}

} // namespace lcevc_dec::utility
