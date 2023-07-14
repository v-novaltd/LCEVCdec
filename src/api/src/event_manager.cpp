/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#include "event_manager.h"

#include "handle.h"
#include "uLog.h"
#include "uPlatform.h"

#include <LCEVC/lcevc_dec.h>

#include <mutex>
#include <vector>

namespace lcevc_dec::decoder {

// - Event ----------------------------------------------------------------------------------------

static const Event kFlushEvent(-2);
static const Event kInvalidEvent(-1);
bool Event::isValid() const { return (eventType > 0) && (eventType < LCEVC_EventCount); }
bool Event::isFlush() const { return eventType == kFlushEvent.eventType; }

// - EventManager ---------------------------------------------------------------------------------

EventManager::EventManager(LCEVC_DecoderHandle& apiHandle)
    : m_apiHandle(apiHandle)
{
    static_assert(8 * sizeof(m_eventMask) >= LCEVC_EventCount, "Increase the size of m_eventMask");
}

void EventManager::initialise(const std::vector<int32_t>& enabledEvents)
{
    // No failure case, because we've already validated the events in DecoderConfig::validate
    for (int32_t eventType : enabledEvents) {
        m_eventMask = m_eventMask | (1 << eventType);
    }

    m_eventThread = std::thread(&EventManager::eventLoop, this);
}

void EventManager::release()
{
    // Send ourselves a flushing event, to force any prior events out of the queue and break out
    // of our loop.
    triggerEvent(kFlushEvent);

    // Prevent double-release:
    if (m_eventThread.joinable()) {
        m_eventThread.join();
        m_eventThread = std::thread();
    }
}

void EventManager::triggerEvent(Event event)
{
    std::lock_guard<std::mutex> lock(m_eventQueueMutex);
    if (isEventEnabled(event.eventType) || event.isFlush()) {
        m_eventQueue.push(event);
        m_eventQueueCv.notify_all();
    }
}

void EventManager::setEventCallback(EventCallback callback, void* userData)
{
    m_eventCallback = callback;
    m_eventCallbackUserData = userData;
}

void EventManager::eventLoop()
{
    lcevc_dec::utility::os::setThreadName("LCEVC_EventManager");

    Event nextEvent = kInvalidEvent;
    while (getNextEvent(nextEvent)) {
        if (!nextEvent.isValid()) {
            // Break loop: if we got an invalid event off the queue, that's the signal to shut
            // down the thread.
            return;
        }

        if (m_eventCallback != nullptr) {
            m_eventCallback(m_apiHandle.hdl, static_cast<LCEVC_Event>(nextEvent.eventType),
                            nextEvent.picHandle, nextEvent.decodeInfo, nextEvent.data,
                            nextEvent.dataSize, m_eventCallbackUserData);
        }
    }
}

bool EventManager::getNextEvent(Event& event)
{
    // Lock on our own mutex, to ensure that events are sent strictly in order. This may mean we
    // trigger a callback while still inside some API call, but that should be fine: if the
    // callback itself uses the API, then THAT call will wait for the API lock.
    std::unique_lock<std::mutex> lock(m_eventQueueMutex);

    // If the event queue is empty, wait here until we're notified (we put it in a loop: if we add
    // 1 event but get 2 notifications for it, then the 1st notification will trigger the callback,
    // while the 2nd notification will get stopped here because the queue will have been emptied).
    while (m_eventQueue.empty()) {
        m_eventQueueCv.wait(lock);
    }

    // Queue is non-empty, let's proceed:
    event = m_eventQueue.front();
    m_eventQueue.pop();

    return true;
}

} // namespace lcevc_dec::decoder
