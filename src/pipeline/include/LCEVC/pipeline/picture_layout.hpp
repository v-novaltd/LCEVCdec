/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova International Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef VN_LCEVC_PIPELINE_PICTURE_LAYOUT_HPP
#define VN_LCEVC_PIPELINE_PICTURE_LAYOUT_HPP

// C++ wrapper for LdpPictureLayout - Constructors, and forwarding to C functions
//
// Everything is static inline all the way down, so 'should' generate sensible code.
//
#include <LCEVC/pipeline/detail/picture_layout.h>
#include <LCEVC/pipeline/picture_layout.h>
#include <LCEVC/pipeline/picture.h>

#include <string>

namespace lcevc_dec::pipeline {

class PictureLayout : public LdpPictureLayout {
public:
    static constexpr uint32_t kMaxNumPlanes = kLdpPictureMaxColorComponents;

    PictureLayout() : LdpPictureLayout() { ldpPictureLayoutInitializeEmpty(this); }
    PictureLayout(const LdpPictureDesc& pictureDescription) : LdpPictureLayout() { ldpPictureLayoutInitializeDesc(this, &pictureDescription, 0); }
    PictureLayout(const LdpPictureDesc& pictureDescription, const uint32_t rowStrides[kMaxNumPlanes]) : LdpPictureLayout() {ldpPictureLayoutInitializeDescStrides(this, &pictureDescription, rowStrides);  }

    // Shortcuts that build the LdpPictureDesc given format and dimensions
    PictureLayout(LdpColorFormat format, uint32_t width, uint32_t height) : LdpPictureLayout() { ldpPictureLayoutInitialize(this, format, width, height, 0); }
    PictureLayout(LdpColorFormat format, uint32_t width, uint32_t height,
                  const uint32_t rowStrides[kMaxNumPlanes]) : LdpPictureLayout(){ ldpPictureLayoutInitializeStrides(this, format, width, height, rowStrides); }

    LdpColorFormat format() const { return ldpPictureLayoutFormat(this); }
    uint32_t width() const { return ldpPictureLayoutWidth(this); }
    uint32_t height() const { return ldpPictureLayoutHeight(this); }

    // Return true if width and height are valid for the chosen format
    bool isValid() const { return ldpPictureLayoutIsValid(this); }

    // Return total number of planes in image
    uint8_t planes() const { return ldpPictureLayoutPlanes(this); }

    // Return total number of color components in the image
    uint8_t colorComponents()  { return ldpPictureLayoutColorComponents(this); }

    // Return the plane that a component resides in, or the first component which resides in the
    // given plane
    uint8_t getPlaneForComponent(uint8_t component) const { return ldpPictureLayoutPlaneForComponent(this, component); }
    uint8_t getComponentForPlane(uint8_t plane) const { return ldpPictureLayoutComponentForPlane(this, plane); }

    // Return total size in bytes of image
    uint32_t size() const  { return ldpPictureLayoutSize(this); }

    // Return width of plane in pixels
    uint32_t planeWidth(uint32_t plane) const { return ldpPictureLayoutPlaneWidth(this, plane); }

    // Return height of plane in pixels
    uint32_t planeHeight(uint32_t plane) const { return ldpPictureLayoutPlaneHeight(this, plane); }

    // Return bytes offset of plane within image
    uint32_t planeOffset(uint32_t plane) const { return ldpPictureLayoutPlaneOffset(this, plane); }

    // Return bytes offset of component within image (including plane's offset relative to 0th plane).
    uint32_t componentOffset(uint8_t component) const { return ldpPictureLayoutComponentOffset(this, component); }

    // Return bytes in the given plane
    uint32_t planeSize(uint32_t plane) const { return ldpPictureLayoutPlaneSize(this, plane); }

    // Return interleave count (i.e. number of color components) of a given plane. In other words,
    // the number of "samples" for each "pixel".
    uint8_t planeInterleave(uint8_t plane) const { return ldpPictureLayoutPlaneInterleave(this, plane); }

    // Return interleave count of a given component (i.e. how many color components is this
    // component sharing a plane with)
    uint8_t componentInterleave(uint8_t component) const { return ldpPictureLayoutComponentInterleave(this, component); }

    // Return byte offset of pixel row within plane
    uint32_t rowOffset(uint8_t plane, uint32_t row) const { return ldpPictureLayoutRowOffset(this, plane, row); }

    // Return byte stride between subsequent rows of plane
    uint32_t rowStride(uint32_t plane) const { return ldpPictureLayoutRowStride(this, plane); }

    // Return calculated minimum/default stride from width and format
    uint32_t defaultRowStride(uint32_t plane) const { return ldpPictureLayoutDefaultRowStride(this, plane, 0); }

    // Return byte stride between horizontal samples of plane
    uint32_t sampleStride(uint32_t plane) const { return ldpPictureLayoutSampleStride(this, plane); }

    // Return byte size of pixel row (may be less than stride due to alignment)
    uint32_t rowSize(uint32_t plane) const { return ldpPictureLayoutRowSize(this, plane); }

    // Return bytes per sample
    uint8_t sampleSize() const { return ldpPictureLayoutSampleSize(this); }

    // Return bits per sample
    uint8_t sampleBits() const { return ldpPictureLayoutSampleBits(this); }

    // Return colour space enum
    LdpColorSpace colorSpace() const { return ldpPictureLayoutColorSpace(this); }

    // Return colour space enum
    std::string suffix() const { return ldpPictureLayoutSuffix(this); }

    // Return true if there are no gaps between rows of pixels
    bool rowsAreContiguous(uint32_t plane) const { return ldpPictureLayoutAreRowsContiguous(this, plane); }

    // Return true if this color component is in a contiguous (NON-interleaved) plane
    bool samplesAreContiguous(uint32_t component) const { return ldpPictureLayoutAreSamplesContiguous(this, component); }

    // Return true if the other PictureLayout is a compatible image. Two pictures are compatible
    // iff you can copy the content (i.e. non-padding data) from either picture to the other,
    // without writing into impermissible memory (e.g. past the end of the buffer, into row
    // padding, or into per-pixel padding in the case of 10bit data stored in 16bit types).
    bool isCompatible(const PictureLayout& other) const { return ldpPictureLayoutIsCompatible(this, &other); }

    // Return true if using an interleaved format
    bool isInterleaved() const { return ldpPictureLayoutIsInterleaved(this); }

    // Construct a vooya/YUView compatible raw filename based on name
    std::string makeRawFilename(std::string_view name) const;

    // Static accessors, to get info about a format without creating a specific layout object
    static uint8_t getBitsPerSample(LdpColorFormat format)  { return ldpColorFormatBitsPerSample(format); }
    static uint8_t getPlaneWidthShift(LdpColorFormat format, uint32_t planeIdx)  { return ldpColorFormatPlaneWidthShift(format, planeIdx); }
    static uint8_t getPlaneHeightShift(LdpColorFormat format, uint32_t planeIdx) { return ldpColorFormatPlaneHeightShift(format, planeIdx); }
    static bool checkValidStrides(const LdpPictureDesc& pictureDesc,
                                  const uint32_t rowStrides[kMaxNumPlanes])  { return ldpPictureDescCheckValidStrides(&pictureDesc, rowStrides); }
    static bool getPaddedStrides(const LdpPictureDesc& pictureDesc, uint32_t rowStrides[kMaxNumPlanes])  { return ldpPictureDescPaddedStrides(&pictureDesc, rowStrides); }
};

} // namespace lcevc_dec::pipeline

#endif
// Inline functions
