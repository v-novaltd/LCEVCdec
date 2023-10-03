/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_EVENT_MANAGER_H_
#define VN_API_EVENT_MANAGER_H_

#include "handle.h"
#include "interface.h"

#include <condition_variable>
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
                    const DecodeInformation* decodeInfoIn = nullptr,
                    const uint8_t* dataIn = nullptr, uint32_t dataSizeIn = 0)
        : picHandle(picHandleIn)
        , data(dataIn)
        , dataSize(dataSizeIn)
        , eventType(eventTypeIn)
    {
        if (decodeInfoIn != nullptr) {
            memcpy(&decodeInfo, decodeInfoIn, sizeof(decodeInfo));
        }
    }

    bool isValid() const;
    bool isFlush() const;

    Handle<Picture> picHandle;
    DecodeInformation decodeInfo = DecodeInformation(-1); // must be a copy so that it's valid until received
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
};

} // namespace lcevc_dec::decoder

#endif // VN_API_EVENT_MANAGER_H_
