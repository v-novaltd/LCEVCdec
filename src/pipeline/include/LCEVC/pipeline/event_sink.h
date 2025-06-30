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

#ifndef VN_LCEVC_PIPELINE_EVENT_SINK_H
#define VN_LCEVC_PIPELINE_EVENT_SINK_H

#include <LCEVC/common/class_utils.hpp>
//
#include <cstdint>
#include <vector>

struct LdpPicture;
struct LdpDecodeInformation;

namespace lcevc_dec::pipeline {

// Matches LCEVC_Event
//
typedef enum Event
{
    EventLog = 0,                /**< A logging event from the decoder */
    EventExit = 1,               /**< The decoder will exit - no further events will be generated */
    EventCanSendBase = 2,        /**< SendDecoderBase will not return LCEVC_Again */
    EventCanSendEnhancement = 3, /**< SendDecoderEnhancementData will not return LCEVC_Again */
    EventCanSendPicture = 4,     /**< SendDecoderPicture will not return LCEVC_Again */
    EventCanReceive = 5,         /**< ReceiveDecoderPicture will not return LCEVC_Again */
    EventBasePictureDone = 6,    /**< A base picture is no longer needed by decoder */
    EventOutputPictureDone = 7,  /**< An output picture has been completed by the decoder */
    Event_Count
} Event;

// EventSink
//
// Interface for something that accepts raised events
//

class EventSink
{
protected:
    EventSink() = default;

public:
    virtual ~EventSink() = 0;

    virtual void enableEvents(const std::vector<int32_t>& enabledEvents) = 0;
    virtual bool isEventEnabled(uint8_t eventType) const = 0;

    virtual void generate(uint8_t eventType, LdpPicture* picture = nullptr,
                          const LdpDecodeInformation* decodeInfo = nullptr,
                          const uint8_t* data = nullptr, uint32_t dataSize = 0) = 0;

    // Return a pointer to a common event sink that does nothing
    static EventSink* nullSink();

    VNNoCopyNoMove(EventSink);

private:
};

} // namespace lcevc_dec::pipeline

#endif // VN_LCEVC_PIPELINE_EVENT_SINK_H
