// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Inlined methods for PictureLayout
//
#ifndef VN_LCEVC_UTILITY_PICTURE_LAYOUT_INL_H
#define VN_LCEVC_UTILITY_PICTURE_LAYOUT_INL_H

#include <cassert>

inline LCEVC_ColorFormat PictureLayout::format() const { return m_layoutInfo->format; }

inline uint32_t PictureLayout::width(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);

    return m_width >> m_layoutInfo->planeWidthShift[plane];
}

inline uint32_t PictureLayout::height(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);

    return m_height >> m_layoutInfo->planeWidthShift[plane];
}

inline bool PictureLayout::isValid() const
{
    return m_layoutInfo->format != LCEVC_ColorFormat_Unknown &&
           !(m_width & m_layoutInfo->validWidthMask || m_height & m_layoutInfo->validHeightMask);
}

inline uint32_t PictureLayout::planes() const { return m_layoutInfo->planes; }

inline uint32_t PictureLayout::size() const { return m_size; }

inline uint32_t PictureLayout::planeSize(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);
    return m_rowStrides[plane] * height(plane);
}

inline uint32_t PictureLayout::planeOffset(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);
    return m_planeOffsets[plane] + m_layoutInfo->offset[plane];
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
    return  sampleSize()  * m_layoutInfo->interleave[plane];
}

inline uint32_t PictureLayout::rowSize(uint32_t plane) const
{
    assert(plane < m_layoutInfo->planes);
    // Width of plane, rounded up to component units
    return sampleSize() * m_layoutInfo->interleave[plane] *
           (m_width >> m_layoutInfo->planeWidthShift[plane]);
}

inline uint32_t PictureLayout::sampleSize() const { return (m_layoutInfo->bits+7)/8; }

inline uint32_t PictureLayout::sampleBits() const { return m_layoutInfo->bits; }

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
