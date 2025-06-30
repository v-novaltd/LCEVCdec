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
// internal ones. Consequently, do NOT add `#include "lcevc_dec.h"` to this header: the idea is
// that other files access lcevc_dec.h THROUGH these functions, enums, etc.

#ifndef VN_LCEVC_PIPELINE_LEGACY_DECODE_INFORMATION_H
#define VN_LCEVC_PIPELINE_LEGACY_DECODE_INFORMATION_H

#include <LCEVC/pipeline/picture.h>
#include <LCEVC/pipeline/types.h>
//
#include "picture.h"
//
#include <cstdint>

struct perseus_decoder_stream;

namespace lcevc_dec::decoder {

struct DecodeInformation : public LdpDecodeInformation
{
    explicit DecodeInformation(uint64_t timestampIn, bool skippedIn)
        : LdpDecodeInformation{}
    {
        timestamp = timestampIn;
        skipped = skippedIn;
        hasBase = false;
        hasEnhancement = false;
        enhanced = false;
        baseWidth = 0;
        baseHeight = 0;
        baseBitdepth = 0;
        userData = nullptr;
    }

    DecodeInformation(const Picture& base, bool lcevcAvailable, bool shouldPassthrough, bool shouldFail);
};

} // namespace lcevc_dec::decoder
#endif // VN_LCEVC_PIPELINE_LEGACY_DECODE_INFORMATION_H
