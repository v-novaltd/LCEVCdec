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

#ifndef VN_LCEVC_API_EVENT_DISPATCHER_H
#define VN_LCEVC_API_EVENT_DISPATCHER_H

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/configure.hpp>
#include <LCEVC/pipeline/event_sink.h>
#include <LCEVC/pipeline/picture.h>
//
#include "decoder_context.h"
//
#include <memory>

namespace lcevc_dec::decoder {

// EventDispatcher
//
// Interface for something that can generate events
//
class EventDispatcher : public pipeline::EventSink
{
protected:
    EventDispatcher() = default;

public:
    virtual ~EventDispatcher() = 0;

    virtual void setEventCallback(LCEVC_EventCallback callback, void* userData) = 0;

    VNNoCopyNoMove(EventDispatcher);

private:
};

std::unique_ptr<EventDispatcher> createEventDispatcher(DecoderContext* context);

} // namespace lcevc_dec::decoder

#endif // VN_LCEVC_API_EVENT_DISPATCHER_H
