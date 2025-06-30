/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#ifndef VN_LCEVC_PIPELINE_LEGACY_CORE_INTERFACE_H
#define VN_LCEVC_PIPELINE_LEGACY_CORE_INTERFACE_H

#include <LCEVC/pipeline/picture.h>
#include <LCEVC/pipeline/types.h>
//
#include <cstdint>

struct perseus_decoder_stream;

namespace lcevc_dec::decoder {

// Helpers for converting <-> core data types
bool toCoreInterleaving(LdpColorFormat format, bool interleaved, int32_t& interleavingOut);
bool toCoreBitdepth(uint8_t val, int32_t& out);
bool fromCoreBitdepth(const int32_t& val, uint8_t& out);
bool coreFormatToLdpPictureDesc(const perseus_decoder_stream& coreFormat, LdpPictureDesc& picDescOut);
uint32_t bitdepthFromLdpColorFormat(int32_t colorFormat);

} // namespace lcevc_dec::decoder

#endif // VN_LCEVC_PIPELINE_LEGACY_CORE_INTERFACE_H
