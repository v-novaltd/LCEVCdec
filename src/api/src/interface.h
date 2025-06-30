/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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
// internal pipeline ones.
//
#ifndef VN_LCEVC_API_INTERFACE_H
#define VN_LCEVC_API_INTERFACE_H

#include <LCEVC/lcevc_dec.h>
#include <LCEVC/pipeline/picture.h>
#include <LCEVC/pipeline/types.h>
//

struct perseus_decoder_stream;

// - Forward declarations (internal) --------------------------------------------------------------

namespace lcevc_dec::decoder {
class Decoder;
class Picture;
} // namespace lcevc_dec::decoder

// ------------------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

// LCEVC_ struct helpers
//
// These deliberately cast between two compatible C structures (API and internal pipeline).
// The tests  in`test_pipeline_types.cpp` makes sure these stay in line.
//
// It is expected that the internal structures and enums will gain trailing memebers as the code evolves.
//
static inline LdpPictureDesc* toLdpPictureDescPtr(LCEVC_PictureDesc* ptr)
{
    return reinterpret_cast<LdpPictureDesc*>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

static inline const LdpPictureDesc* toLdpPictureDescPtr(const LCEVC_PictureDesc* ptr)
{
    return reinterpret_cast<const LdpPictureDesc*>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

static inline LdpPictureBufferDesc* toLdpPictureBufferDescPtr(LCEVC_PictureBufferDesc* ptr)
{
    return reinterpret_cast<LdpPictureBufferDesc*>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

static inline const LdpPictureBufferDesc* toLdpPictureBufferDescPtr(const LCEVC_PictureBufferDesc* ptr)
{
    return reinterpret_cast<const LdpPictureBufferDesc*>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

static inline LdpPicturePlaneDesc* toLdpPicturePlaneDescPtr(LCEVC_PicturePlaneDesc* ptr)
{
    return reinterpret_cast<LdpPicturePlaneDesc*>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

static inline const LdpPicturePlaneDesc* toLdpPicturePlaneDescPtr(const LCEVC_PicturePlaneDesc* ptr)
{
    return reinterpret_cast<const LdpPicturePlaneDesc*>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

static inline LdpDecodeInformation* toLdpDecodeInformationPtr(LCEVC_DecodeInformation* ptr)
{
    return reinterpret_cast<LdpDecodeInformation*>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

static inline LCEVC_DecodeInformation* fromLdpDecodeInformationPtr(LdpDecodeInformation* ptr)
{
    return reinterpret_cast<LCEVC_DecodeInformation*>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

static inline const LdpDecodeInformation* toLdpDecodeInformationPtr(const LCEVC_DecodeInformation* ptr)
{
    return reinterpret_cast<const LdpDecodeInformation*>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

// LCEVC_ enum helpers
//
// Not strictly necessary, but makes is clear what is going on.
//
static inline LdpColorFormat toLdpColorFormat(LCEVC_ColorFormat format)
{
    return static_cast<LdpColorFormat>(format);
}
static inline LCEVC_ReturnCode fromLdcReturnCode(LdcReturnCode returnCode)
{
    return static_cast<LCEVC_ReturnCode>(returnCode);
}
static inline LdpAccess toLdpAccess(LCEVC_Access access) { return static_cast<LdpAccess>(access); }

} // namespace lcevc_dec::decoder

#endif // VN_LCEVC_API_INTERFACE_H
