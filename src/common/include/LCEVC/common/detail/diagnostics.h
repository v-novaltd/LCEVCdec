/* Copyright (c) V-Nova International Limited 2025-2026. All rights reserved.
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

#ifndef VN_LCEVC_COMMON_DETAIL_DIAGNOSTICS_H
#define VN_LCEVC_COMMON_DETAIL_DIAGNOSTICS_H

#include <LCEVC/common/atomic.h>
#include <LCEVC/common/clock.h>
#include <LCEVC/common/diagnostics_buffer.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/stdlib.h>
#include <LCEVC/common/threads.h>

// Functions used by the diagnostic macros
//
#ifdef __cplusplus
extern "C"
{
#endif

#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)
static inline void ldcDiagEvent(const LdcDiagSite* site, size_t valuesSize, ...);
static inline void ldcDiagEventFormatted(const LdcDiagSite* site, const char* fmt, ...);
static inline void ldcMetricInt32(const LdcDiagSite* site, int32_t value);
static inline void ldcMetricUInt32(const LdcDiagSite* site, uint32_t value);
static inline void ldcMetricInt64(const LdcDiagSite* site, int64_t value);
static inline void ldcMetricUInt64(const LdcDiagSite* site, uint64_t value);
static inline void ldcMetricFloat32(const LdcDiagSite* site, float value);
static inline void ldcMetricFloat64(const LdcDiagSite* site, double value);
#else
void ldcDiagEvent(const LdcDiagSite* site, size_t valuesSize, ...);
void ldcDiagEventFormatted(const LdcDiagSite* site, const char* fmt, ...);
void ldcMetricInt32(const LdcDiagSite* site, int32_t value);
void ldcMetricUInt32(const LdcDiagSite* site, uint32_t value);
void ldcMetricInt64(const LdcDiagSite* site, int64_t value);
void ldcMetricUInt64(const LdcDiagSite* site, uint64_t value);
void ldcMetricFloat32(const LdcDiagSite* site, float value);
void ldcMetricFloat64(const LdcDiagSite* site, double value);
#endif

// Common diagnostic state
//
typedef struct DiagnosticState
{
    // Stack of diagnostic handlers
    struct
    {
        LdcDiagHandler* handler;
        void* userData;
    } handlers[VNDiagnosticsMaxHandlers];

    uint32_t handlersCount;

    LdcLogLevel maxLogLevel;

    bool initialized;
    LdcAtomicUint refCount;

    LdcDiagnosticsBuffer diagnosticsBuffer;

#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)
    Thread thread;
    ThreadMutex mutex;

    ThreadCondVar flushed;

    // This is protected by the above mutex
    int flushCount;
#endif

} DiagnosticState;

extern DiagnosticState* ldcDiagnosticsState;

//
static inline void* ldcDiagnosticsStateGet(void) { return ldcDiagnosticsState; }

// Common site used for all scoped TraceEnds
extern const LdcDiagSite ldcDiagnosticsTraceScopedEndSite;

#ifdef __cplusplus
}
#endif

// Argument classification
//
#ifdef __cplusplus

// Use a C++ traits type to classify argument types
template <typename T>
struct LdcDiagArgumentTraits
{};
template <>
struct LdcDiagArgumentTraits<bool>
{
    enum
    {
        Type = LdcDiagArgBool
    };
};
template <>
struct LdcDiagArgumentTraits<char>
{
    enum
    {
        Type = LdcDiagArgChar
    };
};
template <>
struct LdcDiagArgumentTraits<int8_t>
{
    enum
    {
        Type = LdcDiagArgInt8
    };
};
template <>
struct LdcDiagArgumentTraits<uint8_t>
{
    enum
    {
        Type = LdcDiagArgUInt8
    };
};
template <>
struct LdcDiagArgumentTraits<int16_t>
{
    enum
    {
        Type = LdcDiagArgInt16
    };
};
template <>
struct LdcDiagArgumentTraits<uint16_t>
{
    enum
    {
        Type = LdcDiagArgUInt16
    };
};
template <>
struct LdcDiagArgumentTraits<int32_t>
{
    enum
    {
        Type = LdcDiagArgInt32
    };
};
template <>
struct LdcDiagArgumentTraits<uint32_t>
{
    enum
    {
        Type = LdcDiagArgUInt32
    };
};
template <>
struct LdcDiagArgumentTraits<int64_t>
{
    enum
    {
        Type = LdcDiagArgInt64
    };
};
template <>
struct LdcDiagArgumentTraits<uint64_t>
{
    enum
    {
        Type = LdcDiagArgUInt64
    };
};
template <>
struct LdcDiagArgumentTraits<char*>
{
    enum
    {
        Type = LdcDiagArgCharPtr
    };
};
template <>
struct LdcDiagArgumentTraits<const char*>
{
    enum
    {
        Type = LdcDiagArgConstCharPtr
    };
};
template <>
struct LdcDiagArgumentTraits<void*>
{
    enum
    {
        Type = LdcDiagArgVoidPtr
    };
};
template <>
struct LdcDiagArgumentTraits<const void*>
{
    enum
    {
        Type = LdcDiagArgConstVoidPtr
    };
};
template <>
struct LdcDiagArgumentTraits<float>
{
    enum
    {
        Type = LdcDiagArgFloat32
    };
};
template <>
struct LdcDiagArgumentTraits<double>
{
    enum
    {
        Type = LdcDiagArgFloat64
    };
};

#if VN_OS(BROWSER)
template <>
struct LdcDiagArgumentTraits<unsigned long>
{
    enum
    {
        Type = LdcDiagArgUInt32
    };
};
#endif

template <typename T>
constexpr LdcDiagArg LdcDiagArgumentType(T)
{
    return static_cast<LdcDiagArg>(LdcDiagArgumentTraits<T>::Type);
}

constexpr LdcDiagArg LdcDiagArgumentType() { return LdcDiagArgNone; }

#define _VNDiagArgumentType(n, a) LdcDiagArgumentType(a)

#if VN_OS(APPLE)
template <>
struct LdcDiagArgumentTraits<unsigned long>
{
    enum
    {
        Type = LdcDiagArgUInt32
    };
};
#endif

#elif __STDC_VERSION__ >= 201112
// Use C11 _Generic to classify argument type

/* clang-format off */
#if !defined(_MSC_VER) || defined(__clang__)
#define _VNTACharType \
    char: LdcDiagArgChar,
