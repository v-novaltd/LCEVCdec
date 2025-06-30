/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

// Inlined methods for PictureLayout
//
#ifndef VN_LCEVC_API_UTILITY_PICTURE_LAYOUT_INL_H
#define VN_LCEVC_API_UTILITY_PICTURE_LAYOUT_INL_H

#include "picture_layout.h"

inline LCEVC_ColorFormat PictureLayout::format() const { return m_layoutInfo->format; }

inline uint32_t PictureLayout::width() const { return m_width; }

inline uint32_t PictureLayout::height() const { return m_height; }

inline uint32_t PictureLayout::planeWidth(uint32_t plane) const
{
    assert(plane < planes());

    return (m_width >> m_layoutInfo->planeWidthShift[plane]) * planeInterleave(plane);
}

inline uint32_t PictureLayout::planeHeight(uint32_t plane) const
{
    assert(plane < planes());

    return m_height >> m_layoutInfo->planeHeightShift[plane];
}

inline bool PictureLayout::isValid() const
{
    return m_layoutInfo->format != LCEVC_ColorFormat_Unknown &&
           !(m_width & m_layoutInfo->validWidthMask || m_height & m_layoutInfo->validHeightMask);
}

inline uint8_t PictureLayout::colorComponents() const { return m_layoutInfo->colorComponents; }

inline uint8_t PictureLayout::planes() const
{
    uint8_t total = 0;
    for (uint32_t comp = 0; comp < m_layoutInfo->colorComponents; comp += m_layoutInfo->interleave[comp]) {
        total++;
    }
    return total;
}

inline uint32_t PictureLayout::size() const { return m_size; }

inline uint32_t PictureLayout::planeSize(uint32_t plane) const
{
    assert(plane < planes());
    return m_rowStrides[plane] * planeHeight(plane);
}

inline uint32_t PictureLayout::planeOffset(uint32_t plane) const
{
    assert(plane < planes());
    return m_planeOffsets[plane];
}

inline uint32_t PictureLayout::componentOffset(uint8_t component) const
{
    assert(component < m_layoutInfo->colorComponents);
    return m_planeOffsets[getPlaneForComponent(component)] + m_layoutInfo->offset[component];
}

inline uint8_t PictureLayout::planeInterleave(uint8_t plane) const
{
    // Find a component which is in this plane
    assert(plane < planes());
    return m_layoutInfo->interleave[getComponentForPlane(plane)];
}

inline uint8_t PictureLayout::componentInterleave(uint8_t component) const
{
    assert(component < m_layoutInfo->colorComponents);
    return m_layoutInfo->interleave[component];
}

inline uint32_t PictureLayout::rowOffset(uint8_t plane, uint32_t row) const
{
    assert(plane < planes());
    assert(row < m_height);
    return m_planeOffsets[plane] + row * m_rowStrides[plane] +
           m_layoutInfo->offset[getComponentForPlane(plane)];
}

inline uint32_t PictureLayout::rowStride(uint32_t plane) const
{
    assert(plane < planes());
    return m_rowStrides[plane];
}

inline uint32_t PictureLayout::sampleStride(uint32_t plane) const
{
    assert(plane < planes());
    return sampleSize() * m_layoutInfo->interleave[plane];
}

inline uint32_t PictureLayout::rowSize(uint32_t plane) const
{
    assert(plane < planes());
    // Width of plane, rounded up to component units
    return sampleSize() * (m_width >> m_layoutInfo->planeWidthShift[plane]) * planeInterleave(plane);
}

inline uint8_t PictureLayout::sampleSize() const { return (m_layoutInfo->bits + 7) / 8; }

inline uint8_t PictureLayout::sampleBits() const { return m_layoutInfo->bits; }

inline PictureLayout::ColorSpace PictureLayout::colorSpace() const
{
    return m_layoutInfo->colorSpace;
}

inline bool PictureLayout::rowsAreContiguous(uint32_t plane) const
{
    assert(plane < planes());

    return rowSize(plane) == m_rowStrides[plane] && (m_layoutInfo->interleave[plane] == 0);
}

inline bool PictureLayout::samplesAreContiguous(uint32_t component) const
{
    assert(component < m_layoutInfo->colorComponents);
    return m_layoutInfo->interleave[component] == 1;
}

#endif // VN_LCEVC_API_UTILITY_PICTURE_LAYOUT_INL_H
