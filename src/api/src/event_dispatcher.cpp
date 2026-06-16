/* Copyright (c) V-Nova International Limited 2023-2026. All rights reserved.
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

#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/limit.h>
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

static inline bool isValid(uint8_t eventType) { return (eventType < LCEVC_EventCount); }
static inline bool isFlush(uint8_t eventType) { return eventType == kFlushEvent; }

// - EventDispatcher ---------------------------------------------------------------------------------

EventDispatcher::~EventDispatcher() = default;

class EventDispatcherImpl : public EventDispatcher
{
public:
    explicit EventDispatcherImpl(DecoderContext* context);
    ~EventDispatcherImpl() { release(); }

    void enableEvents(const int32_t* events, uint32_t eventCount) override;
    bool isEventEnabled(uint8_t eventType) const final { return m_eventMask & (1 << eventType); }

    void generate(uint8_t eventTypeIn, struct LdpPicture* pictureIn, const LdpDecodeInformation* decodeInfoIn,
                  const uint8_t* dataIn, uint32_t dataSizeIn) override;

    void setEventCallback(LCEVC_EventCallback callback, void* userData) override;

    VNNoCopyNoMove(EventDispatcherImpl);

private:
    void release() noexcept;
    void eventLoop();
    bool getNextEvent(Event& event);

    static bool diagnosticHandler(void* user, const LdcDiagSite* site, const LdcDiagRecord* record,
                                  const LdcDiagValue* values, uint32_t valuesCount);

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
    bool m_logHandlerRegistered = false;
};

EventDispatcherImpl::EventDispatcherImpl(DecoderContext* context)
    : m_context{context}
    , m_eventThread{std::thread(&EventDispatcherImpl::eventLoop, this)}
    , m_threadExists{true}
{
    assert(8 * sizeof(m_eventMask) >=
           std::max(static_cast<uint8_t>(LCEVC_EventCount), std::max(kInvalidEvent, kFlushEvent)));
}

void EventDispatcherImpl::enableEvents(const int32_t* events, uint32_t eventCount)
{
    uint16_t prevMask = m_eventMask;

    // No failure case, because we've already validated the events in DecoderConfig::validate. To
    // reiterate, that validation is: eventTypes are POSITIVE & SMALL. Internally, they are uint8_t
    for (uint32_t i = 0; i < eventCount; ++i) {
        m_eventMask = m_eventMask | static_cast<uint16_t>(1 << events[i]);
    }

    // If the logging event is now enabled, add a diagnostic handler that forwards log messages
    if ((prevMask ^ m_eventMask) & (1 << pipeline::EventLog)) {
        m_logHandlerRegistered = ldcDiagnosticsHandlerPush(diagnosticHandler, this);
    }
}

void EventDispatcherImpl::release() noexcept
{
    // Prevent double-release
    if (!m_threadExists) {
        return;
    }

    if (m_logHandlerRegistered) {
        void* userData = this;
        ldcDiagnosticsHandlerPop(diagnosticHandler, &userData);
        m_logHandlerRegistered = false;
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
    if (!isEventEnabled(eventType) && !isFlush(eventType)) {
        return;
    }

    const std::scoped_lock lock(m_eventQueueMutex);

    // This may throw an exception if m_eventQueue.size() > SIZE_MAX, needs to be caught when
    // this function is used in a class destructor.
    m_eventQueue.emplace(eventType, picture, decodeInfo, data, dataSize);

    m_eventQueueCv.notify_all();
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
        if (!isValid(event.eventType)) {
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
                            {pictureHandle.handle}, decodeInfo, event.data.data(),
                            (uint32_t)event.data.size(), m_eventCallbackUserData);
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

// Single character level indicator to prefix the log data
//
constexpr char logLevelId(LdcLogLevel level)
{
    switch (level) {
        case LdcLogLevelFatal: return '1';
        case LdcLogLevelError: return '2';
        case LdcLogLevelWarning: return '3';
        case LdcLogLevelInfo: return '4';
        case LdcLogLevelDebug: return '5';
        case LdcLogLevelVerbose: return '6';
        default: assert(0); return ' ';
    }
}

// Handle log events - convert to a data buffer and push into event queue
//
// This could make the event callback directly - it would avoid some memory copies and
// extra threading, but runs the risk of surprising the event handler by calling it from two
// threads, and may get logging out of order relative to the other surrounding events.
//
bool EventDispatcherImpl::diagnosticHandler(void* user, const LdcDiagSite* site, const LdcDiagRecord* record,
                                            const LdcDiagValue* values, uint32_t valuesCount)
{
    // Only handle log messages
    if (!(site->type == LdcDiagTypeLog || (site->type == LdcDiagTypeLogFormatted && values != nullptr))) {
        return false;
    }

    auto* self = reinterpret_cast<EventDispatcherImpl*>(user);

    // Temp. string with space for null terminator
    constexpr size_t kBufferSize = 4095;
    char buffer[kBufferSize + 1];
    int bufferUsed = 0;

    // Add "<level> <file>:<line>: " for Debug and Verbose
    if (site->level >= LdcLogLevelDebug) {
        bufferUsed = snprintf(buffer, kBufferSize, "%c %s:%d: ", logLevelId(site->level),
                              site->file, site->line);
    } else {
        // Prefix message with just level otherwise
        bufferUsed = snprintf(buffer, kBufferSize, "%c ", logLevelId(site->level));
    }

    if ((bufferUsed < 0) || (bufferUsed > static_cast<int>(kBufferSize))) {
        // snprintf failure or buffer overrun
        return false;
    }

    // Append rest of message
    bufferUsed += ldcDiagnosticFormatLog(buffer + bufferUsed, kBufferSize - bufferUsed - 1, site,
                                         record, values, valuesCount);

    // Final char always null terminator
    buffer[bufferUsed] = '\0';
    bufferUsed += 1;

    self->generate(pipeline::EventLog, nullptr, nullptr, reinterpret_cast<uint8_t*>(buffer), bufferUsed);
    return true;
}

std::unique_ptr<EventDispatcher> createEventDispatcher(DecoderContext* context)
{
    return std::make_unique<EventDispatcherImpl>(context);
}

} // namespace lcevc_dec::decoder
