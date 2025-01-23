/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

// A value type to hold all the memory layout sizes and strides, for a given LCEVC_PictureDesc
//
#ifndef VN_LCEVC_UTILITY_PICTURE_LAYOUT_H
#define VN_LCEVC_UTILITY_PICTURE_LAYOUT_H

#include "LCEVC/lcevc_dec.h"

#include <cassert>
#include <cstdint>
#include <string>

namespace lcevc_dec::utility {

class PictureLayout
{
public:
    // Maximum number of components is 4 (3 traditional colors, plus alpha). Maximum number of
    // planes is 3 (since we don't currently support any non-interleaved 4-component formats).
    static constexpr uint32_t kMaxColorComponents = 4;
    static constexpr uint32_t kMaxNumPlanes = 3;

    enum ColorSpace
    {
        YUV,
        RGB,
        Greyscale,
    };

    PictureLayout();
    PictureLayout(const LCEVC_PictureDesc& pictureDescription);
    PictureLayout(const LCEVC_PictureDesc& pictureDescription, const uint32_t rowStrides[kMaxNumPlanes]);

    // Shortcuts that build the LCEVC_PictureDesc given format and dimensions
    PictureLayout(LCEVC_ColorFormat format, uint32_t width, uint32_t height);
    PictureLayout(LCEVC_ColorFormat format, uint32_t width, uint32_t height,
                  const uint32_t rowStrides[kMaxNumPlanes]);

    // Fetch description from given picture
    PictureLayout(LCEVC_DecoderHandle decoderHandle, LCEVC_PictureHandle pictureHandle);

    // The format and "nominal" width and height of the layout.
    LCEVC_ColorFormat format() const;
    uint32_t width() const;
    uint32_t height() const;

    // Return true if width and height are valid for the chosen format
    bool isValid() const;

    // Return total number of planes in image
    uint8_t planes() const;

    // Return total number of color components in the image
    uint8_t colorComponents() const;

    // Return the plane that a component resides in, or the first component which resides in the
    // given plane
    uint8_t getPlaneForComponent(uint8_t component) const;
    uint8_t getComponentForPlane(uint8_t plane) const;

    // Return total size in bytes of image
    uint32_t size() const;

    // Return width of plane in pixels
    uint32_t planeWidth(uint32_t plane) const;

    // Return height of plane in pixels
    uint32_t planeHeight(uint32_t plane) const;

    // Return bytes offset of plane within image
    uint32_t planeOffset(uint32_t plane) const;

    // Return bytes offset of component within image (including plane's offset relative to 0th plane).
    uint32_t componentOffset(uint8_t component) const;

    // Return bytes in the given plane
    uint32_t planeSize(uint32_t plane) const;

    // Return interleave count (i.e. number of color components) of a given plane. In other words,
    // the number of "samples" for each "pixel".
    uint8_t planeInterleave(uint8_t plane) const;

    // Return interleave count of a given component (i.e. how many color components is this
    // component sharing a plane with)
    uint8_t componentInterleave(uint8_t component) const;

    // Return byte offset of pixel row within plane
    uint32_t rowOffset(uint8_t plane, uint32_t row) const;

    // Return byte stride between subsequent rows of plane
    uint32_t rowStride(uint32_t plane) const;

    // Return calculated minimum/default stride from width and format
    uint32_t defaultRowStride(uint32_t plane) const;

    // Return byte stride between horizontal samples of plane
    uint32_t sampleStride(uint32_t plane) const;

    // Return byte size of pixel row (may be less than stride due to alignment)
    uint32_t rowSize(uint32_t plane) const;

    // Return bytes per sample
    uint8_t sampleSize() const;

    // Return bits per sample
    uint8_t sampleBits() const;

    // Return colour space enum
    ColorSpace colorSpace() const;

    // Return true if there are no gaps between rows of pixels
    bool rowsAreContiguous(uint32_t plane) const;

    // Return true if this color component is in a contiguous (NON-interleaved) plane
    bool samplesAreContiguous(uint32_t component) const;

    // Return true if the other PictureLayout is a compatible image. Two pictures are compatible
    // iff you can copy the content (i.e. non-padding data) from either picture to the other,
    // without writing into impermissible memory (e.g. past the end of the buffer, into row
    // padding, or into per-pixel padding in the case of 10bit data stored in 16bit types).
    bool isCompatible(const PictureLayout& other) const;

    // Return true if using an interleaved format
    bool isInterleaved() const;

    // Construct a vooya/YUView compatible raw filename based on name
    std::string makeRawFilename(std::string_view name) const;

    // Static accessors, to get info about a format without creating a specific layout object
    static uint8_t getBitsPerSample(LCEVC_ColorFormat format);
    static uint8_t getPlaneWidthShift(LCEVC_ColorFormat format, uint32_t planeIdx);
    static uint8_t getPlaneHeightShift(LCEVC_ColorFormat format, uint32_t planeIdx);
    static bool checkValidStrides(const LCEVC_PictureDesc& pictureDesc,
                                  const uint32_t rowStrides[kMaxNumPlanes]);
    static bool getPaddedStrides(const LCEVC_PictureDesc& pictureDesc, uint32_t rowStrides[kMaxNumPlanes]);

private:
    // Per format constants - held as a static table
    // As a matter of terminology:
    // Color component - one of: Y, U, V, R, G, B, or A
    // Plane - a contiguous region of memory representing one or more color components, for a given frame.
    // Sample - a colour component at a given coordinate.
    // Pixel - the repeating interleave unit of a given plane.
    struct Info
    {
        LCEVC_ColorFormat format; // The format
        ColorSpace colorSpace;    // High-level colorspace distinction
        uint8_t colorComponents; // Number of distinct color components (also called channels) in image
        uint8_t validWidthMask;  // Width bits that must be zero for valid picture
        uint8_t validHeightMask; // Height bits that must be zero for valid picture

        uint8_t planeWidthShift[kMaxNumPlanes]; // This plane's width (in "pixels", i.e. repeating interleaved units) is the nominal width, divided by 2^planeWidthShift
        uint8_t planeHeightShift[kMaxNumPlanes]; // This plane's height is the nominal height, divided by 2^planeHeightShift
        uint8_t alignment[kMaxNumPlanes]; // Alignment of this plane (that is: this plane's width, in bytes, is padded to a multiple of 2^alignment)

        uint8_t interleave[kMaxColorComponents]; // Interleaving of this component - number of components in the plane that this component is in (including itself)
        uint8_t offset[kMaxColorComponents]; // Offset of component within repeating interleave unit.

        uint8_t bits;       // Number of LSB bits per sample
        const char* suffix; // Vooya compatible suffix for files
    };

    static const Info kPictureLayoutInfo[];
    static const Info kPictureLayoutInfoUnknown;
    static const Info& findLayoutInfo(LCEVC_ColorFormat format);

    PictureLayout(const LCEVC_PictureDesc& pictureDesc, const Info& layoutInfo);
    PictureLayout(const LCEVC_PictureDesc& pictureDesc, const Info& layoutInfo,
                  const uint32_t strides[kMaxNumPlanes]);

    void generateOffsets();

    // Format and "nominal" dimensions.
    const Info* m_layoutInfo;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // The size, in bytes, of each row of each plane, and the whole image.
    uint32_t m_rowStrides[kMaxNumPlanes]{0};
    uint32_t m_size = 0;

    // The offset of each plane relative to the 0th sample's address.
    uint32_t m_planeOffsets[kMaxNumPlanes]{0};
};

// Inline functions
#include "picture_layout_inl.h"

} // namespace lcevc_dec::utility

#endif
