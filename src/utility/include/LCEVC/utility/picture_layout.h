// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// A value type to hold all the memory layout sizes and strides, for a given LCEVC_PictureDesc
//
#ifndef VN_LCEVC_UTILITY_PICTURE_LAYOUT_H
#define VN_LCEVC_UTILITY_PICTURE_LAYOUT_H

#include "LCEVC/lcevc_dec.h"

#include <cstdint>
#include <string>

namespace lcevc_dec::utility {

class PictureLayout
{
public:
    static constexpr int kMaxPlanes = 4;

    PictureLayout();
    PictureLayout(const LCEVC_PictureDesc& pictureDescription);
    PictureLayout(const LCEVC_PictureDesc& pictureDescription, const uint32_t rowStrides[kMaxPlanes]);

    // Shortcuts that build the LCEVC_PictureDesc given format and dimensions
    PictureLayout(LCEVC_ColorFormat format, uint32_t width, uint32_t height);
    PictureLayout(LCEVC_ColorFormat format, uint32_t width, uint32_t height,
                  const uint32_t rowStrides[kMaxPlanes]);

    // Fetch description from given picture
    PictureLayout(LCEVC_DecoderHandle decoderHandle, LCEVC_PictureHandle pictureHandle);

    LCEVC_ColorFormat format() const;
    uint32_t width() const;
    uint32_t height() const;

    // Return true if width and height are valid for the chosen format
    bool isValid() const;

    // Return total number of planes in image
    uint32_t planes() const;

    // Return total size in bytes of image
    uint32_t size() const;

    // Return width of plane in pixels
    uint32_t planeWidth(uint32_t plane) const;

    // Return height of plane in pixels
    uint32_t planeHeight(uint32_t plane) const;

    // Return bytes offset of plane within image
    uint32_t planeOffset(uint32_t plane) const;

    // Return bytes in the given plane
    uint32_t planeSize(uint32_t plane) const;

    // Return byte offset of pixel row within plane
    uint32_t rowOffset(uint32_t plane, uint32_t row) const;

    // Return byte stride between subsequent rows of plane
    uint32_t rowStride(uint32_t plane) const;

    // Return byte stride between horizontal samples of plane
    uint32_t sampleStride(uint32_t plane) const;

    // Return byte size of pixel row (may be less than stride due to alignment)
    uint32_t rowSize(uint32_t plane) const;

    // Return bytes per sample
    uint32_t sampleSize() const;

    // Return bits per sample
    uint32_t sampleBits() const;

    // Return true if there are no gaps between rows of pixels
    bool rowsAreContiguous(uint32_t plane) const;

    // Return true if there are no gaps between samples in row
    bool samplesAreContiguous(uint32_t plane) const;

    // Return true if the other PictureLayout is a compatible image -
    // same planes and bitdepth, but maybe with different layout
    bool isCompatible(const PictureLayout& other) const;

    // Construct a vooya/YUView compatible raw filename based on name
    std::string makeRawFilename(std::string_view name) const;

    // Static accessors, to get info about a format without creating a specific layout object
    static uint8_t getBitsPerSample(LCEVC_ColorFormat format);
    static uint8_t getPlaneWidthShift(LCEVC_ColorFormat format, uint32_t planeIdx);
    static uint8_t getPlaneHeightShift(LCEVC_ColorFormat format, uint32_t planeIdx);

private:
    // Per format constants - held as a static table
    struct Info
    {
        LCEVC_ColorFormat format; // The format
        uint8_t planes;           // Number of distinct planes in image
        uint8_t validWidthMask;   // Width bits that must be zero for valid picture
        uint8_t validHeightMask;  // Height bits that must be zero for valid picture

        uint8_t planeWidthShift[kMaxPlanes]; // width of image must divide by 2^width_plane_shift for all planes
        uint8_t planeHeightShift[kMaxPlanes]; // height of image must divide by 2^height_plane_shift for all planes
        uint8_t alignment[kMaxPlanes];  // Alignment of this plane
        uint8_t interleave[kMaxPlanes]; // Interleaving of this plane - number of components that are interleaved with this plane - including itself
        uint8_t offset[kMaxPlanes];     // Offset of component within sample interleave

        uint8_t bits;       // Number of LSB bits per sample
        const char* suffix; // Vooya compatible suffix for files
    };

    static const Info kPictureLayoutInfo[];
    static const Info kPictureLayoutInfoUnknown;
    static const Info& findLayoutInfo(LCEVC_ColorFormat format);

    PictureLayout(const LCEVC_PictureDesc& pictureDesc, const Info& layoutInfo);
    PictureLayout(const LCEVC_PictureDesc& pictureDesc, const Info& layoutInfo,
                  const uint32_t strides[kMaxPlanes]);

    uint32_t defaultRowStride(uint32_t plane) const;
    void generateOffsets();

    // Format and dimensions
    const Info* m_layoutInfo;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Calculated size, strides (in bytes), and offsets
    uint32_t m_rowStrides[kMaxPlanes]{0};
    uint32_t m_planeOffsets[kMaxPlanes]{0};
    uint32_t m_size = 0;
};

// Inline functions
#include "picture_layout_inl.h"

} // namespace lcevc_dec::utility

#endif
