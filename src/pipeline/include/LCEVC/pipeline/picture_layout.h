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

// A value type to hold all the memory layout sizes and strides, for a given LdpPictureDesc
//
#ifndef VN_LCEVC_PIPELINE_PICTURE_LAYOUT_H
#define VN_LCEVC_PIPELINE_PICTURE_LAYOUT_H

#include <LCEVC/pipeline/types.h>
#include <stdint.h>

// NOLINTBEGIN(modernize-use-using)

#ifdef __cplusplus
extern "C"
{
#endif

#define kLdpPictureMaxNumPlanes 4
#define kLdpPictureMaxColorComponents 4

typedef struct LdpPictureLayoutInfo LdpPictureLayoutInfo;

// A layout for a picture: width, height strides (and cropping?)
//
typedef struct LdpPictureLayout
{
    // Format and "nominal" dimensions.
    const LdpPictureLayoutInfo* layoutInfo;
    uint32_t width;
    uint32_t height;

    // The size, in bytes, of each row of each plane, and the whole image.
    uint32_t rowStrides[kLdpPictureMaxNumPlanes];
    uint32_t size;

    // The offset of each plane relative to the 0th sample's address.
    uint32_t planeOffsets[kLdpPictureMaxNumPlanes];
} LdpPictureLayout;

// Cropping for pictures
typedef struct LdpPictureMargins
{
    uint32_t left;
    uint32_t top;
    uint32_t right;
    uint32_t bottom;
} LdpPictureMargins;

// Constant per format info  that describes a picture layout
//
struct LdpPictureLayoutInfo
{
    LdpColorFormat format;    // The format
    LdpColorSpace colorSpace; // High-level colorspace distinction
    uint8_t colorComponents;  // Number of distinct color components (also called channels) in image
    uint8_t validWidthMask;   // Width bits that must be zero for valid picture
    uint8_t validHeightMask;  // Height bits that must be zero for valid picture

    uint8_t planeWidthShift[kLdpPictureMaxNumPlanes]; // This plane's width (in "pixels", i.e. repeating interleaved units) is the nominal width, divided by 2^planeWidthShift
    uint8_t planeHeightShift[kLdpPictureMaxNumPlanes]; // This plane's height is the nominal height, divided by 2^planeHeightShift
    uint8_t alignment[kLdpPictureMaxNumPlanes]; // Alignment of this plane (that is: this plane's width, in bytes, is padded to a multiple of 2^alignment)

    uint8_t interleave[kLdpPictureMaxColorComponents]; // Interleaving of this component - number of components in the plane that this component is in (including itself)
    uint8_t offset[kLdpPictureMaxColorComponents]; // Offset of component within repeating interleave unit.

    uint8_t bits;             // Number of LSB bits per sample
    LdpFixedPoint fixedPoint; // Fixed point format
    const char* suffix;       // Vooya compatible suffix for files
};

// Return the plane that contains a component
uint8_t ldpPictureLayoutPlaneForComponent(const LdpPictureLayout* layout, uint8_t component);
// Return the component contained with a plane
uint8_t ldpPictureLayoutComponentForPlane(const LdpPictureLayout* layout, uint8_t plane);

// Return total number of planes in image
static inline uint8_t ldpPictureLayoutPlanes(const LdpPictureLayout* layout);
static inline uint8_t ldpPictureLayoutPlaneInterleave(const LdpPictureLayout* layout, uint8_t plane);

// Return colour format
static inline LdpColorFormat ldpPictureLayoutFormat(const LdpPictureLayout* layout);

// Return width of image in samples
static inline uint32_t ldpPictureLayoutWidth(const LdpPictureLayout* layout);

// Return height of image in samples
static inline uint32_t ldpPictureLayoutHeight(const LdpPictureLayout* layout);

// Return width of one plane within image in samples
static inline uint32_t ldpPictureLayoutPlaneWidth(const LdpPictureLayout* layout, uint32_t plane);

// Return height of one plane within image in samples
static inline uint32_t ldpPictureLayoutPlaneHeight(const LdpPictureLayout* layout, uint32_t plane);

// Return true if the layout's width, height and strides are compatible with the format
static inline bool ldpPictureLayoutIsValid(const LdpPictureLayout* layout);

// Return total number of color components in the picture
static inline uint8_t ldpPictureLayoutColorComponents(const LdpPictureLayout* layout);

// Return total size in bytes of picture
static inline uint32_t ldpPictureLayoutSize(const LdpPictureLayout* layout);

// Return size in bytes if the given plane
static inline uint32_t ldpPictureLayoutPlaneSize(const LdpPictureLayout* layout, uint32_t plane);

// Return byte offset of plane within picture's bytes
static inline uint32_t ldpPictureLayoutPlaneOffset(const LdpPictureLayout* layout, uint32_t plane);

// Return byte offset of component within picture (including plane's offset relative to 0th plane).
static inline uint32_t ldpPictureLayoutComponentOffset(const LdpPictureLayout* layout, uint8_t component);

// Return interleave count of a given component
// (i.e. how many color components is this component sharing a plane with)
static inline uint8_t ldpPictureLayoutComponentInterleave(const LdpPictureLayout* layout, uint8_t component);

// Return byte offset of pixel row within plane
static inline uint32_t ldpPictureLayoutRowOffset(const LdpPictureLayout* layout, uint8_t plane,
                                                 uint32_t row);

// Return byte stride between subsequent rows of plane
static inline uint32_t ldpPictureLayoutRowStride(const LdpPictureLayout* layout, uint32_t plane);

// Return calculated minimum/default stride from width and format
uint32_t ldpPictureLayoutDefaultRowStride(const LdpPictureLayout* layout, uint32_t plane,
                                          uint32_t minRowAlignment);

// Return bytes per sample
static inline uint8_t ldpPictureLayoutSampleSize(const LdpPictureLayout* layout);

// Return byte stride between horizontal samples of plane
static inline uint32_t ldpPictureLayoutSampleStride(const LdpPictureLayout* layout, uint32_t plane);

// Return byte size of pixel row (may be less than stride due to alignment)
static inline uint32_t ldpPictureLayoutRowSize(const LdpPictureLayout* layout, uint32_t plane);

// Return bits per sample
static inline uint8_t ldpPictureLayoutSampleBits(const LdpPictureLayout* layout);

// Return color space of picture samples
static inline LdpColorSpace ldpPictureLayoutColorSpace(const LdpPictureLayout* layout);

// Return the extension to use for raw output files of this format
static inline const char* ldpPictureLayoutSuffix(const LdpPictureLayout* layout);

// Return true if there are no gaps between rows of pixels
static inline bool ldpPictureLayoutAreRowsContiguous(const LdpPictureLayout* layout, uint32_t plane);

// Return true if this color component is in a contiguous (NON-interleaved) plane
static inline bool ldpPictureLayoutAreSamplesContiguous(const LdpPictureLayout* layout, uint32_t component);

// Return true if using an interleaved format
uint8_t ldpPictureLayoutIsInterleaved(const LdpPictureLayout* layout);

// Return true if two layouts are 'compatible' - same size and components, possibly with different interleaving.
bool ldpPictureLayoutIsCompatible(const LdpPictureLayout* layout, const LdpPictureLayout* other);

// Related information about color formats
//
uint8_t ldpColorFormatBitsPerSample(LdpColorFormat format);
uint8_t ldpColorFormatPlaneWidthShift(LdpColorFormat format, uint32_t planeIdx);
uint8_t ldpColorFormatPlaneHeightShift(LdpColorFormat format, uint32_t planeIdx);

// Utilities for checking LdpPictureDesc
//
bool ldpPictureDescCheckValidStrides(const LdpPictureDesc* pictureDesc,
                                     const uint32_t rowStrides[kLdpPictureMaxNumPlanes]);
bool ldpPictureDescPaddedStrides(const LdpPictureDesc* pictureDesc,
                                 uint32_t rowStrides[kLdpPictureMaxNumPlanes]);

// Initialization
//
void ldpInternalPictureLayoutInitialize(LdpPictureLayout* layout, LdpColorFormat format,
                                        uint32_t width, uint32_t height, uint32_t minRowAlignment);
void ldpPictureLayoutInitialize(LdpPictureLayout* layout, LdpColorFormat format, uint32_t width,
                                uint32_t height, uint32_t minRowAlignment);
void ldpPictureLayoutInitializeStrides(LdpPictureLayout* layout, LdpColorFormat format, uint32_t width,
                                       uint32_t height, const uint32_t strides[kLdpPictureMaxNumPlanes]);
void ldpPictureLayoutInitializeDesc(LdpPictureLayout* layout, const LdpPictureDesc* pictureDesc,
                                    uint32_t minRowAlignment);
void ldpPictureLayoutInitializeDescStrides(LdpPictureLayout* layout, const LdpPictureDesc* pictureDesc,
                                           const uint32_t strides[kLdpPictureMaxNumPlanes]);
void ldpPictureLayoutInitializeEmpty(LdpPictureLayout* layout);

#ifdef __cplusplus
}
#endif

#include "detail/picture_layout.h"

// NOLINTEND(modernize-use-using)

#endif // VN_LCEVC_PIPELINE_PICTURE_LAYOUT_H