#else
#define _VNTACharType
#endif

#if VN_OS(APPLE)
#define _VNTASizeType \
    size_t: LdcDiagArgUInt64,
#else
#define _VNTASizeType
#endif
/* clang-format on */

#define _VNDiagArgumentType(n, a) \
    _Generic((a),                               \
        _Bool: LdcDiagArgBool,                  \
        _VNTACharType int8_t: LdcDiagArgInt8,   \
        uint8_t: LdcDiagArgUInt8,               \
        int16_t: LdcDiagArgInt16,               \
        uint16_t: LdcDiagArgUInt16,             \
        int32_t: LdcDiagArgInt32,               \
        uint32_t: LdcDiagArgUInt32,             \
        int64_t: LdcDiagArgInt64,               \
        uint64_t: LdcDiagArgUInt64,             \
        float: LdcDiagArgFloat32,               \
        double: LdcDiagArgFloat64,              \
        _VNTASizeType char*: LdcDiagArgCharPtr, \
        const char*: LdcDiagArgConstCharPtr,    \
        void*: LdcDiagArgVoidPtr,               \
        const void*: LdcDiagArgConstVoidPtr)
#else
#error "Cannot identity argument types for tracing."
#endif

#define _VNDiagArgumentComma() ,

#define _VNDiagArgumentPairValue(n, a0, a1) a1
#define _VNDiagArgumentPairType(n, a0, a1) _VNDiagArgumentType(n, a1)
#define _VNDiagArgumentPairName(n, a0, a1) a0

// do { ... } while(0) is used to make macros work with if/else correctly
//
// NOLINTBEGIN(cppcoreguidelines-avoid-do-while)

