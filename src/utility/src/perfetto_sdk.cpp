/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

#include <LCEVC/utility/perfetto_sdk.h>

#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

namespace {

constexpr uint32_t kMonotonicClock = perfetto::protos::pbzero::BuiltinClock::BUILTIN_CLOCK_MONOTONIC;

enum class TraceCategory : uint8_t
{
    Default,
    Jni,
    Vulkan
};

std::mutex gHandlerMutex;
std::once_flag gInitialize;
bool gHandlerRegistered = false;

std::mutex gTraceStackMutex;
std::unordered_map<uint32_t, std::vector<TraceCategory>> gTraceStacks;

class SessionObserver final : public perfetto::TrackEventSessionObserver
{
public:
    void OnStop(const perfetto::DataSourceBase::StopArgs&) override
    {
        // OnStop still accepts TrackEvents. Drain LCEVCdec's upstream queue before Perfetto
        // completes its own producer flush so the end of a playback is not lost.
        {
            std::lock_guard<std::mutex> lock(gHandlerMutex);
            if (gHandlerRegistered && ldcDiagnosticsStateGet()) {
                ldcDiagnosticsFlush();
            }
        }
        perfetto::TrackEvent::Flush();
    }
};

SessionObserver gSessionObserver;

bool hasPrefix(const char* string, const char* prefix)
{
    return string != nullptr && std::strncmp(string, prefix, std::strlen(prefix)) == 0;
}

void rememberTraceBegin(uint32_t threadId, TraceCategory category)
{
    std::lock_guard<std::mutex> lock(gTraceStackMutex);
    gTraceStacks[threadId].push_back(category);
}

TraceCategory traceEndCategory(uint32_t threadId)
{
    std::lock_guard<std::mutex> lock(gTraceStackMutex);
    auto it = gTraceStacks.find(threadId);
    if (it == gTraceStacks.end() || it->second.empty()) {
        return TraceCategory::Default;
    }
    const TraceCategory category = it->second.back();
    it->second.pop_back();
    if (it->second.empty()) {
        gTraceStacks.erase(it);
    }
    return category;
}

void nameTrack(const perfetto::Track& track, const char* name)
{
    auto descriptor = track.Serialize();
    descriptor.set_name(name);
    perfetto::TrackEvent::SetTrackDescriptor(track, descriptor);
}

void addArgument(perfetto::protos::pbzero::DebugAnnotation* annotation, LdcDiagArg type,
                 const LdcDiagValue& value)
{
    switch (type) {
        case LdcDiagArgId: annotation->set_uint_value(value.id); break;
        case LdcDiagArgBool: annotation->set_bool_value(value.valueBool); break;
        case LdcDiagArgChar: annotation->set_int_value(value.valueChar); break;
        case LdcDiagArgInt8: annotation->set_int_value(value.valueInt8); break;
        case LdcDiagArgUInt8: annotation->set_uint_value(value.valueUInt8); break;
        case LdcDiagArgInt16: annotation->set_int_value(value.valueInt16); break;
        case LdcDiagArgUInt16: annotation->set_uint_value(value.valueUInt16); break;
        case LdcDiagArgInt32: annotation->set_int_value(value.valueInt32); break;
        case LdcDiagArgUInt32: annotation->set_uint_value(value.valueUInt32); break;
        case LdcDiagArgInt64: annotation->set_int_value(value.valueInt64); break;
        case LdcDiagArgUInt64: annotation->set_uint_value(value.valueUInt64); break;
        case LdcDiagArgCharPtr:
            annotation->set_string_value(value.valueCharPtr ? value.valueCharPtr : "null");
            break;
        case LdcDiagArgConstCharPtr:
            annotation->set_string_value(value.valueConstCharPtr ? value.valueConstCharPtr : "null");
            break;
        case LdcDiagArgVoidPtr:
            annotation->set_pointer_value(reinterpret_cast<uintptr_t>(value.valueVoidPtr));
            break;
        case LdcDiagArgConstVoidPtr:
            annotation->set_pointer_value(reinterpret_cast<uintptr_t>(value.valueConstVoidPtr));
            break;
        case LdcDiagArgFloat32: annotation->set_double_value(value.valueFloat32); break;
        case LdcDiagArgFloat64: annotation->set_double_value(value.valueFloat64); break;
        case LdcDiagArgNone:
        case LdcDiagArgCount: break;
    }
}

void addArguments(perfetto::EventContext context, const LdcDiagSite* site,
                  const LdcDiagValue* values, uint32_t valuesCount, uint32_t first = 0)
{
    if (!values) {
        return;
    }

    const uint32_t count = site->argumentCount < valuesCount ? site->argumentCount : valuesCount;
    for (uint32_t index = first; index < count; ++index) {
        auto* annotation = context.event()->add_debug_annotations();
        const char* name = site->argumentNames ? site->argumentNames[index] : "argument";
        annotation->set_name(name);
        addArgument(annotation, site->argumentTypes[index], values[index]);
    }
}

bool emitDefaultDiagnostic(const LdcDiagSite* site, const LdcDiagRecord* record,
                           const LdcDiagValue* values, uint32_t valuesCount)
{
    if (!TRACE_EVENT_CATEGORY_ENABLED("lcevc.dec")) {
        return false;
    }

    const perfetto::TraceTimestamp eventTimestamp{kMonotonicClock, record->timestamp};
    const auto eventThread = perfetto::ThreadTrack::ForThread(record->threadId);
    const auto arguments = [&](perfetto::EventContext context) {
        addArguments(std::move(context), site, values, valuesCount);
    };

    switch (site->type) {
        case LdcDiagTypeTraceBegin:
            TRACE_EVENT_BEGIN("lcevc.dec", perfetto::StaticString(site->str), eventThread,
                              eventTimestamp, arguments);
            return true;
        case LdcDiagTypeTraceEnd:
            TRACE_EVENT_END("lcevc.dec", eventThread, eventTimestamp);
            return true;
        case LdcDiagTypeTraceInstant:
            TRACE_EVENT_INSTANT("lcevc.dec", perfetto::StaticString(site->str), eventThread,
                                eventTimestamp, arguments);
            return true;
        case LdcDiagTypeTraceAsyncBegin:
        case LdcDiagTypeTraceAsyncEnd:
        case LdcDiagTypeTraceAsyncInstant: {
            if (!values || valuesCount == 0 || site->argumentTypes[0] != LdcDiagArgId) {
                return false;
            }
            const perfetto::Track track(values[0].id);
            const auto asyncArguments = [&](perfetto::EventContext context) {
                addArguments(std::move(context), site, values, valuesCount, 1);
            };
            if (site->type == LdcDiagTypeTraceAsyncBegin) {
                nameTrack(track, site->str);
                TRACE_EVENT_BEGIN("lcevc.dec", perfetto::StaticString(site->str), track,
                                  eventTimestamp, asyncArguments);
            } else if (site->type == LdcDiagTypeTraceAsyncEnd) {
                TRACE_EVENT_END("lcevc.dec", track, eventTimestamp);
            } else {
                nameTrack(track, site->str);
                TRACE_EVENT_INSTANT("lcevc.dec", perfetto::StaticString(site->str), track,
                                    eventTimestamp, asyncArguments);
            }
            return true;
        }
        case LdcDiagTypeMetric: {
            const auto track = perfetto::CounterTrack(perfetto::StaticString(site->str));
            switch (site->valueType) {
                case LdcDiagArgInt32:
                    TRACE_COUNTER("lcevc.dec", track, eventTimestamp, record->value.valueInt32);
                    break;
                case LdcDiagArgUInt32:
                    TRACE_COUNTER("lcevc.dec", track, eventTimestamp, record->value.valueUInt32);
                    break;
                case LdcDiagArgInt64:
                    TRACE_COUNTER("lcevc.dec", track, eventTimestamp, record->value.valueInt64);
                    break;
                case LdcDiagArgUInt64:
                    TRACE_COUNTER("lcevc.dec", track, eventTimestamp, record->value.valueUInt64);
                    break;
                case LdcDiagArgFloat32:
                    TRACE_COUNTER("lcevc.dec", track, eventTimestamp, record->value.valueFloat32);
                    break;
                case LdcDiagArgFloat64:
                    TRACE_COUNTER("lcevc.dec", track, eventTimestamp, record->value.valueFloat64);
                    break;
                default: return false;
            }
            return true;
        }
        case LdcDiagTypeNone:
        case LdcDiagTypeLog:
        case LdcDiagTypeLogFormatted:
        case LdcDiagTypeMemoryAllocate:
        case LdcDiagTypeMemoryReallocate:
        case LdcDiagTypeMemoryFree:
        case LdcDiagTypeFlush: return false;
    }
    return false;
}

bool emitJniDiagnostic(const LdcDiagSite* site, const LdcDiagRecord* record,
                       const LdcDiagValue* values, uint32_t valuesCount)
{
    if (!TRACE_EVENT_CATEGORY_ENABLED("lcevc.dec.jni")) {
        return false;
    }

    constexpr size_t prefixLength = sizeof("jni:") - 1;
    const char* name = hasPrefix(site->str, "jni:") ? site->str + prefixLength : site->str;
    const perfetto::TraceTimestamp eventTimestamp{kMonotonicClock, record->timestamp};
    const auto eventThread = perfetto::ThreadTrack::ForThread(record->threadId);
    const auto arguments = [&](perfetto::EventContext context) {
        addArguments(std::move(context), site, values, valuesCount);
    };

    switch (site->type) {
        case LdcDiagTypeTraceBegin:
            TRACE_EVENT_BEGIN("lcevc.dec.jni", perfetto::DynamicString(name), eventThread,
                              eventTimestamp, arguments);
            return true;
        case LdcDiagTypeTraceEnd:
            TRACE_EVENT_END("lcevc.dec.jni", eventThread, eventTimestamp);
            return true;
        case LdcDiagTypeTraceInstant:
            TRACE_EVENT_INSTANT("lcevc.dec.jni", perfetto::DynamicString(name), eventThread,
                                eventTimestamp, arguments);
            return true;
        default: return false;
    }
}

#if VN_SDK_PIPELINE(VULKAN)

int gVulkanGpuTrackToken{};
std::once_flag gVulkanGpuTrackDescriptor;

const LdcDiagValue* findArgument(const LdcDiagSite* site, const LdcDiagValue* values,
                                 uint32_t valuesCount, const char* name, LdcDiagArg type)
{
    if (!values || !site->argumentNames || !site->argumentTypes) {
        return nullptr;
    }
    const uint32_t count = site->argumentCount < valuesCount ? site->argumentCount : valuesCount;
    for (uint32_t index = 0; index < count; ++index) {
        if (site->argumentTypes[index] == type && site->argumentNames[index] != nullptr &&
            std::strcmp(site->argumentNames[index], name) == 0) {
            return &values[index];
        }
    }
    return nullptr;
}

bool emitGpuDuration(const LdcDiagSite* site, const LdcDiagValue* values, uint32_t valuesCount)
{
    constexpr size_t prefixLength = sizeof("vulkan:") - 1;
    const char* name = hasPrefix(site->str, "vulkan:") ? site->str + prefixLength : site->str;
    if (site->type != LdcDiagTypeTraceInstant || std::strcmp(name, "gpu.timestamp") != 0) {
        return false;
    }

    const auto* label = findArgument(site, values, valuesCount, "label", LdcDiagArgConstCharPtr);
    const auto* start = findArgument(site, values, valuesCount, "startTimestampNs", LdcDiagArgUInt64);
    const auto* end = findArgument(site, values, valuesCount, "endTimestampNs", LdcDiagArgUInt64);
    if (!label || !label->valueConstCharPtr || !start || !end || end->valueUInt64 < start->valueUInt64) {
        return false;
    }

    const auto track = perfetto::Track::FromPointer(&gVulkanGpuTrackToken);
    std::call_once(gVulkanGpuTrackDescriptor, [&] { nameTrack(track, "LCEVC Vulkan GPU"); });
    const auto arguments = [&](perfetto::EventContext context) {
        addArguments(std::move(context), site, values, valuesCount);
    };

    TRACE_EVENT_BEGIN("lcevc.dec.vulkan", perfetto::DynamicString(label->valueConstCharPtr), track,
                      perfetto::TraceTimestamp{kMonotonicClock, start->valueUInt64}, arguments);
    TRACE_EVENT_END("lcevc.dec.vulkan", track, perfetto::TraceTimestamp{kMonotonicClock, end->valueUInt64});
    return true;
}

bool emitVulkanDiagnostic(const LdcDiagSite* site, const LdcDiagRecord* record,
                          const LdcDiagValue* values, uint32_t valuesCount)
{
    if (!TRACE_EVENT_CATEGORY_ENABLED("lcevc.dec.vulkan")) {
        return false;
    }

    constexpr size_t prefixLength = sizeof("vulkan:") - 1;
    const char* name = hasPrefix(site->str, "vulkan:") ? site->str + prefixLength : site->str;
    const perfetto::TraceTimestamp eventTimestamp{kMonotonicClock, record->timestamp};
    const auto eventThread = perfetto::ThreadTrack::ForThread(record->threadId);
    const auto arguments = [&](perfetto::EventContext context) {
        addArguments(std::move(context), site, values, valuesCount);
    };

    switch (site->type) {
        case LdcDiagTypeTraceBegin:
            TRACE_EVENT_BEGIN("lcevc.dec.vulkan", perfetto::DynamicString(name), eventThread,
                              eventTimestamp, arguments);
            return true;
        case LdcDiagTypeTraceEnd:
            TRACE_EVENT_END("lcevc.dec.vulkan", eventThread, eventTimestamp);
            return true;
        case LdcDiagTypeTraceInstant:
            TRACE_EVENT_INSTANT("lcevc.dec.vulkan", perfetto::DynamicString(name), eventThread,
                                eventTimestamp, arguments);
            return true;
        default: return false;
    }
}

} // namespace

