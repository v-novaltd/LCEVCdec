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

// Inlined methods for PictureLayout
//
#ifndef VN_LCEVC_UTILITY_PICTURE_LAYOUT_INL_H
#define VN_LCEVC_UTILITY_PICTURE_LAYOUT_INL_H

#include "picture_layout.h"

inline LCEVC_ColorFormat PictureLayout::format() const { return m_layoutInfo->format; }

inline uint32_t PictureLayout::width() const { return planeWidth(0); }

inline uint32_t PictureLayout::height() const { return planeHeight(0); }

inline uint32_t PictureLayout::planeWidth(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);

    return m_width >> m_layoutInfo->planeWidthShift[plane];
}

inline uint32_t PictureLayout::planeHeight(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);

    return m_height >> m_layoutInfo->planeHeightShift[plane];
}

inline bool PictureLayout::isValid() const
{
    return m_layoutInfo->format != LCEVC_ColorFormat_Unknown &&
           !(m_width & m_layoutInfo->validWidthMask || m_height & m_layoutInfo->validHeightMask);
}

inline uint8_t PictureLayout::planes() const { return m_layoutInfo->planes; }

inline uint32_t PictureLayout::size() const { return m_size; }

inline uint32_t PictureLayout::planeSize(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);
    return m_rowStrides[plane] * (planeHeight(plane) >> (m_layoutInfo->interleave[plane] - 1));
}

inline uint32_t PictureLayout::planeOffset(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);
    return m_planeOffsets[plane] + m_layoutInfo->offset[plane];
}

inline uint32_t PictureLayout::planeInterleave(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);
    return m_layoutInfo->interleave[plane];
}

inline uint32_t PictureLayout::rowOffset(uint32_t plane, uint32_t row) const
{
    assert(plane < m_layoutInfo->planes);
    assert(row < m_height);
    return m_planeOffsets[plane] + row * m_rowStrides[plane] + m_layoutInfo->offset[plane];
}

inline uint32_t PictureLayout::rowStride(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);
    return m_rowStrides[plane];
}

inline uint32_t PictureLayout::sampleStride(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);
    return sampleSize() * m_layoutInfo->interleave[plane];
}

inline uint32_t PictureLayout::rowSize(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);
    // Width of plane, rounded up to component units
    return sampleSize() * m_layoutInfo->interleave[plane] *
           (m_width >> m_layoutInfo->planeWidthShift[plane]);
}

inline uint8_t PictureLayout::sampleSize() const { return (m_layoutInfo->bits + 7) / 8; }

inline uint8_t PictureLayout::sampleBits() const { return m_layoutInfo->bits; }

inline PictureLayout::ColorSpace PictureLayout::colorSpace() const
{
    return m_layoutInfo->colorSpace;
}

inline bool PictureLayout::rowsAreContiguous(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);

    return rowSize(plane) == m_rowStrides[plane] && (m_layoutInfo->interleave[plane] == 0);
}

inline bool PictureLayout::samplesAreContiguous(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);
    return m_layoutInfo->interleave[plane] == 1;
}

#endif
