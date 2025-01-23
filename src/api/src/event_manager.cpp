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

#include "event_manager.h"

#include "handle.h"
#include "interface.h"

#include <LCEVC/lcevc_dec.h>
#include <LCEVC/utility/threading.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace lcevc_dec::decoder {

// - Event ----------------------------------------------------------------------------------------

static constexpr Event kInvalidEvent(LCEVC_EventCount + 1);
static constexpr Event kFlushEvent(LCEVC_EventCount + 2);
bool Event::isValid() const { return (eventType > 0) && (eventType < LCEVC_EventCount); }
bool Event::isFlush() const { return eventType == kFlushEvent.eventType; }

// - EventManager ---------------------------------------------------------------------------------

EventManager::EventManager(LCEVC_DecoderHandle& apiHandle)
    : m_apiHandle(apiHandle)
{
    static_assert(8 * sizeof(m_eventMask) >=
                      std::max(static_cast<uint8_t>(LCEVC_EventCount),
                               std::max(kInvalidEvent.eventType, kFlushEvent.eventType)),
                  "Increase the size of m_eventMask");
}

void EventManager::initialise(const std::vector<int32_t>& enabledEvents)
{
    // No failure case, because we've already validated the events in DecoderConfig::validate. To
    // reiterate, that validation is: eventTypes are POSITIVE & SMALL. Internally, they are uint8_t
    for (const int32_t eventType : enabledEvents) {
        m_eventMask = m_eventMask | (1 << eventType);
    }

    m_threadExists = true;
    m_eventThread = std::thread(&EventManager::eventLoop, this);
}

void EventManager::release()
{
    // Prevent double-release
    if (!m_threadExists) {
        return;
    }

    // Send ourselves a flushing event, to force any prior events out of the queue and break out
    // of our loop. Note that catchExceptions is true, because this is called in a destructor.
    triggerEvent(kFlushEvent, true);

    m_eventThread.join();
    m_eventThread = std::thread();
    m_threadExists = false;
}

void EventManager::triggerEvent(Event event, bool catchExceptions)
{
    const std::lock_guard<std::mutex> lock(m_eventQueueMutex);

    if (isEventEnabled(event.eventType) || event.isFlush()) {
        try {
            m_eventQueue.push(event);
        } catch (const std::exception& e) {
            // Possible if m_eventQueue.size() > SIZE_MAX, needs to be caught when this function is
            // used in a class destructor.
            if (!catchExceptions) {
                throw e;
            }
        }

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
    setThreadName(VN_TO_THREAD_NAME("LCEVC_EventManager"));

    Event nextEvent = kInvalidEvent;
    while (getNextEvent(nextEvent)) {
        if (!nextEvent.isValid()) {
            // Break loop: if we got an invalid event off the queue, that's the signal to shut
            // down the thread.
            return;
        }

        if (m_eventCallback != nullptr) {
            std::unique_ptr<LCEVC_DecodeInformation> shortLivedDecInfo = nullptr;
            if (nextEvent.decodeInfo.timestamp >= 0) {
                shortLivedDecInfo = std::make_unique<LCEVC_DecodeInformation>();
                memcpy(shortLivedDecInfo.get(), &(nextEvent.decodeInfo), sizeof(*shortLivedDecInfo));
            }

            m_eventCallback(m_apiHandle.hdl, static_cast<LCEVC_Event>(nextEvent.eventType),
                            nextEvent.picHandle, shortLivedDecInfo.get(), nextEvent.data,
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

    // If the event queue is empty, wait here until we're notified. So, wait here until notified
    // AND m_eventQueue is non-empty (the condition prevents spurious unblocks)
    m_eventQueueCv.wait(lock, [this]() { return !m_eventQueue.empty(); });

    event = m_eventQueue.front();
    m_eventQueue.pop();

    return true;
}

} // namespace lcevc_dec::decoder
