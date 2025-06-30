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

#ifndef VN_LCEVC_API_EVENT_H
#define VN_LCEVC_API_EVENT_H

#include <LCEVC/pipeline/picture.h>
//
#include "handle.h"
#include "interface.h"

// ------------------------------------------------------------------------------------------------

struct LCEVC_DecoderHandle;
struct LdpPicture;

namespace lcevc_dec::decoder {

class Decoder;

// Event
//
// Holds the event state
//
struct Event
{
    // The constructor parameters are in a different order to the member variables, because the
    // parameters are designed to let you do "Event e(LCEVC_EventType)", whereas the member
    // variables are in memory alignment order.
    constexpr Event(uint8_t eventTypeIn, struct LdpPicture* pictureIn = nullptr,
                    const LdpDecodeInformation* decodeInfoIn = nullptr,
                    const uint8_t* dataIn = nullptr, uint32_t dataSizeIn = 0)
        : picture(pictureIn)
        , decodeInfo(decodeInfoIn ? *decodeInfoIn : LdpDecodeInformation{})
        , data(dataIn)
        , dataSize(dataSizeIn)
        , eventType(eventTypeIn)
    {}

    bool isValid() const;
    bool isFlush() const;

    LdpPicture* picture;
    LdpDecodeInformation decodeInfo; // Must be a copy (not pointer or reference) so that it's valid until received
    const uint8_t* data;
    uint32_t dataSize;
    uint8_t eventType;

    // picture handle resolved at event trigger time
    Handle<LdpPicture> pictureHandle{kInvalidHandle};
};

} // namespace lcevc_dec::decoder

#endif // VN_LCEVC_API_EVENT_H
