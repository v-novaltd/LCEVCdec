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

#include <LCEVC/common/class_utils.hpp>
#include <LCEVC/common/constants.h>
#include <LCEVC/common/threads.h>
#include <LCEVC/lcevc_dec.h>
//
#include "decoder_context.h"
#include "event.h"
#include "event_dispatcher.h"
#include "handle.h"
#include "interface.h"
#include "pool.h"
//
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace lcevc_dec::decoder {

// - Event ----------------------------------------------------------------------------------------

// Internal event types
static constexpr uint8_t kInvalidEvent = LCEVC_EventCount + 1;
static constexpr uint8_t kFlushEvent = LCEVC_EventCount + 2;

bool Event::isValid() const { return (eventType > 0) && (eventType < LCEVC_EventCount); }
bool Event::isFlush() const { return eventType == kFlushEvent; }

// - EventDispatcher ---------------------------------------------------------------------------------

EventDispatcher::~EventDispatcher() = default;

class EventDispatcherImpl : public EventDispatcher
{
public:
    explicit EventDispatcherImpl(DecoderContext* context);
    ~EventDispatcherImpl() { release(); }

    void enableEvents(const std::vector<int32_t>& enabledEvents) override;
    bool isEventEnabled(uint8_t eventType) const final { return m_eventMask & (1 << eventType); }

    void generate(uint8_t eventTypeIn, struct LdpPicture* pictureIn, const LdpDecodeInformation* decodeInfoIn,
                  const uint8_t* dataIn, uint32_t dataSizeIn) override;

    void setEventCallback(LCEVC_EventCallback callback, void* userData) override;

    VNNoCopyNoMove(EventDispatcherImpl);

private:
    void release() noexcept;
    void eventLoop();
    bool getNextEvent(Event& event);

    DecoderContext* m_context = nullptr;

    uint16_t m_eventMask = 0; // The enabled events (set once at initialize and never changed).
    LCEVC_EventCallback m_eventCallback = nullptr;
    void* m_eventCallbackUserData = nullptr;

    // Threading. The event queue is protected by m_eventQueueMutex, and when it changes, it
    // notifies m_eventQueueCv.
    std::queue<Event> m_eventQueue;
    std::mutex m_eventQueueMutex;
    std::condition_variable m_eventQueueCv;
    std::thread m_eventThread;
    bool m_threadExists = false;
};

EventDispatcherImpl::EventDispatcherImpl(DecoderContext* context)
    : m_context{context}
    , m_eventThread{std::thread(&EventDispatcherImpl::eventLoop, this)}
    , m_threadExists{true}
{
    assert(8 * sizeof(m_eventMask) >=
           std::max(static_cast<uint8_t>(LCEVC_EventCount), std::max(kInvalidEvent, kFlushEvent)));
}

void EventDispatcherImpl::enableEvents(const std::vector<int32_t>& enabledEvents)
{
    // No failure case, because we've already validated the events in DecoderConfig::validate. To
    // reiterate, that validation is: eventTypes are POSITIVE & SMALL. Internally, they are uint8_t
    for (const int32_t eventType : enabledEvents) {
        m_eventMask = m_eventMask | static_cast<uint16_t>(1 << eventType);
    }
}

void EventDispatcherImpl::release() noexcept
{
    // Prevent double-release
    if (!m_threadExists) {
        return;
    }

    // Send ourselves a flushing event, to force any prior events out of the queue and break out
    // of our loop.
    //
    // NB: Explicit qualification of method as this will be called during a destructor.
    EventDispatcherImpl::generate(kFlushEvent, nullptr, nullptr, nullptr, 0);

    m_eventThread.join();
    m_eventThread = std::thread();
    m_threadExists = false;
}

void EventDispatcherImpl::generate(uint8_t eventType, struct LdpPicture* picture,
                                   const LdpDecodeInformation* decodeInfo, const uint8_t* data,
                                   uint32_t dataSize)
{
    Event event(eventType, picture, decodeInfo, data, dataSize);

    const std::scoped_lock lock(m_eventQueueMutex);

    if (isEventEnabled(event.eventType) || event.isFlush()) {
        // This may throw an exception if m_eventQueue.size() > SIZE_MAX, needs to be caught when
        // this function is used in a class destructor.
        m_eventQueue.push(event);

        m_eventQueueCv.notify_all();
    }
}

void EventDispatcherImpl::setEventCallback(LCEVC_EventCallback callback, void* userData)
{
    m_eventCallback = callback;
    m_eventCallbackUserData = userData;
}

void EventDispatcherImpl::eventLoop()
{
    Event event = kInvalidEvent;
    while (getNextEvent(event)) {
        if (!event.isValid()) {
            // Break loop: if we got an invalid event off the queue, that's the signal to shut
            // down the thread.
            return;
        }

        if (m_eventCallback != nullptr) {
            const LCEVC_DecoderHandle decoderHandle =
                m_context ? m_context->handle() : LCEVC_DecoderHandle{kInvalidHandle};

            const LCEVC_DecodeInformation* const decodeInfo =
                (event.decodeInfo.timestamp != kInvalidTimestamp)
                    ? fromLdpDecodeInformationPtr(&event.decodeInfo)
                    : nullptr;

            Handle<LdpPicture> pictureHandle{kInvalidHandle};
            if (event.picture) {
                m_context->lock();
                pictureHandle = m_context->picturePool().reverseLookup(event.picture);
                m_context->unlock();
            }

            m_eventCallback(decoderHandle, static_cast<LCEVC_Event>(event.eventType),
                            {pictureHandle.handle}, decodeInfo, event.data, event.dataSize,
                            m_eventCallbackUserData);
        }
    }
}

bool EventDispatcherImpl::getNextEvent(Event& event)
{
    // Lock on our own mutex, to ensure that events are sent strictly in order. This may mean we
    // trigger a callback while still inside some API call, but that should be fine: if the
    // callback itself uses the API, then THAT call will wait for the API lock.
    std::unique_lock lock(m_eventQueueMutex);

    // If the event queue is empty, wait here until we're notified. So, wait here until notified
    // AND m_eventQueue is non-empty (the condition prevents spurious unblocks)
    m_eventQueueCv.wait(lock, [this]() { return !m_eventQueue.empty(); });

    event = m_eventQueue.front();
    m_eventQueue.pop();

    return true;
}

std::unique_ptr<EventDispatcher> createEventDispatcher(DecoderContext* context)
{
    return std::make_unique<EventDispatcherImpl>(context);
}

} // namespace lcevc_dec::decoder
