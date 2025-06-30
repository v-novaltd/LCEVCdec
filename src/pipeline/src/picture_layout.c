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

// Functions for common Picture operations.
//
#include <assert.h>
#include <LCEVC/common/check.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/pipeline/picture_layout.h>
#include <LCEVC/pipeline/types.h>
#include <stdint.h>

// clang-format off
#define VN_PICTURE_LAYOUTS(format, prefix)                                                                                                                                            \
    /* Various constants per color format used to work out actual sizes, offsets & strides, and file names */                                                                         \
    static const LdpPictureLayoutInfo k##format##PictureLayoutInfo[] = {                                                                                                              \
/*                                     colorSpace
                                       |                       colorComponents
                                       |                       |  validWidthMask
                                       |                       |  |  validHeightMask
                                       |                       |  |  |  planeWidthShift
                                       |                       |  |  |  |          planeHeightShift
                                       |                       |  |  |  |          |          alignment
                                       |                       |  |  |  |          |          |          interleave
                                       |                       |  |  |  |          |          |          |             offset
                                       |                       |  |  |  |          |          |          |             |             bits
                                       |                       |  |  |  |          |          |          |             |             |         fixedPoint
                                       |                       |  |  |  |          |          |          |             |             |         |                  suffix */           \
        {LdpColorFormatI420_8,         LdpColorSpaceYUV,       3, 1, 1, {0, 1, 1}, {0, 1, 1}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(8),  LdpFP##prefix##8,  "_p420.yuv"},       \
        {LdpColorFormatI420_10_LE,     LdpColorSpaceYUV,       3, 1, 1, {0, 1, 1}, {0, 1, 1}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(10), LdpFP##prefix##10, "_10bit_p420.yuv"}, \
        {LdpColorFormatI420_12_LE,     LdpColorSpaceYUV,       3, 1, 1, {0, 1, 1}, {0, 1, 1}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(12), LdpFP##prefix##12, "_12bit_p420.yuv"}, \
        {LdpColorFormatI420_14_LE,     LdpColorSpaceYUV,       3, 1, 1, {0, 1, 1}, {0, 1, 1}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(14), LdpFP##prefix##14, "_14bit_p420.yuv"}, \
        {LdpColorFormatI420_16_LE,     LdpColorSpaceYUV,       3, 1, 1, {0, 1, 1}, {0, 1, 1}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(16), LdpFP##prefix##14, "_16bit_p420.yuv"}, \
                                                                                                                                                                                      \
        {LdpColorFormatI422_8,         LdpColorSpaceYUV,       3, 1, 0, {0, 1, 1}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(8),  LdpFP##prefix##8,  "_p422.yuv"},       \
        {LdpColorFormatI422_10_LE,     LdpColorSpaceYUV,       3, 1, 0, {0, 1, 1}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(10), LdpFP##prefix##10, "_10bit_p422.yuv"}, \
        {LdpColorFormatI422_12_LE,     LdpColorSpaceYUV,       3, 1, 0, {0, 1, 1}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(12), LdpFP##prefix##12, "_12bit_p422.yuv"}, \
        {LdpColorFormatI422_14_LE,     LdpColorSpaceYUV,       3, 1, 0, {0, 1, 1}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(14), LdpFP##prefix##14, "_14bit_p422.yuv"}, \
        {LdpColorFormatI422_16_LE,     LdpColorSpaceYUV,       3, 1, 0, {0, 1, 1}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(16), LdpFP##prefix##14, "_16bit_p422.yuv"}, \
                                                                                                                                                                                      \
        {LdpColorFormatI444_8,         LdpColorSpaceYUV,       3, 0, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(8),  LdpFP##prefix##8,  "_p444.yuv"},       \
        {LdpColorFormatI444_10_LE,     LdpColorSpaceYUV,       3, 0, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(10), LdpFP##prefix##10, "_10bit_p444.yuv"}, \
        {LdpColorFormatI444_12_LE,     LdpColorSpaceYUV,       3, 0, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(12), LdpFP##prefix##12, "_12bit_p444.yuv"}, \
        {LdpColorFormatI444_14_LE,     LdpColorSpaceYUV,       3, 0, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(14), LdpFP##prefix##14, "_14bit_p444.yuv"}, \
        {LdpColorFormatI444_16_LE,     LdpColorSpaceYUV,       3, 0, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(16), LdpFP##prefix##14, "_16bit_p444.yuv"}, \
                                                                                                                                                                                      \
        {LdpColorFormatNV12_8,         LdpColorSpaceYUV,       3, 1, 1, {0, 1},    {0, 1},    {0, 0},    {1, 2, 2},    {0, 0, 1},    BITS(8),  LdpFP##prefix##8,  ".nv12"},           \
        {LdpColorFormatNV21_8,         LdpColorSpaceYUV,       3, 1, 1, {0, 1},    {0, 1},    {0, 0},    {1, 2, 2},    {0, 1, 0},    BITS(8),  LdpFP##prefix##8,  ".nv21"},           \
                                                                                                                                                                                      \
        {LdpColorFormatRGB_8,          LdpColorSpaceRGB,       3, 0, 0, {0},       {0},       {0},       {3, 3, 3},    {0, 1, 2},    BITS(8),  LdpFP##prefix##8,  ".rgb"},            \
        {LdpColorFormatBGR_8,          LdpColorSpaceRGB,       3, 0, 0, {0},       {0},       {0},       {3, 3, 3},    {2, 1, 0},    BITS(8),  LdpFP##prefix##8,  ".bgr"},            \
        {LdpColorFormatRGBA_8,         LdpColorSpaceRGB,       4, 0, 0, {0},       {0},       {0},       {4, 4, 4, 4}, {0, 1, 2, 3}, BITS(8),  LdpFP##prefix##8,  ".rgba"},           \
        {LdpColorFormatBGRA_8,         LdpColorSpaceRGB,       4, 0, 0, {0},       {0},       {0},       {4, 4, 4, 4}, {2, 1, 0, 3}, BITS(8),  LdpFP##prefix##8,  ".bgra"},           \
        {LdpColorFormatARGB_8,         LdpColorSpaceRGB,       4, 0, 0, {0},       {0},       {0},       {4, 4, 4, 4}, {3, 0, 1, 2}, BITS(8),  LdpFP##prefix##8,  ".argb"},           \
        {LdpColorFormatABGR_8,         LdpColorSpaceRGB,       4, 0, 0, {0},       {0},       {0},       {4, 4, 4, 4}, {3, 2, 1, 0}, BITS(8),  LdpFP##prefix##8,  ".abgr"},           \
                                                                                                                                                                                      \
        {LdpColorFormatGRAY_8,         LdpColorSpaceGreyscale, 1, 0, 0, {0},       {0},       {0},       {1},          {0},          BITS(8),  LdpFP##prefix##8,  ".y"},              \
        {LdpColorFormatGRAY_10_LE,     LdpColorSpaceGreyscale, 1, 0, 0, {0},       {0},       {0},       {1},          {0},          BITS(10), LdpFP##prefix##10, "_10bit.y"},        \
        {LdpColorFormatGRAY_12_LE,     LdpColorSpaceGreyscale, 1, 0, 0, {0},       {0},       {0},       {1},          {0},          BITS(12), LdpFP##prefix##12, "_12bit.y"},        \
        {LdpColorFormatGRAY_14_LE,     LdpColorSpaceGreyscale, 1, 0, 0, {0},       {0},       {0},       {1},          {0},          BITS(14), LdpFP##prefix##14, "_14bit.y"},        \
        {LdpColorFormatGRAY_16_LE,     LdpColorSpaceGreyscale, 1, 0, 0, {0},       {0},       {0},       {1},          {0},          BITS(16), LdpFP##prefix##14, "_16bit.y"},        \
    };                                                                                                                                                                                \

//Make a layout table for unsigned formats
#define BITS(bits) bits
VN_PICTURE_LAYOUTS(, U)
#undef BITS

//Make a layout table for internal signed formats
#define BITS(bits) 16
VN_PICTURE_LAYOUTS(Internal, S)
#undef BITS


#define kInternalFormat 1
#define kExternalFormat 0
// clang-format on

// LayoutInfo for unknown formats
static const LdpPictureLayoutInfo kPictureLayoutInfoUnknown = {LdpColorFormatUnknown};

// Find the layout info for a given format
static const LdpPictureLayoutInfo* findLayoutInfo(LdpColorFormat format, bool internal)
{
    for (uint32_t i = 0; i < (sizeof(kPictureLayoutInfo) / sizeof(kPictureLayoutInfo[0])); ++i) {
        if (kPictureLayoutInfo[i].format == format) {
            return internal ? &kInternalPictureLayoutInfo[i] : &kPictureLayoutInfo[i];
        }
    }

    return &kPictureLayoutInfoUnknown;
}

uint8_t ldpColorFormatBitsPerSample(LdpColorFormat format)
{
    return findLayoutInfo(format, kExternalFormat)->bits;
}

uint8_t ldpColorFormatPlaneWidthShift(LdpColorFormat format, uint32_t planeIdx)
{
    return findLayoutInfo(format, kExternalFormat)->planeWidthShift[planeIdx];
}

uint8_t ldpColorFormatPlaneHeightShift(LdpColorFormat format, uint32_t planeIdx)
{
    return findLayoutInfo(format, kExternalFormat)->planeHeightShift[planeIdx];
}

bool ldpPictureDescCheckValidStrides(const LdpPictureDesc* pictureDesc,
                                     const uint32_t rowStrides[kLdpPictureMaxNumPlanes])
{
    LdpPictureLayout layout;
    ldpPictureLayoutInitializeDesc(&layout, pictureDesc, 0);
    for (uint32_t plane = 0; plane < ldpPictureLayoutPlanes(&layout); plane++) {
        if (rowStrides[plane] < ldpPictureLayoutDefaultRowStride(&layout, plane, 0)) {
            return false;
        }
    }
    return true;
}

uint8_t ldpPictureLayoutPlaneForComponent(const LdpPictureLayout* layout, uint8_t component)
{
    uint8_t plane = 0;
    for (uint8_t testComp = 0; testComp < component;
         testComp += layout->layoutInfo->interleave[testComp + 1]) {
        plane++;
    }
    return plane;
}

uint8_t ldpPictureLayoutComponentForPlane(const LdpPictureLayout* layout, uint8_t plane)
{
    uint8_t component = 0;
    for (uint8_t testPlane = 0; testPlane < plane; testPlane++) {
        component += layout->layoutInfo->interleave[component];
    }
    // Proceed through the components in this plane until you find the one with no offset
    while (layout->layoutInfo->offset[component] != 0) {
        component++;
    }
    return component;
}

// Work out minimum stride from width
uint32_t ldpPictureLayoutDefaultRowStride(const LdpPictureLayout* layout, uint32_t plane, uint32_t minAlignment)
{
    assert(plane < ldpPictureLayoutPlanes(layout));
    uint32_t align = layout->layoutInfo->alignment[plane];
    if (minAlignment > align) {
        align = minAlignment;
    }

    const uint32_t defaultStride = (ldpPictureLayoutRowSize(layout, plane) + align) & ~align;

    // Default stride
    return defaultStride;
}

// Return true if the layouts are compatible
bool ldpPictureLayoutIsCompatible(const LdpPictureLayout* layout, const LdpPictureLayout* other)
{
    // Must be same dimension
    if (layout->width != other->width || layout->height != other->height) {
        return false;
    }

    // If it is exactly the same format - it is compatible
    if (layout->layoutInfo->format == other->layoutInfo->format) {
        return true;
    }

    // Sample bit depths must match
    if (layout->layoutInfo->bits != other->layoutInfo->bits) {
        return false;
    }

    // Number of color components must match
    if (layout->layoutInfo->colorComponents != other->layoutInfo->colorComponents) {
        return false;
    }

    // Shifts must match
    for (uint32_t plane = 0; plane < ldpPictureLayoutPlanes(layout); ++plane) {
        if (layout->layoutInfo->planeWidthShift[plane] != other->layoutInfo->planeWidthShift[plane] ||
            layout->layoutInfo->planeHeightShift[plane] != other->layoutInfo->planeHeightShift[plane]) {
            return false;
        }
    }

    // Other differences (e.g. order of color components) don't affect the memory footprint of the
    // actual content of the picture, so are ignored.
    return true;
}

uint8_t ldpPictureLayoutIsInterleaved(const LdpPictureLayout* layout)
{
    for (uint32_t i = 0; i < kLdpPictureMaxColorComponents; ++i) {
        if (layout->layoutInfo->interleave[i] > 1) {
            return true;
        }
    }
    return false;
}

// Initialization
//
// Fill in plane offsets based on stride and interleave
static void generateOffsets(LdpPictureLayout* layout)
{
    // Work out per plane offsets
    uint32_t offset = 0;
    for (uint32_t plane = 0; plane < ldpPictureLayoutPlanes(layout); ++plane) {
        layout->planeOffsets[plane] = offset;
        offset += layout->rowStrides[plane] *
                  (layout->height >> layout->layoutInfo->planeHeightShift[plane]);
    }

    // Store final offset as total size
    layout->size = offset;
}

void ldpInternalPictureLayoutInitialize(LdpPictureLayout* layout, LdpColorFormat format,
                                        uint32_t width, uint32_t height, uint32_t minRowAlignment)
{
    VNClear(layout);

    layout->layoutInfo = findLayoutInfo(format, kInternalFormat);
    layout->width = width;
    layout->height = height;

    // Figure out per plane strides
    for (uint32_t plane = 0; plane < ldpPictureLayoutPlanes(layout); ++plane) {
        layout->rowStrides[plane] = ldpPictureLayoutDefaultRowStride(layout, plane, minRowAlignment);
    }

    generateOffsets(layout);
}

void ldpPictureLayoutInitialize(LdpPictureLayout* layout, LdpColorFormat format, uint32_t width,
                                uint32_t height, uint32_t minRowAlignment)
{
    VNClear(layout);

    layout->layoutInfo = findLayoutInfo(format, kExternalFormat);
    layout->width = width;
    layout->height = height;

    // Figure out per plane strides
    for (uint32_t plane = 0; plane < ldpPictureLayoutPlanes(layout); ++plane) {
        layout->rowStrides[plane] = ldpPictureLayoutDefaultRowStride(layout, plane, minRowAlignment);
    }

    generateOffsets(layout);
}

void ldpPictureLayoutInitializeStrides(LdpPictureLayout* layout, LdpColorFormat format, uint32_t width,
                                       uint32_t height, const uint32_t strides[kLdpPictureMaxNumPlanes])
{
    VNClear(layout);

    layout->layoutInfo = findLayoutInfo(format, kExternalFormat);
    layout->width = width;
    layout->height = height;

    // Fill in supplied strides
    for (uint32_t plane = 0; plane < ldpPictureLayoutPlanes(layout); ++plane) {
        assert(strides[plane] >= ldpPictureLayoutDefaultRowStride(layout, plane, 0));
        layout->rowStrides[plane] = strides[plane];
    }

    generateOffsets(layout);
}

void ldpPictureLayoutInitializeDesc(LdpPictureLayout* layout, const LdpPictureDesc* pictureDesc,
                                    uint32_t minRowAlignment)
{
    ldpPictureLayoutInitialize(layout, pictureDesc->colorFormat, pictureDesc->width,
                               pictureDesc->height, minRowAlignment);
}

void ldpPictureLayoutInitializeDescStrides(LdpPictureLayout* layout, const LdpPictureDesc* pictureDesc,
                                           const uint32_t strides[kLdpPictureMaxNumPlanes])
{
    ldpPictureLayoutInitializeStrides(layout, pictureDesc->colorFormat, pictureDesc->width,
                                      pictureDesc->height, strides);
}

void ldpPictureLayoutInitializeEmpty(LdpPictureLayout* layout)
{
    VNClear(layout);
    layout->layoutInfo = &kPictureLayoutInfoUnknown;
}
