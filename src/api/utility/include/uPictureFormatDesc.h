/* Copyright (c) V-Nova International Limited 2022-2023. All rights reserved. */
#pragma once

#include "uPlatform.h"

#include <string>

namespace lcevc_dec::api_utility {

// -------------------------------------------------------------------------

struct ChromaSamplingType
{
    enum Enum
    {
        Monochrome = 0,
        Chroma420,
        Chroma422,
        Chroma444,

        Invalid,
        Count = Invalid
    };

    static Enum FromPictureFormat(uint32_t format);

    static bool ToString(const char** res, Enum type);
    static const char* ToString2(Enum val);
    static bool GetShifters(ChromaSamplingType::Enum chromaType, int32_t& shiftWidthC, int32_t& shiftHeightC);
    static int32_t GetHorizontalShift(ChromaSamplingType::Enum chromaType);
    static int32_t GetVerticalShift(ChromaSamplingType::Enum chromaType);
    static int32_t GetHorizontalShift(uint32_t format);
    static int32_t GetVerticalShift(uint32_t format);
};

// -------------------------------------------------------------------------

struct BitDepthType
{
    enum Enum
    {
        Depth8 = 0,
        Depth10,
        Depth12,
        Depth14,
        Depth16,

        Invalid,
        Count = Invalid
    };

    static Enum FromPictureFormat(uint32_t format);
    static uint8_t ToValue(Enum type);
    static Enum FromValue(uint8_t value);
};

// -------------------------------------------------------------------------

struct PictureInterleaving
{
    enum Enum
    {
        None,
        NV12,

        Invalid,
        Count = Invalid
    };

    static bool FromString(Enum& res, const std::string& str);
    static Enum FromString2(const std::string& str);
    static bool ToString(const char** res, Enum type);
    static const char* ToString2(Enum val);
};

// -------------------------------------------------------------------------

struct PictureFormat
{
    // Note: this is used to index into the arrays in dec_il_gl.cpp (and may be used elsewhere)
    // so don't reorder without an extremely good reason.
    enum Enum
    {
        // Planar YUV formats
        YUV8Planar420 = 0,
        YUV8Planar422,
        YUV8Planar444,
        YUV10Planar420,
        YUV10Planar422,
        YUV10Planar444,
        YUV12Planar420,
        YUV12Planar422,
        YUV12Planar444,
        YUV14Planar420,
        YUV14Planar422,
        YUV14Planar444,
        YUV16Planar420,
        YUV16Planar422,
        YUV16Planar444,

        // Raster YUV formats
        // GPU sampling is YUV 4:4:4 with UV quads assuming same values therefore data are 420, CPU sampling is hardware dependent and signalled separately
        // see https://registry.khronos.org/OpenGL/extensions/EXT/EXT_YUV_target.txt
        YUV8Raster420,

        // Monochrome planar formats
        Y8Planar,
        Y10Planar,
        Y12Planar,
        Y14Planar,
        Y16Planar,

        // Interleaved RGB formats
        RGB24,
        BGR24,
        RGBA32,
        BGRA32,
        ABGR32,
        ARGB32,

        // Raw Formats
        RAW8,
        RAW16,
        RAW16f,
        RAW32f,

        // 4 components - 16 bits each
        RGBA64,

        // Interleaved RGB with 10bit R, G, and B components
        RGB10A2,

        Invalid,
        Count = Invalid
    };

    static bool FromString(Enum& res, const std::string& str);
    static Enum FromString2(const std::string& str);
    static bool ToString(const char** res, Enum type);
    static const char* ToString2(Enum val);

    static bool FromStringEPI(Enum& res, const std::string& str);
    static Enum FromStringEPI2(const std::string& str);
    static bool ToStringEPI(const char** res, Enum type);
    static const char* ToStringEPI2(Enum val);

    static Enum FromBitDepthChroma(BitDepthType::Enum depth, ChromaSamplingType::Enum chroma);

    static bool IsRaw(Enum val);
    static bool IsRGB(Enum val);
    static bool IsYUV(Enum val);
    static bool IsMonochrome(Enum val);
    static uint8_t BitDepth(Enum format);
    static uint8_t BitDepthPerChannel(Enum format);
    static uint8_t NumPlanes(Enum format, PictureInterleaving::Enum ilv);
};

// -------------------------------------------------------------------------

struct Colorspace
{
    enum Enum
    {
        Auto,

