/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "uPictureFormatDesc.h"

#include "uEnumMap.h"

#include <cassert>

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace lcevc_dec { namespace utility {
    // -------------------------------------------------------------------------

    namespace {
        struct PictureFormatInfo
        {
            PictureFormatInfo(uint32_t planeCount, uint32_t numChannels, uint8_t bitDepth,
                              uint32_t horizontalDownsample, uint32_t verticalDownsample)
                : m_planeCount(planeCount)
                , m_numChannels(numChannels)
                , m_bitDepthPerChannel(bitDepth)
                , m_horizontalDownsample(horizontalDownsample)
                , m_verticalDownsample(verticalDownsample)
            {}

            uint32_t m_planeCount;
            uint32_t m_numChannels;
            uint8_t m_bitDepthPerChannel;
            uint32_t m_horizontalDownsample;
            uint32_t m_verticalDownsample;
        };

        const PictureFormatInfo kFormatInfo[PictureFormat::Count] = {
            PictureFormatInfo(3, 1, 8, 2, 2), // YUV8Planar420
            PictureFormatInfo(3, 1, 8, 2, 1), // YUV8Planar422
            PictureFormatInfo(3, 1, 8, 1, 1), // YUV8Planar444

            PictureFormatInfo(3, 1, 10, 2, 2), // YUV10Planar420
            PictureFormatInfo(3, 1, 10, 2, 1), // YUV10Planar422
            PictureFormatInfo(3, 1, 10, 1, 1), // YUV10Planar444

            PictureFormatInfo(3, 1, 12, 2, 2), // YUV12Planar420
            PictureFormatInfo(3, 1, 12, 2, 1), // YUV12Planar422
            PictureFormatInfo(3, 1, 12, 1, 1), // YUV12Planar444

            PictureFormatInfo(3, 1, 14, 2, 2), // YUV14Planar420
            PictureFormatInfo(3, 1, 14, 2, 1), // YUV14Planar422
            PictureFormatInfo(3, 1, 14, 1, 1), // YUV14Planar444

            PictureFormatInfo(3, 1, 16, 2, 2), // YUV16Planar420
            PictureFormatInfo(3, 1, 16, 2, 1), // YUV16Planar422
            PictureFormatInfo(3, 1, 16, 1, 1), // YUV16Planar444

            PictureFormatInfo(1, 3, 8, 1, 1), // YUV8Raster420

            PictureFormatInfo(1, 1, 8, 1, 1),  // Y8Planar
            PictureFormatInfo(1, 1, 10, 1, 1), // Y10Planar
            PictureFormatInfo(1, 1, 12, 1, 1), // Y12Planar
            PictureFormatInfo(1, 1, 14, 1, 1), // Y14Planar
            PictureFormatInfo(1, 1, 16, 1, 1), // Y16Planar

            PictureFormatInfo(1, 3, 8, 1, 1), // RGB24
            PictureFormatInfo(1, 3, 8, 1, 1), // BGR24

            PictureFormatInfo(1, 4, 8, 1, 1), // RGBA32
            PictureFormatInfo(1, 4, 8, 1, 1), // BGRA32
            PictureFormatInfo(1, 4, 8, 1, 1), // ARGB32
            PictureFormatInfo(1, 4, 8, 1, 1), // ABGR32

            PictureFormatInfo(1, 1, 8, 1, 1),  // RAW8
            PictureFormatInfo(1, 1, 16, 1, 1), // RAW16
            PictureFormatInfo(1, 1, 16, 1, 1), // RAW16f
            PictureFormatInfo(1, 1, 32, 1, 1), // RAW32f

            PictureFormatInfo(1, 4, 16, 1, 1), // RGBA64

            PictureFormatInfo(1, 4, 10, 1, 1), // RGB10A2
        };

        // clang-format off
        constexpr EnumMapArr<PictureFormat::Enum, PictureFormat::Count> kPictureFormatMap(
            PictureFormat::YUV8Planar420, "yuv420p",
            PictureFormat::YUV8Planar422, "yuv422p",
            PictureFormat::YUV8Planar444, "yuv444p",
            PictureFormat::YUV10Planar420, "yuv420p10",
            PictureFormat::YUV10Planar422, "yuv422p10",
            PictureFormat::YUV10Planar444, "yuv444p10",
            PictureFormat::YUV12Planar420, "yuv420p12",
            PictureFormat::YUV12Planar422, "yuv422p12",
            PictureFormat::YUV12Planar444, "yuv444p12",
            PictureFormat::YUV14Planar420, "yuv420p14",
            PictureFormat::YUV14Planar422, "yuv422p14",
            PictureFormat::YUV14Planar444, "yuv444p14",
            PictureFormat::YUV16Planar420, "yuv420p16",
            PictureFormat::YUV16Planar422, "yuv422p16",
            PictureFormat::YUV16Planar444, "yuv444p16",
            PictureFormat::YUV8Raster420, "yuv420r",
            PictureFormat::Y8Planar, "gray",
            PictureFormat::Y10Planar, "gray10le",
            PictureFormat::Y12Planar, "gray12le",
            PictureFormat::Y14Planar, "gray14le",
            PictureFormat::Y16Planar, "gray16le",
            PictureFormat::RGB24, "rgb24",
            PictureFormat::BGR24, "bgr24",
            PictureFormat::RGBA32, "rgba32",
            PictureFormat::BGRA32, "bgra32",
            PictureFormat::ARGB32, "argb32",
            PictureFormat::ABGR32, "abgr32",
            PictureFormat::RAW8, "raw8",
            PictureFormat::RAW16, "raw16",
            PictureFormat::RAW16f, "raw16f",
            PictureFormat::RAW32f, "raw32f",
            PictureFormat::RGBA64, "rgba64",
            PictureFormat::RGB10A2, "rgb10a2");
        static_assert(kPictureFormatMap.size == kPictureFormatMap.capacity(), "kPictureFormatMap "
            "has too many or too few entries.");

        constexpr EnumMapArr<PictureFormat::Enum, PictureFormat::Count> kPictureFormatMapEPI(
            PictureFormat::YUV8Planar420, "yuv8planar420",
            PictureFormat::YUV8Planar422, "yuv8planar422",
            PictureFormat::YUV8Planar444, "yuv8planar444",
            PictureFormat::YUV10Planar420, "yuv10planar420",
            PictureFormat::YUV10Planar422, "yuv10planar422",
            PictureFormat::YUV10Planar444, "yuv10planar444",
            PictureFormat::YUV12Planar420, "yuv12planar420",
            PictureFormat::YUV12Planar422, "yuv12planar422",
            PictureFormat::YUV12Planar444, "yuv12planar444",
            PictureFormat::YUV14Planar420, "yuv14planar420",
            PictureFormat::YUV14Planar422, "yuv14planar422",
            PictureFormat::YUV14Planar444, "yuv14planar444",
            PictureFormat::YUV16Planar420, "yuv16planar420",
            PictureFormat::YUV16Planar422, "yuv16planar422",
            PictureFormat::YUV16Planar444, "yuv16planar444",
            PictureFormat::YUV8Raster420, "yuv420raster",
            PictureFormat::Y8Planar, "y8planar",
            PictureFormat::Y10Planar, "y10planar",
            PictureFormat::Y12Planar, "y12planar",
            PictureFormat::Y14Planar, "y14planar",
            PictureFormat::Y16Planar, "y16planar",
            PictureFormat::RGB24, "rgb24",
            PictureFormat::BGR24, "bgr24",
            PictureFormat::RGBA32, "rgba32",
            PictureFormat::BGRA32, "bgra32",
            PictureFormat::ARGB32, "argb32",
            PictureFormat::ABGR32, "abgr32",
            PictureFormat::RAW8, "raw8",
            PictureFormat::RAW16, "raw16",
            PictureFormat::RAW16f, "raw16f",
            PictureFormat::RAW32f, "raw32f",
            PictureFormat::RGBA64, "rgba64",
            PictureFormat::RGB10A2, "rgb10a2");
        static_assert(kPictureFormatMapEPI.size == kPictureFormatMapEPI.capacity(),
            "kPictureFormatMapEPI has too many or too few entries.");

        constexpr EnumMapArr<Colorspace::Enum, Colorspace::Count> kColorspaceMap(
            Colorspace::Auto, "auto",
            Colorspace::YCbCr_BT601, "ycbcrbt601",
            Colorspace::YCbCr_BT709, "ycbcrbt709",
            Colorspace::YCbCr_BT2020, "ycbcrbt2020",
            Colorspace::sRGB, "srgb");
        static_assert(kColorspaceMap.size == kColorspaceMap.capacity(), "kColorspaceMap has too "
            "many or too few entries.");

        constexpr EnumMapArr<PictureInterleaving::Enum, PictureInterleaving::Count> kPictureInterleavingMap(
            PictureInterleaving::None, "none",
            PictureInterleaving::NV12, "nv12");
        static_assert(kPictureInterleavingMap.size == kPictureInterleavingMap.capacity(),
            "kPictureInterleavingMap has too many or too few entries.");

        constexpr EnumMapArr<ChromaSamplingType::Enum, ChromaSamplingType::Count> kChromaSamplingTypeMap(
            ChromaSamplingType::Monochrome, "monochrome",
            ChromaSamplingType::Chroma420, "420",
            ChromaSamplingType::Chroma422, "422",
            ChromaSamplingType::Chroma444, "444");
        static_assert(kChromaSamplingTypeMap.size == kChromaSamplingTypeMap.capacity(), 
            "kPictureFormatMap has too many or too few entries.");

        // clang-format on
    } // namespace

    // -------------------------------------------------------------------------

    ChromaSamplingType::Enum ChromaSamplingType::FromPictureFormat(uint32_t format)
    {
        switch (format) {
            case PictureFormat::YUV8Planar420:
            case PictureFormat::YUV10Planar420:
            case PictureFormat::YUV12Planar420:
            case PictureFormat::YUV14Planar420:
            case PictureFormat::YUV16Planar420:
            case PictureFormat::YUV8Raster420: return Chroma420;
            case PictureFormat::YUV8Planar422:
            case PictureFormat::YUV10Planar422:
            case PictureFormat::YUV12Planar422:
            case PictureFormat::YUV14Planar422:
            case PictureFormat::YUV16Planar422: return Chroma422;
            case PictureFormat::YUV8Planar444:
            case PictureFormat::YUV10Planar444:
            case PictureFormat::YUV12Planar444:
            case PictureFormat::YUV14Planar444:
            case PictureFormat::YUV16Planar444: return Chroma444;
            default: break;
        }

        return Invalid;
    }

    bool ChromaSamplingType::ToString(const char** res, Enum type)
    {
        return kChromaSamplingTypeMap.findName(res, type, "ChromaSamplingType-ERROR");
    }

    const char* ChromaSamplingType::ToString2(Enum val) { return ToString2Helper(ToString, val); }

    // -------------------------------------------------------------------------

    BitDepthType::Enum BitDepthType::FromPictureFormat(uint32_t format)
    {
        switch (format) {
            case PictureFormat::YUV8Planar420:
            case PictureFormat::YUV8Planar422:
            case PictureFormat::YUV8Planar444:
            case PictureFormat::YUV8Raster420: return Depth8;
            case PictureFormat::YUV10Planar420:
            case PictureFormat::YUV10Planar422:
            case PictureFormat::YUV10Planar444: return Depth10;
            case PictureFormat::YUV12Planar420:
            case PictureFormat::YUV12Planar422:
            case PictureFormat::YUV12Planar444: return Depth12;
            case PictureFormat::YUV14Planar420:
            case PictureFormat::YUV14Planar422:
            case PictureFormat::YUV14Planar444: return Depth14;
            case PictureFormat::YUV16Planar420:
            case PictureFormat::YUV16Planar422:
            case PictureFormat::YUV16Planar444: return Depth16;
            default: break;
        }

        return Invalid;
    }

    uint8_t BitDepthType::ToValue(Enum type)
    {
        switch (type) {
            case BitDepthType::Depth8: return 8;
            case BitDepthType::Depth10: return 10;
            case BitDepthType::Depth12: return 12;
            case BitDepthType::Depth14: return 14;
            case BitDepthType::Depth16: return 16;
            default: break;
        }

        // Prevent silent failure without runtime error
        VNAssert(false);
        return 8;
    }

    BitDepthType::Enum BitDepthType::FromValue(uint8_t value)
    {
        switch (value) {
            case 8: return Depth8;
            case 10: return Depth10;
            case 12: return Depth12;
            case 14: return Depth14;
            case 16: return Depth16;
        }

        return Invalid;
    }

    // -------------------------------------------------------------------------

    bool PictureFormat::FromString(Enum& res, const std::string& str)
    {
        return kPictureFormatMap.findEnum(res, str, PictureFormat::Invalid);
    }

    PictureFormat::Enum PictureFormat::FromString2(const std::string& str)
    {
        return FromString2Helper(FromString, str);
    }

    bool PictureFormat::ToString(const char** res, Enum type)
    {
        return kPictureFormatMap.findName(res, type, "PictureFormat-ERROR");
    }

    const char* PictureFormat::ToString2(Enum val) { return ToString2Helper(ToString, val); }

    // @todo (rob): These functions are currently a necessary evil to translate from the
    //				FFmpeg inspired formats to EPI legacy formats. EPI API changes should
    //				work to eliminate this.
    bool PictureFormat::ToStringEPI(const char** res, Enum type)
    {
        return kPictureFormatMapEPI.findName(res, type, "PictureFormat-ERROR");
    }

    const char* PictureFormat::ToStringEPI2(Enum val) { return ToString2Helper(ToStringEPI, val); }

    bool PictureFormat::FromStringEPI(Enum& res, const std::string& str)
    {
        return kPictureFormatMapEPI.findEnum(res, str, PictureFormat::Invalid);
    }

    PictureFormat::Enum PictureFormat::FromStringEPI2(const std::string& str)
    {
        return FromString2Helper(FromStringEPI, str);
    }

    PictureFormat::Enum PictureFormat::FromBitDepthChroma(BitDepthType::Enum depth,
                                                          ChromaSamplingType::Enum chroma)
    {
        uint32_t chromaIndex = 0;
        PictureFormat::Enum res = PictureFormat::Invalid;

        switch (chroma) {
            case ChromaSamplingType::Chroma420: chromaIndex = 0; break;
            case ChromaSamplingType::Chroma422: chromaIndex = 1; break;
            case ChromaSamplingType::Chroma444: chromaIndex = 2; break;
            case ChromaSamplingType::Monochrome: chromaIndex = 3; break;
            default: return res;
        }

        if (chromaIndex < 3) {
            res = static_cast<PictureFormat::Enum>((depth * 3) + chromaIndex);
            VNAssert(res >= YUV8Planar420 && res <= YUV8Raster420);
        } else {
            res = static_cast<PictureFormat::Enum>(PictureFormat::Y8Planar + depth);
            VNAssert(res >= Y8Planar && res <= Y14Planar);
        }

        return res;
    }

    bool PictureFormat::IsRaw(Enum format)
    {
        switch (format) {
            case RAW8:
            case RAW16:
            case RAW16f:
            case RAW32f: return true;
            default:;
        }

        return false;
    }

    bool PictureFormat::IsRGB(Enum format)
    {
        switch (format) {
            case PictureFormat::RGB24:
            case PictureFormat::BGR24:
            case PictureFormat::RGBA32:
            case PictureFormat::BGRA32:
            case PictureFormat::ARGB32:
            case PictureFormat::ABGR32:
            case PictureFormat::RGB10A2: return true;
            default:;
        }

        return false;
    }

    bool PictureFormat::IsYUV(Enum format)
    {
        switch (format) {
            case PictureFormat::YUV8Planar420:
            case PictureFormat::YUV8Planar422:
            case PictureFormat::YUV8Planar444:
            case PictureFormat::YUV10Planar420:
            case PictureFormat::YUV10Planar422:
            case PictureFormat::YUV10Planar444:
            case PictureFormat::YUV12Planar420:
            case PictureFormat::YUV12Planar422:
            case PictureFormat::YUV12Planar444:
            case PictureFormat::YUV14Planar420:
            case PictureFormat::YUV14Planar422:
            case PictureFormat::YUV14Planar444:
            case PictureFormat::YUV16Planar420:
            case PictureFormat::YUV16Planar422:
            case PictureFormat::YUV16Planar444:
            case PictureFormat::YUV8Raster420: return true;
            default:;
        }
        return false;
    }

    bool PictureFormat::IsMonochrome(Enum format)
    {
        switch (format) {
            case PictureFormat::Y8Planar:
            case PictureFormat::Y10Planar:
            case PictureFormat::Y12Planar:
            case PictureFormat::Y14Planar:
            case PictureFormat::Y16Planar: return true;
            default:;
        }
        return false;
    }

    uint8_t PictureFormat::BitDepth(Enum format)
    {
        switch (format) {
            // Special case for formats where different channels are at different bitdepths.
            case PictureFormat::RGB10A2: return 32;
            default:;
        }
        return kFormatInfo[format].m_bitDepthPerChannel * kFormatInfo[format].m_numChannels;
    }

    uint8_t PictureFormat::BitDepthPerChannel(Enum format)
    {
        if (format >= PictureFormat::Count) {
            return BitDepthType::Invalid;
        }

        return kFormatInfo[format].m_bitDepthPerChannel;
    }

    uint8_t PictureFormat::NumPlanes(Enum format, PictureInterleaving::Enum ilv)
    {
        if (ilv == PictureInterleaving::NV12) {
            return 2;
        }

        return kFormatInfo[format].m_planeCount;
    }

    // -------------------------------------------------------------------------

    bool Colorspace::FromString(Enum& res, const std::string& str)
    {
        return kColorspaceMap.findEnum(res, str, Colorspace::Invalid);
    }

    Colorspace::Enum Colorspace::FromString2(const std::string& str)
    {
        return FromString2Helper(FromString, str);
    }

    bool Colorspace::ToString(const char** res, Enum type)
    {
        return kColorspaceMap.findName(res, type, "Colorspace-ERROR");
    }

    const char* Colorspace::ToString2(Enum val) { return ToString2Helper(ToString, val); }

    Colorspace::Enum Colorspace::AutoDetectFromFormat(const PictureFormat::Enum format)
    {
        switch (format) {
            case PictureFormat::YUV8Planar420:
            case PictureFormat::YUV8Planar422:
            case PictureFormat::YUV8Planar444:
            case PictureFormat::YUV10Planar420:
            case PictureFormat::YUV10Planar422:
            case PictureFormat::YUV10Planar444:
            case PictureFormat::YUV12Planar420:
            case PictureFormat::YUV12Planar422:
            case PictureFormat::YUV12Planar444:
            case PictureFormat::YUV14Planar420:
            case PictureFormat::YUV14Planar422:
            case PictureFormat::YUV14Planar444:
            case PictureFormat::YUV16Planar420:
            case PictureFormat::YUV16Planar422:
            case PictureFormat::YUV16Planar444:
            case PictureFormat::YUV8Raster420:
            case PictureFormat::Y8Planar:
            case PictureFormat::Y10Planar:
            case PictureFormat::Y12Planar:
            case PictureFormat::Y14Planar:
            case PictureFormat::Y16Planar:
                return Colorspace::YCbCr_BT601;
                // return Colorspace::YCbCr_BT709;
                // return Colorspace::YCbCr_BT2020;

            case PictureFormat::RGB24:
            case PictureFormat::BGR24:
            case PictureFormat::RGBA32:
            case PictureFormat::BGRA32:
            case PictureFormat::ARGB32:
            case PictureFormat::ABGR32: return Colorspace::sRGB;
            default:;
        }

        return Colorspace::Invalid;
    }

    // -------------------------------------------------------------------------

    bool PictureInterleaving::FromString(Enum& res, const std::string& str)
    {
        return kPictureInterleavingMap.findEnum(res, str, PictureInterleaving::Invalid);
    }

    PictureInterleaving::Enum PictureInterleaving::FromString2(const std::string& str)
    {
        return FromString2Helper(FromString, str);
    }

    bool PictureInterleaving::ToString(const char** res, Enum type)
    {
        return kPictureInterleavingMap.findName(res, type, "PictureInterleaving-ERROR");
    }

    const char* PictureInterleaving::ToString2(Enum val) { return ToString2Helper(ToString, val); }

    // -------------------------------------------------------------------------

    PictureFormatDesc::PlaneDesc::PlaneDesc(uint32_t width, uint32_t height, uint32_t stridePixels,
                                            uint32_t strideBytes, uint32_t pixelStride)
        : m_width(width)
        , m_height(height)
        , m_stridePixels(stridePixels)
        , m_strideBytes(strideBytes)
        , m_pixelStride(pixelStride)
    {}

    // -------------------------------------------------------------------------

    PictureFormatDesc::PictureFormatDesc()
        : m_format(PictureFormat::Invalid)
        , m_interleaving(PictureInterleaving::Invalid)
        , m_colorspace(Colorspace::Invalid)
        , m_planeCount(0)
        , m_byteSize(0)
        , m_bitDepth(0)
        , m_bitDepthContainer(0)
        , m_numChannels(0)
    {}

    bool PictureFormatDesc::Initialise(PictureFormat::Enum format, uint32_t width, uint32_t height,
                                       PictureInterleaving::Enum interleaving,
                                       Colorspace::Enum colorspace, uint32_t bitDepthPerChannel,
                                       const uint32_t planeStridesPixels[kMaxNumPlanes] /*= nullptr*/)
    {
        if (format >= PictureFormat::Count) {
            return false;
        }

        const PictureFormatInfo& info = kFormatInfo[format];

        // User may pass in bit-depth which matches that of the format or is less
        // than the default bit-depth for raw formats.
        //
        // e.g. u10 in a u16 would set bitDepth to 10.
        const bool bIsBitDepthSettableForFormat =
            PictureFormat::IsRaw(format) && (format != PictureFormat::RAW8);
        const bool bBitDepthDiffersFromDefault = info.m_bitDepthPerChannel != bitDepthPerChannel;

        if (!bitDepthPerChannel) {
            bitDepthPerChannel = info.m_bitDepthPerChannel;
        } else if ((!bIsBitDepthSettableForFormat && bBitDepthDiffersFromDefault) ||
                   (bitDepthPerChannel > info.m_bitDepthPerChannel)) {
            return false;
        }

        m_format = format;
        m_interleaving = interleaving;
        m_colorspace = (colorspace == Colorspace::Auto) ? Colorspace::AutoDetectFromFormat(format)
                                                        : colorspace;
        m_planeCount = PictureFormat::NumPlanes(format, m_interleaving);
        assert(m_planeCount <= kMaxNumPlanes);

        m_byteSize = 0;
        m_bitDepth = PictureFormat::BitDepth(format);
        m_bitDepthContainer = (m_bitDepth + 7) & ~7;
        m_numChannels = info.m_numChannels;

        // For a "UVUVUV" plane, a pixel is "UV", whereas a sample is "U" or "V". If no unit is
        // specified, assume pixels, rather than samples.
        const uint8_t bytesPerSample = GetByteDepth();

        for (uint32_t planeIndex = 0; planeIndex < m_planeCount; ++planeIndex) {
            const uint32_t samplesPerPixel =
                ((planeIndex != 0) && (m_interleaving == PictureInterleaving::NV12)) ? 2 : 1;
            const uint32_t planeWidth = (planeIndex == 0 || (info.m_horizontalDownsample == 1))
                                            ? width
                                            : ((width + 1) / info.m_horizontalDownsample);
            const uint32_t planeHeight = (planeIndex == 0 || (info.m_verticalDownsample == 1))
                                             ? height
                                             : ((height + 1) / info.m_verticalDownsample);
            const uint32_t pixelsPerRow =
                (planeStridesPixels ? planeStridesPixels[planeIndex] : planeWidth);
            m_planeDesc[planeIndex] =
                PlaneDesc(planeWidth, planeHeight, pixelsPerRow,
                          bytesPerSample * samplesPerPixel * pixelsPerRow, samplesPerPixel);
            m_byteSize += m_planeDesc[planeIndex].m_strideBytes * m_planeDesc[planeIndex].m_height;
        }
        return true;
    }

    void PictureFormatDesc::SetDimensions(uint32_t width, uint32_t height)
    {
        Initialise(m_format, width, height, m_interleaving, m_colorspace, m_bitDepth);
    }

    void PictureFormatDesc::SetPlaneStrides(uint32_t planeIndex, uint32_t bytesPerRow, uint32_t bytesPerPixel)
    {
        assert(planeIndex < kMaxNumPlanes);

        // Remember: For an interleaved plane, like "UVUVUV", a "pixel" is "UV", while a "sample"
        // is "U" or "V".
        const uint32_t samplesPerPixel =
            ((planeIndex != 0) && (m_interleaving == PictureInterleaving::NV12)) ? 2 : 1;

        // Subtract contribution to byte size from the current stride, and add the new one later.
        m_byteSize -= m_planeDesc[planeIndex].m_strideBytes * m_planeDesc[planeIndex].m_height;

        m_planeDesc[planeIndex].m_strideBytes = bytesPerRow;
        m_planeDesc[planeIndex].m_stridePixels = (bytesPerPixel ? bytesPerRow / bytesPerPixel : 0);
        m_planeDesc[planeIndex].m_pixelStride = samplesPerPixel;

        m_byteSize += m_planeDesc[planeIndex].m_strideBytes * m_planeDesc[planeIndex].m_height;
    }

    uint32_t PictureFormatDesc::GetWidth() const { return GetPlaneWidth(0); }

    uint32_t PictureFormatDesc::GetHeight() const { return GetPlaneHeight(0); }

    uint32_t* PictureFormatDesc::GetWidthPtr() { return GetPlaneWidthPtr(0); }

    uint32_t* PictureFormatDesc::GetHeightPtr() { return GetPlaneHeightPtr(0); }

    PictureFormat::Enum PictureFormatDesc::GetFormat() const { return m_format; }

    PictureInterleaving::Enum PictureFormatDesc::GetInterleaving() const { return m_interleaving; }

    Colorspace::Enum PictureFormatDesc::GetColorspace() const { return m_colorspace; }

    uint8_t PictureFormatDesc::GetBitDepth() const { return m_bitDepth; }

    uint8_t PictureFormatDesc::GetBitDepthPerPixel() const
    {
        return PictureFormat::BitDepthPerChannel(m_format);
    }

    uint8_t PictureFormatDesc::GetBitDepthContainer() const { return m_bitDepthContainer; }

    uint8_t PictureFormatDesc::GetByteDepth() const { return m_bitDepthContainer >> 3; }

    uint8_t PictureFormatDesc::GetNumChannels() const { return m_numChannels; }

    uint32_t PictureFormatDesc::GetMemorySize() const { return m_byteSize; }

    uint32_t PictureFormatDesc::GetPlaneCount() const { return m_planeCount; }

    uint32_t PictureFormatDesc::GetPlaneWidth(uint32_t planeIndex) const
    {
        assert(planeIndex < kMaxNumPlanes);
        return m_planeDesc[planeIndex].m_width;
    }

    uint32_t PictureFormatDesc::GetPlaneHeight(uint32_t planeIndex) const
    {
        assert(planeIndex < kMaxNumPlanes);
        return m_planeDesc[planeIndex].m_height;
    }

    uint32_t PictureFormatDesc::GetPlaneWidthBytes(uint32_t planeIndex) const
    {
        // This may differ from PlaneStrideBytes if there's padding at the end of rows, or if
        // you have adjacent interlace-fields (where the width may be half the stride).
        return GetPlaneWidth(planeIndex) * GetPlaneBytesPerPixel(planeIndex);
    }

    uint32_t* PictureFormatDesc::GetPlaneWidthPtr(uint32_t planeIndex)
    {
        assert(planeIndex < kMaxNumPlanes);
        return &m_planeDesc[planeIndex].m_width;
    }

    uint32_t* PictureFormatDesc::GetPlaneHeightPtr(uint32_t planeIndex)
    {
        assert(planeIndex < kMaxNumPlanes);
        return &m_planeDesc[planeIndex].m_height;
    }

    uint32_t PictureFormatDesc::GetPlaneStrideBytes(uint32_t planeIndex) const
    {
        assert(planeIndex < kMaxNumPlanes);
        return m_planeDesc[planeIndex].m_strideBytes;
    }

    uint32_t PictureFormatDesc::GetPlaneStridePixels(uint32_t planeIndex) const
    {
        assert(planeIndex < kMaxNumPlanes);
        return m_planeDesc[planeIndex].m_stridePixels;
    }

    uint32_t PictureFormatDesc::GetPlanePixelStride(uint32_t planeIndex) const
    {
        assert(planeIndex < kMaxNumPlanes);
        return m_planeDesc[planeIndex].m_pixelStride;
    }

    uint32_t PictureFormatDesc::GetPlaneBytesPerPixel(uint32_t planeIndex) const
    {
        assert(planeIndex < kMaxNumPlanes);
        return GetPlanePixelStride(planeIndex) * GetByteDepth();
    }

    uint32_t PictureFormatDesc::GetPlaneMemorySize(uint32_t planeIndex) const
    {
        assert(planeIndex < kMaxNumPlanes);
        const PlaneDesc& desc = m_planeDesc[planeIndex];
        return (desc.m_strideBytes * desc.m_height);
    }

    void PictureFormatDesc::GetPlanePointers(uint8_t* memoryPtr, uint8_t** planePtrs,
                                             uint32_t* planePixelStrides) const
    {
        GetPlanePointers((const uint8_t*)memoryPtr, (const uint8_t**)planePtrs, planePixelStrides);
    }

    void PictureFormatDesc::GetPlanePointers(const uint8_t* memoryPtr, const uint8_t** planePtrs,
                                             uint32_t* planePixelStrides) const
    {
        // Assumes that the out arrays are the correct size.
        if (m_interleaving == PictureInterleaving::None) {
            for (uint32_t planeIndex = 0; planeIndex < m_planeCount; ++planeIndex) {
                planePtrs[planeIndex] = memoryPtr;

                if (planePixelStrides) {
                    planePixelStrides[planeIndex] = GetPlaneStridePixels(planeIndex);
                }

                memoryPtr += GetPlaneMemorySize(planeIndex);
            }
        } else if (m_interleaving == PictureInterleaving::NV12) {
            // NV12 makes the assumption that the second plane is a fully contiguous block of
            // memory and that the user will know how to interpret the behaviour.
            planePtrs[0] = memoryPtr;
            planePtrs[1] = memoryPtr + GetPlaneMemorySize(0);
            planePtrs[2] = nullptr;

            if (planePixelStrides) {
                planePixelStrides[0] = planePixelStrides[1] = GetPlaneStridePixels(0);
                planePixelStrides[2] = 0;
            }
        }
    }

    bool ChromaSamplingType::GetShifters(ChromaSamplingType::Enum chromaType, int32_t& shiftWidthC,
                                         int32_t& shiftHeightC)
    {
        const int32_t horizontal = GetHorizontalShift(chromaType);
        const int32_t vertical = GetVerticalShift(chromaType);
        if (horizontal < 0 || vertical < 0) {
            return false;
        }
        shiftWidthC = horizontal;
        shiftHeightC = vertical;
        return true;
    }

    int32_t ChromaSamplingType::GetHorizontalShift(uint32_t format)
    {
        const ChromaSamplingType::Enum chromaType = FromPictureFormat(format);
        return GetHorizontalShift(chromaType);
    }

    int32_t ChromaSamplingType::GetVerticalShift(uint32_t format)
    {
        const ChromaSamplingType::Enum chromaType = FromPictureFormat(format);
        return GetVerticalShift(chromaType);
    }

    int32_t ChromaSamplingType::GetHorizontalShift(ChromaSamplingType::Enum chromaType)
    {
        switch (chromaType) {
            case ChromaSamplingType::Chroma420:
            case ChromaSamplingType::Chroma422: return 1;
            case ChromaSamplingType::Monochrome:
            case ChromaSamplingType::Chroma444: return 0;
            case ChromaSamplingType::Invalid: break;
        }
        return -1;
    }

    int32_t ChromaSamplingType::GetVerticalShift(ChromaSamplingType::Enum chromaType)
    {
        switch (chromaType) {
            case ChromaSamplingType::Chroma420: return 1;
            case ChromaSamplingType::Chroma422:
            case ChromaSamplingType::Monochrome:
            case ChromaSamplingType::Chroma444: return 0;
            case ChromaSamplingType::Invalid: break;
        }
        return -1;
    }
    // -------------------------------------------------------------------------
}} // namespace lcevc_dec::utility