bool lcevcPerfettoDiagnosticHandler(void*, const LdcDiagSite* site, const LdcDiagRecord* record,
                                    const LdcDiagValue* values, uint32_t valuesCount)
{
    TraceCategory category = TraceCategory::Default;
    if (hasPrefix(site->str, "jni:")) {
        category = TraceCategory::Jni;
    } else if (hasPrefix(site->str, "vulkan:")) {
        category = TraceCategory::Vulkan;
    }

    if (category == TraceCategory::Vulkan && TRACE_EVENT_CATEGORY_ENABLED("lcevc.dec.vulkan") &&
        emitGpuDuration(site, values, valuesCount)) {
        return true;
    }
    if (site->type == LdcDiagTypeTraceBegin) {
        rememberTraceBegin(record->threadId, category);
    } else if (site->type == LdcDiagTypeTraceEnd) {
        // Trace-end records use a shared site, so recover the category from their matching begin.
        category = traceEndCategory(record->threadId);
    }

    switch (category) {
        case TraceCategory::Jni: return emitJniDiagnostic(site, record, values, valuesCount);
        case TraceCategory::Vulkan: return emitVulkanDiagnostic(site, record, values, valuesCount);
        case TraceCategory::Default:
            return emitDefaultDiagnostic(site, record, values, valuesCount);
    }
    return false;
}

