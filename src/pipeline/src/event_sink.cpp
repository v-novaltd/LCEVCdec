/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#include <LCEVC/pipeline/event_sink.h>

namespace lcevc_dec::pipeline {

EventSink::~EventSink() = default;

class EventSinkNull : public EventSink
{
public:
    void enableEvents(const std::vector<int32_t>& enabledEvents) override
    {
        //   Always act as if events are diabled
    }
    bool isEventEnabled(uint8_t) const override { return false; }

    void generate(uint8_t eventType, struct LdpPicture* picture,
                  const LdpDecodeInformation* decodeInfo, const uint8_t* data, uint32_t dataSize) override
    {
        // Never genaretes any events
    }
};

EventSink* EventSink::nullSink()
{
    static EventSinkNull sink;
    return &sink;
}

} // namespace lcevc_dec::pipeline