// Common internal macro to generate the static Diag event site and call
//
#define _VNDiagEvent(type, level, str)                                                  \
    do {                                                                                \
        static const LdcDiagSite site_ = {type, __FILE__, __LINE__, level,         str, \
                                          0,    NULL,     NULL,     LdcDiagArgNone};    \
        ldcDiagEvent(&site_, 0);                                                        \
    } while (0)

#define _VNDiagEventNoArgs(type, level, str) _VNDiagEvent(type, level, str)

#define _VNDiagEventId(type, level, id, str)                                            \
    do {                                                                                \
        static const LdcDiagArg args_[] = {LdcDiagArgId};                               \
        static const char* const names_[] = {"id"};                                     \
        static const LdcDiagSite site_ = {type, __FILE__, __LINE__, level,         str, \
                                          1,    args_,    names_,   LdcDiagArgNone};    \
        ldcDiagEvent(&site_, sizeof(LdcDiagValue), (uint64_t)id);                       \
    } while (0)

#define _VNDiagEventIdNoArgs(type, level, id, str) _VNDiagEventId(type, level, id, str)

#define _VNDiagEventArgs(type, level, str, ...)                                                 \
    do {                                                                                        \
        static const LdcDiagArg args_[] = {LdcDiagArgNone VNForEach(                            \
            _VNDiagArgumentType, _VNDiagArgumentComma, _VNDiagArgumentComma, ##__VA_ARGS__)};   \
        static const LdcDiagSite site_ = {                                                      \
            type,      __FILE__, __LINE__,                                                      \
            level,     str,      (sizeof(args_) / sizeof(LdcDiagArg)) - 1,                      \
            args_ + 1, NULL,     LdcDiagArgNone};                                               \
        ldcDiagEvent(&site_, sizeof(LdcDiagValue) * ((sizeof(args_) / sizeof(LdcDiagArg)) - 1), \
                     ##__VA_ARGS__);                                                            \
    } while (0)

// Version that records pairs of argument name and value
#define _VNDiagEventNamedArgs(type, level, str, ...)                                               \
    do {                                                                                           \
        static const LdcDiagArg args_[] = {LdcDiagArgNone VNForEachPair(                           \
            _VNDiagArgumentPairType, _VNDiagArgumentComma, _VNDiagArgumentComma, ##__VA_ARGS__)};  \
        static const char* const names_[] = {NULL VNForEachPair(                                   \
            _VNDiagArgumentPairName, _VNDiagArgumentComma, _VNDiagArgumentComma, ##__VA_ARGS__)};  \
        static const LdcDiagSite site_ = {                                                         \
            type,      __FILE__,   __LINE__,                                                       \
            level,     str,        (sizeof(args_) / sizeof(LdcDiagArg)) - 1,                       \
            args_ + 1, names_ + 1, LdcDiagArgNone};                                                \
        ldcDiagEvent(&site_, sizeof(LdcDiagValue) *                                                \
                                 ((sizeof(args_) / sizeof(LdcDiagArg)) - 1)                        \
                                     VNForEachPair(_VNDiagArgumentPairValue, _VNDiagArgumentComma, \
                                                   _VNDiagArgumentComma, ##__VA_ARGS__));          \
    } while (0)

// Version of above with extra event id
#define _VNDiagEventIdNamedArgs(type, level, id, str, ...)                                        \
    do {                                                                                          \
        static const LdcDiagArg args_[] = {LdcDiagArgId VNForEachPair(                            \
            _VNDiagArgumentPairType, _VNDiagArgumentComma, _VNDiagArgumentComma, ##__VA_ARGS__)}; \
        static const char* const names_[] = {"id" VNForEachPair(                                  \
            _VNDiagArgumentPairName, _VNDiagArgumentComma, _VNDiagArgumentComma, ##__VA_ARGS__)}; \
        static const LdcDiagSite site_ = {type,  __FILE__, __LINE__,                              \
                                          level, str,      (sizeof(args_) / sizeof(LdcDiagArg)),  \
                                          args_, names_,   LdcDiagArgNone};                       \
        ldcDiagEvent(&site_, sizeof(LdcDiagValue) * (sizeof(args_) / sizeof(LdcDiagArg)),         \
                     (uint64_t)id VNForEachPair(_VNDiagArgumentPairValue, _VNDiagArgumentComma,   \
                                                _VNDiagArgumentComma, ##__VA_ARGS__));            \
    } while (0)

#ifdef __cplusplus
// Tracing macros - C++

class LdcTraceScoped
{
public:
    LdcTraceScoped() {}
    ~LdcTraceScoped() { ldcDiagEvent(&ldcDiagnosticsTraceScopedEndSite, 0); }

    VNNoCopyNoMove(LdcTraceScoped);
};

class LdcDiagVaEndGuard
{
public:
    explicit LdcDiagVaEndGuard(va_list* args)
        : m_args(args)
    {}
    ~LdcDiagVaEndGuard() { va_end(*m_args); }

    VNNoCopyNoMove(LdcDiagVaEndGuard);

private:
    va_list* m_args;
};
#endif

// Metrics
#define _VNTraceMetric(type, name, value)                                                \
    do {                                                                                 \
        static const LdcDiagSite site_ = {                                               \
            LdcDiagTypeMetric, __FILE__, __LINE__, LdcLogLevelNone, name, 0, NULL, NULL, \
            LdcDiagArg##type};                                                           \
        ldcMetric##type(&site_, value);                                                  \
    } while (0)

#ifdef __cplusplus
extern "C"
{
#endif

// Get a time for diagnostic records
//
static inline uint64_t ldcDiagnosticGetTimestamp(void) { return ldcClockMonotonicNanoseconds(); }

// Fill in LdcDiagRecord
static inline void ldcDiagnosticsRecordSet(LdcDiagRecord* record, const LdcDiagSite* site)
{
    record->site = site;
    record->timestamp = ldcDiagnosticGetTimestamp();
    record->threadId = (uint32_t)VNGetThreadId();
}

// Fill in entire LdcDiagRecord
static inline void ldcDiagnosticsRecordSetAll(LdcDiagRecord* record, const LdcDiagSite* site,
                                              uint64_t id, uint32_t size)
{
    record->site = site;
    record->timestamp = ldcDiagnosticGetTimestamp();
    record->threadId = (uint32_t)VNGetThreadId();
    record->size = size;
    record->value.id = id;
}

// Out of line utility to copy VA args into diag record variable data
// Returns number of values actually copied.
uint32_t ldcDiagnosticsCopyArguments(const LdcDiagSite* site, LdcDiagValue values[],
                                     uint32_t valuesCount, va_list args);

#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)

// The inline implementations of the underlying diagnostics entry points
//
static inline void ldcDiagEvent(const LdcDiagSite* site, size_t valuesSize, ...)
{
    if (!site || !ldcDiagnosticsState->initialized || site->level > ldcDiagnosticsState->maxLogLevel) {
        return;
    }

    if (valuesSize == 0) {
        // Special case this path so that inlining can simplify this code
        LdcDiagRecord* rec = ldcDiagnosticsBufferPushBegin(&ldcDiagnosticsState->diagnosticsBuffer, 0);
        ldcDiagnosticsRecordSetAll(rec, site, 0, 0);
        ldcDiagnosticsBufferPushEnd(&ldcDiagnosticsState->diagnosticsBuffer);
    } else {
        LdcDiagRecord* rec =
            ldcDiagnosticsBufferPushBegin(&ldcDiagnosticsState->diagnosticsBuffer, valuesSize);

        // Extract arguments into a value array
        LdcDiagValue* values =
            (LdcDiagValue*)ldcDiagnosticsBufferVarData(&ldcDiagnosticsState->diagnosticsBuffer, rec);

        ldcDiagnosticsRecordSet(rec, site);

        va_list args;
        va_start(args, valuesSize);
#ifdef __cplusplus
        {
            LdcDiagVaEndGuard argsGuard(&args);
            ldcDiagnosticsCopyArguments(site, values, site->argumentCount, args);
        }
#else
        ldcDiagnosticsCopyArguments(site, values, site->argumentCount, args);
        va_end(args);
#endif

        ldcDiagnosticsBufferPushEnd(&ldcDiagnosticsState->diagnosticsBuffer);
    }
}

static inline void ldcDiagEventFormatted(const LdcDiagSite* site, const char* fmt, ...)
{
    if (!site || site->level > ldcDiagnosticsState->maxLogLevel) {
        return;
    }

    LdcDiagRecord record;

    char buffer[4096];

    va_list args;
    va_start(args, fmt);
    const int chrs = vsnprintf(buffer, sizeof(buffer) - 1, fmt, args);
    va_end(args);
    buffer[chrs] = '\0';

    ldcDiagnosticsRecordSet(&record, site);

    ldcDiagnosticsBufferPush(&ldcDiagnosticsState->diagnosticsBuffer, &record, (uint8_t*)buffer, chrs + 1);
}

static inline void ldcTracingScoped(const LdcDiagSite* site, uint64_t id)
{
    if (!ldcDiagnosticsState->initialized) {
        return;
    }

    LdcDiagRecord* rec = ldcDiagnosticsBufferPushBegin(&ldcDiagnosticsState->diagnosticsBuffer, 0);
    ldcDiagnosticsRecordSetAll(rec, site, id, 0);
    ldcDiagnosticsBufferPushEnd(&ldcDiagnosticsState->diagnosticsBuffer);
}

static inline void ldcMetricInt32(const LdcDiagSite* site, int32_t value)
{
    if (!ldcDiagnosticsState->initialized) {
        return;
    }
    LdcDiagRecord record;
    ldcDiagnosticsRecordSetAll(&record, site, 0, 0);
    record.value.valueInt32 = value;

    ldcDiagnosticsBufferPush(&ldcDiagnosticsState->diagnosticsBuffer, &record, NULL, 0);
}

static inline void ldcMetricUInt32(const LdcDiagSite* site, uint32_t value)
{
    if (!ldcDiagnosticsState->initialized) {
        return;
    }
    LdcDiagRecord record;
    ldcDiagnosticsRecordSetAll(&record, site, 0, 0);
    record.value.valueUInt32 = value;

    ldcDiagnosticsBufferPush(&ldcDiagnosticsState->diagnosticsBuffer, &record, NULL, 0);
}

static inline void ldcMetricInt64(const LdcDiagSite* site, int64_t value)
{
    if (!ldcDiagnosticsState->initialized) {
        return;
    }
    LdcDiagRecord record;
    ldcDiagnosticsRecordSetAll(&record, site, 0, 0);
    record.value.valueInt64 = value;

    ldcDiagnosticsBufferPush(&ldcDiagnosticsState->diagnosticsBuffer, &record, NULL, 0);
}

static inline void ldcMetricUInt64(const LdcDiagSite* site, uint64_t value)
{
    if (!ldcDiagnosticsState->initialized) {
        return;
    }
    LdcDiagRecord record;
    ldcDiagnosticsRecordSetAll(&record, site, 0, 0);
    record.value.valueUInt64 = value;

    ldcDiagnosticsBufferPush(&ldcDiagnosticsState->diagnosticsBuffer, &record, NULL, 0);
}

static inline void ldcMetricFloat32(const LdcDiagSite* site, float value)
{
    if (!ldcDiagnosticsState->initialized) {
        return;
    }
    LdcDiagRecord record;
    ldcDiagnosticsRecordSetAll(&record, site, 0, 0);
    record.value.valueFloat32 = value;

    ldcDiagnosticsBufferPush(&ldcDiagnosticsState->diagnosticsBuffer, &record, NULL, 0);
}

static inline void ldcMetricFloat64(const LdcDiagSite* site, double value)
{
    if (!ldcDiagnosticsState->initialized) {
        return;
    }
    LdcDiagRecord record;
    ldcDiagnosticsRecordSetAll(&record, site, 0, 0);
    record.value.valueFloat64 = value;

    ldcDiagnosticsBufferPush(&ldcDiagnosticsState->diagnosticsBuffer, &record, NULL, 0);
}

#endif

#ifdef __cplusplus
}
#endif

// NOLINTEND(cppcoreguidelines-avoid-do-while)

#endif // VN_LCEVC_COMMON_DETAIL_DIAGNOSTICS_H
