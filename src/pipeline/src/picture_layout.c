/* Copyright (c) V-Nova International Limited 2025-2026. All rights reserved.
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
#include <LCEVC/pipeline/picture_layout.h>
//
#include <LCEVC/common/memory.h>

// clang-format off
#define VN_PICTURE_LAYOUTS(format, prefix)                                                                                                                                            \
    /* Various constants per color format used to work out actual sizes, offsets & strides, and file names */                                                                         \
    static const LdpPictureLayoutInfo k##format##PictureLayoutInfo[] = {                                                                                                              \
/*                                     colorSpace
                                       |                       chromaSubsampling
                                       |                       |             colorComponents
                                       |                       |             |  validWidthMask
                                       |                       |             |  |  validHeightMask
                                       |                       |             |  |  |  planeWidthShift
                                       |                       |             |  |  |  |          planeHeightShift
                                       |                       |             |  |  |  |          |          alignment
                                       |                       |             |  |  |  |          |          |          interleave
                                       |                       |             |  |  |  |          |          |          |             offset
                                       |                       |             |  |  |  |          |          |          |             |             bits
                                       |                       |             |  |  |  |          |          |          |             |             |         fixedPoint
                                       |                       |             |  |  |  |          |          |          |             |             |         |                  suffix */           \
        {LdpColorFormatI420_8,         LdpColorSpaceYUV,       CT420,        3, 1, 1, {0, 1, 1}, {0, 1, 1}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(8),  LdpFP##prefix##8,  "_p420.yuv"},       \
        {LdpColorFormatI420_10_LE,     LdpColorSpaceYUV,       CT420,        3, 1, 1, {0, 1, 1}, {0, 1, 1}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(10), LdpFP##prefix##10, "_10bit_p420.yuv"}, \
        {LdpColorFormatI420_12_LE,     LdpColorSpaceYUV,       CT420,        3, 1, 1, {0, 1, 1}, {0, 1, 1}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(12), LdpFP##prefix##12, "_12bit_p420.yuv"}, \
        {LdpColorFormatI420_14_LE,     LdpColorSpaceYUV,       CT420,        3, 1, 1, {0, 1, 1}, {0, 1, 1}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(14), LdpFP##prefix##14, "_14bit_p420.yuv"}, \
        {LdpColorFormatI420_16_LE,     LdpColorSpaceYUV,       CT420,        3, 1, 1, {0, 1, 1}, {0, 1, 1}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(16), LdpFP##prefix##14, "_16bit_p420.yuv"}, \
                                                                                                                                                                                                    \
        {LdpColorFormatI422_8,         LdpColorSpaceYUV,       CT422,        3, 1, 0, {0, 1, 1}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(8),  LdpFP##prefix##8,  "_p422.yuv"},       \
        {LdpColorFormatI422_10_LE,     LdpColorSpaceYUV,       CT422,        3, 1, 0, {0, 1, 1}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(10), LdpFP##prefix##10, "_10bit_p422.yuv"}, \
        {LdpColorFormatI422_12_LE,     LdpColorSpaceYUV,       CT422,        3, 1, 0, {0, 1, 1}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(12), LdpFP##prefix##12, "_12bit_p422.yuv"}, \
        {LdpColorFormatI422_14_LE,     LdpColorSpaceYUV,       CT422,        3, 1, 0, {0, 1, 1}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(14), LdpFP##prefix##14, "_14bit_p422.yuv"}, \
        {LdpColorFormatI422_16_LE,     LdpColorSpaceYUV,       CT422,        3, 1, 0, {0, 1, 1}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(16), LdpFP##prefix##14, "_16bit_p422.yuv"}, \
                                                                                                                                                                                                    \
        {LdpColorFormatI444_8,         LdpColorSpaceYUV,       CT444,        3, 0, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(8),  LdpFP##prefix##8,  "_p444.yuv"},       \
        {LdpColorFormatI444_10_LE,     LdpColorSpaceYUV,       CT444,        3, 0, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(10), LdpFP##prefix##10, "_10bit_p444.yuv"}, \
        {LdpColorFormatI444_12_LE,     LdpColorSpaceYUV,       CT444,        3, 0, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(12), LdpFP##prefix##12, "_12bit_p444.yuv"}, \
        {LdpColorFormatI444_14_LE,     LdpColorSpaceYUV,       CT444,        3, 0, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(14), LdpFP##prefix##14, "_14bit_p444.yuv"}, \
        {LdpColorFormatI444_16_LE,     LdpColorSpaceYUV,       CT444,        3, 0, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {1, 1, 1},    {0, 0, 0},    BITS(16), LdpFP##prefix##14, "_16bit_p444.yuv"}, \
                                                                                                                                                                                                    \
        {LdpColorFormatNV12_8,         LdpColorSpaceYUV,       CT420,        3, 1, 1, {0, 1},    {0, 1},    {0, 0},    {1, 2, 2},    {0, 0, 1},    BITS(8),  LdpFP##prefix##8,  ".nv12"},           \
        {LdpColorFormatNV21_8,         LdpColorSpaceYUV,       CT420,        3, 1, 1, {0, 1},    {0, 1},    {0, 0},    {1, 2, 2},    {0, 1, 0},    BITS(8),  LdpFP##prefix##8,  ".nv21"},           \
                                                                                                                                                                                                    \
        {LdpColorFormatP010_10,        LdpColorSpaceYUV,       CT420,        3, 1, 1, {0, 1},    {0, 1},    {0, 0},    {1, 2, 2},    {0, 0, 1},    BITS(10), LdpFP##prefix##10,  ".p010"},          \
        {LdpColorFormatP016_16,        LdpColorSpaceYUV,       CT420,        3, 1, 1, {0, 1},    {0, 1},    {0, 0},    {1, 2, 2},    {0, 0, 1},    BITS(16), LdpFP##prefix##14,  ".p016"},          \
                                                                                                                                                                                                    \
        {LdpColorFormatRGB_8,          LdpColorSpaceRGB,       CTMonochrome, 3, 0, 0, {0},       {0},       {0},       {3, 3, 3},    {0, 1, 2},    BITS(8),  LdpFP##prefix##8,  ".rgb"},            \
        {LdpColorFormatBGR_8,          LdpColorSpaceRGB,       CTMonochrome, 3, 0, 0, {0},       {0},       {0},       {3, 3, 3},    {2, 1, 0},    BITS(8),  LdpFP##prefix##8,  ".bgr"},            \
        {LdpColorFormatRGBA_8,         LdpColorSpaceRGB,       CTMonochrome, 4, 0, 0, {0},       {0},       {0},       {4, 4, 4, 4}, {0, 1, 2, 3}, BITS(8),  LdpFP##prefix##8,  ".rgba"},           \
        {LdpColorFormatBGRA_8,         LdpColorSpaceRGB,       CTMonochrome, 4, 0, 0, {0},       {0},       {0},       {4, 4, 4, 4}, {2, 1, 0, 3}, BITS(8),  LdpFP##prefix##8,  ".bgra"},           \
        {LdpColorFormatARGB_8,         LdpColorSpaceRGB,       CTMonochrome, 4, 0, 0, {0},       {0},       {0},       {4, 4, 4, 4}, {3, 0, 1, 2}, BITS(8),  LdpFP##prefix##8,  ".argb"},           \
        {LdpColorFormatABGR_8,         LdpColorSpaceRGB,       CTMonochrome, 4, 0, 0, {0},       {0},       {0},       {4, 4, 4, 4}, {3, 2, 1, 0}, BITS(8),  LdpFP##prefix##8,  ".abgr"},           \
                                                                                                                                                                                                    \
        {LdpColorFormatRGBA_10_2_LE,   LdpColorSpaceRGB,       CTMonochrome, 1, 0, 0, {0},       {0},       {0},       {1, 1, 1, 1}, {0, 0, 0, 0}, BITS(32), LdpFP##prefix##10, ".rgba10"},         \
                                                                                                                                                                                                    \
        {LdpColorFormatGRAY_8,         LdpColorSpaceGreyscale, CTMonochrome, 1, 0, 0, {0},       {0},       {0},       {1},          {0},          BITS(8),  LdpFP##prefix##8,  ".y"},              \
        {LdpColorFormatGRAY_10_LE,     LdpColorSpaceGreyscale, CTMonochrome, 1, 0, 0, {0},       {0},       {0},       {1},          {0},          BITS(10), LdpFP##prefix##10, "_10bit.y"},        \
        {LdpColorFormatGRAY_12_LE,     LdpColorSpaceGreyscale, CTMonochrome, 1, 0, 0, {0},       {0},       {0},       {1},          {0},          BITS(12), LdpFP##prefix##12, "_12bit.y"},        \
        {LdpColorFormatGRAY_14_LE,     LdpColorSpaceGreyscale, CTMonochrome, 1, 0, 0, {0},       {0},       {0},       {1},          {0},          BITS(14), LdpFP##prefix##14, "_14bit.y"},        \
        {LdpColorFormatGRAY_16_LE,     LdpColorSpaceGreyscale, CTMonochrome, 1, 0, 0, {0},       {0},       {0},       {1},          {0},          BITS(16), LdpFP##prefix##14, "_16bit.y"},        \
    };                                                                                                                                                                                              \
// clang-format on

//Make a layout table for unsigned formats
#define BITS(bits) bits
VN_PICTURE_LAYOUTS(, U)
#undef BITS

//Make a layout table for internal signed formats
#define BITS(bits) 16
VN_PICTURE_LAYOUTS(Internal, S)
#undef BITS

static const bool kInternalFormat = true;
static const bool kExternalFormat = false;

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

LdeChroma ldpColorFormatChroma(LdpColorFormat format)
{
    return findLayoutInfo(format, kExternalFormat)->chromSubsampling;
#if 0
    const LdpPictureLayoutInfo* info = findLayoutInfo(format, kExternalFormat);

    if (info->format == LdpColorFormatUnknown) {
        return CT420;
    }
    if (info->colorSpace == LdpColorSpaceGreyscale) {
        return CTMonochrome;
    }
    if (info->colorSpace != LdpColorSpaceYUV) {
        // RGB / packed formats: no chroma subsampling.
        return CT444;
    }
    const uint8_t ws = info->planeWidthShift[1];
    const uint8_t hs = info->planeHeightShift[1];
    if (ws == 1 && hs == 1) {
        return CT420;
    }
    if (ws == 1 && hs == 0) {
        return CT422;
    }
    if (ws == 0 && hs == 0) {
        return CT444;
    }
    return CT420;
#endif
}

LdeChroma ldpPictureLayoutChroma(const LdpPictureLayout* layout)
{
    return ldpColorFormatChroma(ldpPictureLayoutFormat(layout));
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
    const uint8_t colorComponents = layout->layoutInfo->colorComponents;
    uint8_t component = 0;
    for (uint8_t testPlane = 0; testPlane < plane && component < colorComponents; testPlane++) {
        component += layout->layoutInfo->interleave[component];
    }
    // Proceed through the components in this plane until you find the one with no offset
    while (component < colorComponents && layout->layoutInfo->offset[component] != 0) {
        component++;
    }
    return (component < colorComponents) ? component : 0;
}

// Work out minimum stride from width
uint32_t ldpPictureLayoutDefaultRowStride(const LdpPictureLayout* layout, uint32_t plane, uint32_t minAlignment)
{
    assert(plane < ldpPictureLayoutPlanes(layout));
    uint32_t align = 0;
    if (minAlignment == 0) {
        minAlignment = 1;
    }
    if (minAlignment > layout->layoutInfo->alignment[plane]) {
        align = minAlignment - 1;
    } else {
        align = layout->layoutInfo->alignment[plane] - 1;
    }
    assert(((align + 1) & align) == 0);
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
