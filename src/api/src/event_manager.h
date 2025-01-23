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

#ifndef VN_API_EVENT_MANAGER_H_
#define VN_API_EVENT_MANAGER_H_

#include "handle.h"
#include "interface.h"
#include "memory.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>

// ------------------------------------------------------------------------------------------------

struct LCEVC_DecoderHandle;

namespace lcevc_dec::decoder {

class Decoder;
class Picture;

// - Event ----------------------------------------------------------------------------------------

struct Event
{
    // The constructor parameters are in a different order to the member variables, because the
    // parameters are designed to let you do "Event e(LCEVC_EventType)", whereas the member
    // variables are in memory alignment order.
    constexpr Event(uint8_t eventTypeIn, Handle<Picture> picHandleIn = kInvalidHandle,
                    const DecodeInformation& decodeInfoIn = DecodeInformation(-1, false),
                    const uint8_t* dataIn = nullptr, uint32_t dataSizeIn = 0)
        : picHandle(picHandleIn)
        , decodeInfo(decodeInfoIn)
        , data(dataIn)
        , dataSize(dataSizeIn)
        , eventType(eventTypeIn)
    {}

    bool isValid() const;
    bool isFlush() const;

    Handle<Picture> picHandle;
    DecodeInformation decodeInfo; // Must be a copy (not pointer or reference) so that it's valid until received
    const uint8_t* data;
    uint32_t dataSize;
    uint8_t eventType;
};

// - EventManager ---------------------------------------------------------------------------------

class EventManager
{
public:
    EventManager(LCEVC_DecoderHandle& apiHandle);
    ~EventManager() { release(); }

    void initialise(const std::vector<int32_t>& enabledEvents);
    void release();

    // catchExceptions should be false everywhere except in destructors
    void triggerEvent(Event event, bool catchExceptions = false);
    bool isEventEnabled(uint8_t eventType) const { return m_eventMask & (1 << eventType); }
    void setEventCallback(EventCallback callback, void* userData);

private:
    void eventLoop();
    bool getNextEvent(Event& event);

    uint16_t m_eventMask = 0; // The enabled events (set once at initialise and never changed).
    EventCallback m_eventCallback = nullptr;
    void* m_eventCallbackUserData = nullptr;
    LCEVC_DecoderHandle& m_apiHandle; // The handle that the decoder gave out in api
    // Note: m_apiHandle must be a reference rather than a copy, because the value actually isn't
    // set until the moment RIGHT AFTER event manager is constructed.

    // Threading. The event queue is protected by m_eventQueueMutex, and when it changes, it
    // notifies m_eventQueueCv.
    std::queue<Event> m_eventQueue;
    std::mutex m_eventQueueMutex;
    std::condition_variable m_eventQueueCv;
    std::thread m_eventThread;
    bool m_threadExists = false;
};

} // namespace lcevc_dec::decoder

#endif // VN_API_EVENT_MANAGER_H_
