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

// This file primarily serves as a way to convert between public-facing API constants/types and
// internal ones. Consequently, do NOT add `#include "lcevc_dec.h"` to this header: the idea is
// that other files access lcevc_dec.h THROUGH these functions, enums, etc.

#ifndef VN_API_INTERFACE_H_
#define VN_API_INTERFACE_H_

#include "handle.h"

#include <LCEVC/lcevc_dec.h>

#include <cstdint>

// - Forward declarations (external) --------------------------------------------------------------

struct LCEVC_DecodeInformation;
struct LCEVC_HDRStaticInfo;
struct LCEVC_PictureDesc;
struct LCEVC_PictureBufferDesc;
struct LCEVC_PicturePlaneDesc;

struct perseus_decoder_stream;

// - Forward declarations (internal) --------------------------------------------------------------

namespace lcevc_dec::decoder {

class AccelBuffer;
class Decoder;
class Picture;
} // namespace lcevc_dec::decoder

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

bool equals(const LCEVC_HDRStaticInfo& lhs, const LCEVC_HDRStaticInfo& rhs);

// Replicates LCEVC_DecodeInformation
struct DecodeInformation
{
    explicit constexpr DecodeInformation(int64_t timestampIn, bool skippedIn)
        : timestamp(timestampIn)
        , skipped(skippedIn)
    {}
    DecodeInformation(const Picture& base, bool lcevcAvailable, bool shouldPassthrough, bool shouldFail);
    int64_t timestamp;
    bool hasBase = false;
    bool hasEnhancement = false;
    bool skipped;
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
bool equals(const LCEVC_PictureBufferDesc& lhs, const LCEVC_PictureBufferDesc& rhs);
void fromLCEVCPictureBufferDesc(const LCEVC_PictureBufferDesc& lcevcPictureBufferDesc,
                                PictureBufferDesc& pictureBufferDescOut);
void toLCEVCPictureBufferDesc(const PictureBufferDesc& pictureBufferDesc,
                              LCEVC_PictureBufferDesc& lcevcPictureBufferDescOut);

// Replicates LCEVC_PicturePlaneDesc
struct PicturePlaneDesc
{
    uint8_t* firstSample = nullptr;
    uint32_t rowByteStride = 0;
};
bool equals(const LCEVC_PicturePlaneDesc& lhs, const LCEVC_PicturePlaneDesc& rhs);
void fromLCEVCPicturePlaneDesc(const LCEVC_PicturePlaneDesc& lcevcPicturePlaneDesc,
                               PicturePlaneDesc& picturePlaneDescOut);
void toLCEVCPicturePlaneDesc(const PicturePlaneDesc& picturePlaneDesc,
                             LCEVC_PicturePlaneDesc& lcevcPicturePlaneDescOut);

// Helpers for perseus_interleaving and perseus_bitdepth
bool toCoreInterleaving(const LCEVC_ColorFormat format, bool interleaved, int32_t& interleavingOut);
bool toCoreBitdepth(uint8_t val, int32_t& out);
bool fromCoreBitdepth(const int32_t& val, uint8_t& out);

// Closely mirrors LCEVC_EventCallback (though not exactly: enums are replaced with ints)
using EventCallback = void (*)(Handle<Decoder>, int32_t, Handle<Picture>,
                               const LCEVC_DecodeInformation*, const uint8_t*, uint32_t, void*);

} // namespace lcevc_dec::decoder

#endif // VN_API_INTERFACE_H_