void lcevcPerfettoResetDiagnosticHandler()
{
    std::lock_guard<std::mutex> lock(gTraceStackMutex);
    gTraceStacks.clear();
}

#else

} // namespace

bool lcevcPerfettoDiagnosticHandler(void*, const LdcDiagSite* site, const LdcDiagRecord* record,
                                    const LdcDiagValue* values, uint32_t valuesCount)
{
    TraceCategory category = hasPrefix(site->str, "jni:") ? TraceCategory::Jni : TraceCategory::Default;
    if (site->type == LdcDiagTypeTraceBegin) {
        rememberTraceBegin(record->threadId, category);
    } else if (site->type == LdcDiagTypeTraceEnd) {
        category = traceEndCategory(record->threadId);
    }
    return category == TraceCategory::Jni ? emitJniDiagnostic(site, record, values, valuesCount)
                                          : emitDefaultDiagnostic(site, record, values, valuesCount);
}

void lcevcPerfettoResetDiagnosticHandler()
{
    std::lock_guard<std::mutex> lock(gTraceStackMutex);
    gTraceStacks.clear();
}

#endif

void lcevcPerfettoSdkInitialize()
{
    std::call_once(gInitialize, [] {
        perfetto::TracingInitArgs arguments;
        arguments.backends = perfetto::kSystemBackend;
        perfetto::Tracing::Initialize(arguments);
        perfetto::TrackEvent::Register();
        perfetto::TrackEvent::AddSessionObserver(&gSessionObserver);
    });
}

bool lcevcPerfettoSdkRegister()
{
    lcevcPerfettoSdkInitialize();

    std::lock_guard<std::mutex> lock(gHandlerMutex);
    if (gHandlerRegistered || !ldcDiagnosticsStateGet() ||
        !ldcDiagnosticsHandlerPush(lcevcPerfettoDiagnosticHandler, nullptr)) {
        return false;
    }
    gHandlerRegistered = true;
    return true;
}

void lcevcPerfettoSdkUnregister()
{
    {
        std::lock_guard<std::mutex> lock(gHandlerMutex);
        if (!gHandlerRegistered) {
            return;
        }
        if (ldcDiagnosticsStateGet()) {
            ldcDiagnosticsFlush();
            void* userData = nullptr;
            ldcDiagnosticsHandlerPop(lcevcPerfettoDiagnosticHandler, &userData);
        }
        gHandlerRegistered = false;
        lcevcPerfettoResetDiagnosticHandler();
    }
    perfetto::TrackEvent::Flush();
}
