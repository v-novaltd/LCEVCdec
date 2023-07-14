/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

// This file primarily serves as a way to convert between public-facing API constants/types and
// internal ones. Consequently, do NOT add `#include "lcevc_dec.h"` to this header: the idea is
// that other files access lcevc_dec.h THROUGH these functions, enums, etc.

#ifndef VN_API_INTERFACE_H_
#define VN_API_INTERFACE_H_

#include "handle.h"
#include "uPictureFormatDesc.h"

// - Forward declarations (api) -------------------------------------------------------------------

struct LCEVC_DecodeInformation;
struct LCEVC_HDRStaticInfo;
struct LCEVC_PictureDesc;
struct LCEVC_PictureBufferDesc;
struct LCEVC_PicturePlaneDesc;

// - Forward declarations (internal) --------------------------------------------------------------

namespace lcevc_dec::decoder {

class AccelBuffer;
class Decoder;
class Picture;
} // namespace lcevc_dec::decoder

// - Types (internal) -----------------------------------------------------------------------------

struct perseus_decoder_stream;

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

// Replicates LCEVC_ColorRange
enum class ColorRange
{
    Unknown = 0,
    Full = 1,
    Limited = 2
};

ColorRange fromLCEVCColorRange(int32_t lcevcColorRange);
int32_t toLCEVCColorRange(ColorRange colorRange);
lcevc_dec::utility::Colorspace::Enum fromLCEVCColorStandard(int32_t lcevcColorStandard);
int32_t toLCEVCColorStandard(lcevc_dec::utility::Colorspace::Enum colorspace);

// Replicates LCEVC_ColorTransfer
enum class ColorTransfer
{
    Unknown = 0,
    Linear = 1,
    SDR = 2,
    ST2084 = 3,
    HLG = 4,
};

ColorTransfer fromLCEVCColorTransfer(int32_t lcevcColorRange);
int32_t toLCEVCColorTransfer(ColorTransfer colorRange);

// Replicates LCEVC_HDRStaticInfo
struct HDRStaticInfo
{
    uint16_t displayPrimariesX0;
    uint16_t displayPrimariesY0;
    uint16_t displayPrimariesX1;
    uint16_t displayPrimariesY1;
    uint16_t displayPrimariesX2;
    uint16_t displayPrimariesY2;
    uint16_t whitePointX;
    uint16_t whitePointY;
    uint16_t maxDisplayMasteringLuminance;
    uint16_t minDisplayMasteringLuminance;
    uint16_t maxContentLightLevel;
    uint16_t maxFrameAverageLightLevel;
};
bool equals(const LCEVC_HDRStaticInfo& lhs, const LCEVC_HDRStaticInfo& rhs);

// Replicates LCEVC_DecodeInformation
struct DecodeInformation
{
    DecodeInformation(int64_t timestampIn)
        : timestamp(timestampIn)
    {}
    int64_t timestamp;
    bool hasBase = false;
    bool hasEnhancement = false;
    bool skipped = false;
    bool enhanced = false;

    uint32_t baseWidth = 0;
    uint32_t baseHeight = 0;
    uint8_t baseBitdepth = 0;

    void* userData = nullptr;
};

// To store sample_aspect_ratio_num and sample_aspect_ratio_den
struct AspectRatio
{
    uint32_t numerator;
    uint32_t denominator;
};

// Helpers for LCEVC_ColorFormat
lcevc_dec::utility::PictureFormat::Enum fromLCEVCDescColorFormat(int32_t descColorFormat);
lcevc_dec::utility::PictureInterleaving::Enum fromLCEVCDescInterleaving(int32_t descColorFormat);
int32_t toLCEVCDescColorFormat(lcevc_dec::utility::PictureFormat::Enum format,
                               lcevc_dec::utility::PictureInterleaving::Enum ilv);
// Bitdepth is "bits per channel" where R, G, and B are channels, and so are Y, U, and V.
uint32_t bitdepthFromLCEVCDescColorFormat(int32_t descColorFormat);

// Helpers for LCEVC_PictureDesc
bool equals(const LCEVC_PictureDesc& lhs, const LCEVC_PictureDesc& rhs);
bool coreFormatToLCEVCPictureDesc(const perseus_decoder_stream& coreFormat, LCEVC_PictureDesc& picDescOut);

// Replicates LCEVC_Access
enum class Access
{
    Unknown = 0,
    Read = 1,
    Modify = 2,
    Write = 3,
};

// Replicates LCEVC_PictureBufferDesc
struct PictureBufferDesc
{
    uint8_t* data = nullptr;
    uint32_t byteSize = 0;
    Handle<AccelBuffer> accelBuffer = kInvalidHandle;
    int32_t access = 0;
};

void fromLCEVCPictureBufferDesc(const LCEVC_PictureBufferDesc& lcevcPictureBufferDesc,
                                PictureBufferDesc& pictureBufferDescOut);
void toLCEVCPictureBufferDesc(const PictureBufferDesc& pictureBufferDesc,
                              LCEVC_PictureBufferDesc& lcevcPictureBufferDescOut);

// Replicates LCEVC_PicturePlaneDesc
struct PicturePlaneDesc
{
    uint8_t* firstSample = nullptr;
    uint32_t sampleByteStride = 0;
    uint32_t rowByteStride = 0;
    uint32_t sampleByteSize = 0;
    uint32_t rowByteSize = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

void fromLCEVCPicturePlaneDesc(const LCEVC_PicturePlaneDesc& lcevcPicturePlaneDesc,
                               PicturePlaneDesc& picturePlaneDescOut);
void toLCEVCPicturePlaneDesc(const PicturePlaneDesc& picturePlaneDesc,
                             LCEVC_PicturePlaneDesc& lcevcPicturePlaneDescOut);

// Helpers for perseus_interleaving and perseus_bitdepth
bool toCoreInterleaving(const lcevc_dec::utility::PictureFormat::Enum& format,
                        const lcevc_dec::utility::PictureInterleaving::Enum& interleaving,
                        int32_t& interleavingOut);
bool toCoreBitdepth(uint8_t val, int32_t& out);
bool fromCoreBitdepth(const int32_t& val, uint8_t& out);

// Closely mirrors LCEVC_EventCallback (though not exactly: enums are replaced with ints)
using EventCallback = void (*)(Handle<Decoder>, int32_t, Handle<Picture>,
                               const LCEVC_DecodeInformation*, const uint8_t*, uint32_t, void*);

} // namespace lcevc_dec::decoder

#endif // VN_API_INTERFACE_H_