        YCbCr_BT601,
        YCbCr_BT709,
        YCbCr_BT2020,

        sRGB,

        Invalid,
        Count = Invalid
    };

    static bool FromString(Enum& res, const std::string& str);
    static Enum FromString2(const std::string& str);
    static bool ToString(const char** res, Enum type);
    static const char* ToString2(Enum val);

    static Enum AutoDetectFromFormat(const PictureFormat::Enum format);
};

// -------------------------------------------------------------------------

class PictureFormatDesc
{
public:
    // 4 allows for an alpha plane. None of the formats in PictureFormat::Enum actually have an
    // alpha plane, so this may not be needed at the moment.
    static constexpr uint8_t kMaxNumPlanes = 4;

    PictureFormatDesc();

    // Must at least provide the format
    bool Initialise(PictureFormat::Enum format, uint32_t width = 0, uint32_t height = 0,
                    PictureInterleaving::Enum interleaving = PictureInterleaving::None,
                    Colorspace::Enum colorspace = Colorspace::Auto, uint32_t bitDepthPerChannel = 0,
                    const uint32_t planeStrideBytes[kMaxNumPlanes] = nullptr);

    void SetDimensions(uint32_t width, uint32_t height);
    void SetPlaneStrides(uint32_t planeIndex, uint32_t bytesPerRow, uint32_t bytesPerPixel);

    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    uint32_t* GetWidthPtr();
    uint32_t* GetHeightPtr();
    PictureFormat::Enum GetFormat() const;
    PictureInterleaving::Enum GetInterleaving() const;
    Colorspace::Enum GetColorspace() const;
    uint8_t GetBitDepth() const;
    uint8_t GetBitDepthPerPixel() const;
    uint8_t GetBitDepthContainer() const;
    uint8_t GetByteDepth() const;
    uint8_t GetNumChannels() const;
    uint32_t GetMemorySize() const;
    uint32_t GetPlaneCount() const;
    uint32_t GetPlaneWidth(uint32_t planeIndex) const;
    uint32_t GetPlaneWidthBytes(uint32_t planeIndex) const;
    uint32_t GetPlaneHeight(uint32_t planeIndex) const;
    uint32_t* GetPlaneWidthPtr(uint32_t planeIndex);
    uint32_t* GetPlaneHeightPtr(uint32_t planeIndex);
    uint32_t GetPlaneStrideBytes(uint32_t planeIndex) const;
    uint32_t GetPlaneStridePixels(uint32_t planeIndex) const;
    uint32_t GetPlanePixelStride(uint32_t planeIndex) const; //< Amount of pixels to step for each pixel read/write.
    uint32_t GetPlaneBytesPerPixel(uint32_t planeIndex) const;
    uint32_t GetPlaneMemorySize(uint32_t planeIndex) const;
    void GetPlanePointers(uint8_t* memoryPtr, uint8_t** planePtrs, uint32_t* planeStridePixels) const;
    void GetPlanePointers(const uint8_t* memoryPtr, const uint8_t** planePtrs,
                          uint32_t* planeStridePixels) const;

private:
    struct PlaneDesc
    {
        PlaneDesc(uint32_t width = 0, uint32_t height = 0, uint32_t stridePixels = 0,
                  uint32_t strideBytes = 0, uint32_t pixelStride = 0);

        uint32_t m_width;
        uint32_t m_height;
        uint32_t m_stridePixels; // Line size in pixels, where "UVUVUV" is considered 3 pixels. This INCLUDES padding (unlike m_width).
        uint32_t m_strideBytes; // Line size in bytes of stride. (stridePixels * pixelStride * channeldepth).
        uint32_t m_pixelStride; // Means number of pixels to step over when inspecting pixel dat. Interleaving impacts this: it's 2 for "UVUVUV", and 4 for "YUYVYUYV".
    };

    PictureFormat::Enum m_format;
    PictureInterleaving::Enum m_interleaving;
    Colorspace::Enum m_colorspace;
    uint32_t m_planeCount;
    PlaneDesc m_planeDesc[kMaxNumPlanes];
    uint32_t m_byteSize;
    uint8_t m_bitDepth;
    uint8_t m_bitDepthContainer;
    uint8_t m_numChannels;
};

// -------------------------------------------------------------------------

using YUVFormat = PictureFormat;
using YUVInterleaving = PictureInterleaving;
using YUVDesc = PictureFormatDesc;

// -------------------------------------------------------------------------

} // namespace lcevc_dec::api_utility
