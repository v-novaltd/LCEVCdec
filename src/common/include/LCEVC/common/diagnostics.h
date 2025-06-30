/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

// Diagnostics tools - logging, tracing and metrics
//
// This can be used from C and C++
//
#ifndef VN_LCEVC_COMMON_DIAGNOSTICS_H
#define VN_LCEVC_COMMON_DIAGNOSTICS_H

#include <LCEVC/build_config.h>
#ifdef __cplusplus
#include <LCEVC/common/class_utils.hpp>
#endif
#include <LCEVC/common/cpp_tools.h>
//
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Maximum number of handlers that can be registered
//
#define VNDiagnosticsMaxHandlers 16

// Severity level of log messages
//
typedef enum LdcLogLevel
{
    LdcLogLevelNone,

    LdcLogLevelFatal,
    LdcLogLevelError,
    LdcLogLevelWarning,
    LdcLogLevelInfo,
    LdcLogLevelDebug,
    LdcLogLevelVerbose,

    LdcLogLevelCount
} LdcLogLevel;

// Type of tracing event
//
typedef enum LdcDiagType
{
    LdcDiagTypeNone,

    // Logging
    LdcDiagTypeLog, // Default log message with deferred formatting
    LdcDiagTypeLogFormatted, // Format string at capture time - for 'complex' messages where performance is not an issue

    // Tracing
    LdcDiagTypeTraceBegin,   // STart named event
    LdcDiagTypeTraceEnd,     // Stop named event
    LdcDiagTypeTraceInstant, // Named event with no duration
    LdcDiagTypeTraceScoped,  // Enter/leave function scope (id in event marks begin vs. end)

    // Async events
    LdcDiagTypeTraceAsyncBegin,   // Async start
    LdcDiagTypeTraceAsyncEnd,     // Async end
    LdcDiagTypeTraceAsyncInstant, // Async instant

    // Metrics
    LdcDiagTypeMetric, // Record a sample of some named data

    //
    LdcDiagTypeFlush, // Mark a 'flush' in buffer
} LdcDiagType;

// Type of argument associated with a diagnostic record
//
typedef enum LdcDiagArg
{
    LdcDiagArgNone,

    LdcDiagArgId,

    LdcDiagArgBool,
    LdcDiagArgChar,
    LdcDiagArgInt8,
    LdcDiagArgUInt8,
    LdcDiagArgInt16,
    LdcDiagArgUInt16,
    LdcDiagArgInt32,
    LdcDiagArgUInt32,
    LdcDiagArgInt64,
    LdcDiagArgUInt64,
    LdcDiagArgCharPtr,
    LdcDiagArgConstCharPtr,
    LdcDiagArgVoidPtr,
    LdcDiagArgConstVoidPtr,

    LdcDiagArgFloat32,
    LdcDiagArgFloat64,

    LdcDiagArgCount
} LdcDiagArg;

// Static data used to describe a diagnostic site
//
typedef struct LdcDiagSite
{
    LdcDiagType type;       // Type of associated event
    const char* file;       // Source coordinates - file
    uint32_t line;          //                    - line
    LdcLogLevel level;      // Level for log messages
    const char* str;        // Event specific constant string (message/function/name/metric name)
    uint32_t argumentCount; // Description of any arguments - count
    const LdcDiagArg* argumentTypes;   // Type of each argument
    const char* const * argumentNames; // String for each argument
    LdcDiagArg valueType;              // Type of any value in record
} LdcDiagSite;

// Type specific values
typedef union LdcDiagValue
{
    uint64_t id;

    bool valueBool;
    char valueChar;
    int8_t valueInt8;
    uint8_t valueUInt8;
    int16_t valueInt16;
    uint16_t valueUInt16;
    int32_t valueInt32;
    uint32_t valueUInt32;
    int64_t valueInt64;
    uint64_t valueUInt64;
    char* valueCharPtr;
    const char* valueConstCharPtr;
    void* valueVoidPtr;
    const void* valueConstVoidPtr;
    float valueFloat32;
    double valueFloat64;

    uint64_t varDataOffset;
} LdcDiagValue;

// Record in diagnostic ring buffer - 32 bytes
//
typedef struct DiagRecord
{
    const LdcDiagSite* site; // Where the diagnostic was raised
    uint64_t timestamp;      // When the diagnostic was raised in nanoseconds 'system clock'
    uint32_t threadId;       // Thread that raised it
    uint32_t size;      // Type dependant size associated with diagnostic - e.g. variable data size
    LdcDiagValue value; // Type dependant value associated with diagnostic - e.g. a metric or id
} LdcDiagRecord;

// Function to connect diagnostics to some output mechanisms (stdout, trace file etc.)
//
// Return true if no further handlers should process the event
//
typedef bool LdcDiagHandler(void* user, const LdcDiagSite* site, const LdcDiagRecord* record,
                            const LdcDiagValue* values);

//
//
#ifdef __cplusplus
extern "C"
{
#endif
void ldcDiagnosticsInitialize(void* diagnosticState);
static inline void* ldcDiagnosticsStateGet(void);
void ldcDiagnosticsRelease(void);
bool ldcDiagnosticsHandlerPush(LdcDiagHandler* handler, void* userData);
bool ldcDiagnosticsHandlerPop(LdcDiagHandler* handler, void** userData);
void ldcDiagnosticsFlush(void);

// Set maximum reported log level
void ldcDiagnosticsLogLevel(LdcLogLevel maxLevel);

// Standard diagnostic handlers
int ldcDiagnosticFormatLog(char* dst, uint32_t dstSize, const LdcDiagSite* site,
                           const LdcDiagRecord* record, const LdcDiagValue* values);
int ldcDiagnosticFormatJson(char* dst, uint32_t dstSize, const LdcDiagSite* site,
                            const LdcDiagRecord* record, uint32_t processId);

bool ldcDiagHandlerStdio(void* user, const LdcDiagSite* site, const LdcDiagRecord* record,
                         const LdcDiagValue* values);

bool ldcDiagTraceFileInitialize(const char* filename);
bool ldcDiagTraceFileRelease(void);

// Functions used by the diagnostic macros
//

#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)
static inline void ldcLogEvent(const LdcDiagSite* site, size_t valuesSize, ...);
static inline void ldcLogEventFormatted(const LdcDiagSite* site, const char* fmt, ...);

static inline void ldcTracingEvent(const LdcDiagSite* site, size_t valuesSize, ...);

static inline void ldcTracingScopedBegin(const LdcDiagSite* site);
static inline void ldcTracingScopedEnd(const LdcDiagSite* site);

static inline void ldcMetricInt32(const LdcDiagSite* site, int32_t value);
static inline void ldcMetricUInt32(const LdcDiagSite* site, uint32_t value);
static inline void ldcMetricInt64(const LdcDiagSite* site, int64_t value);
static inline void ldcMetricUInt64(const LdcDiagSite* site, uint64_t value);
static inline void ldcMetricFloat32(const LdcDiagSite* site, float value);
static inline void ldcMetricFloat64(const LdcDiagSite* site, double value);
#else
void ldcLogEvent(const LdcDiagSite* site, size_t valuesSize, ...);
void ldcLogEventFormatted(const LdcDiagSite* site, const char* fmt, ...);
void ldcTracingEvent(const LdcDiagSite* site, size_t valuesSize, ...);
void ldcTracingScopedBegin(const LdcDiagSite* site);
void ldcTracingScopedEnd(const LdcDiagSite* site);
void ldcMetricInt32(const LdcDiagSite* site, int32_t value);
void ldcMetricUInt32(const LdcDiagSite* site, uint32_t value);
void ldcMetricInt64(const LdcDiagSite* site, int64_t value);
void ldcMetricUInt64(const LdcDiagSite* site, uint64_t value);
void ldcMetricFloat32(const LdcDiagSite* site, float value);
void ldcMetricFloat64(const LdcDiagSite* site, double value);
#endif

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

template <typename T>
constexpr LdcDiagArg LdcDiagArgumentType(T t)
{
    return static_cast<LdcDiagArg>(LdcDiagArgumentTraits<T>::Type);
}

constexpr LdcDiagArg LdcDiagArgumentType() { return LdcDiagArgNone; }

#define _VNDiagArgumentType(n, a) LdcDiagArgumentType(a),

#define _VNDiagArgumentName(n, a) #a,

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
        _VNTASizeType char*: LdcDiagArgCharPtr, \
        const char*: LdcDiagArgConstCharPtr,    \
        void*: LdcDiagArgVoidPtr,               \
        const void*: LdcDiagArgConstVoidPtr),
#else
#error "Cannot identity argument types for tracing."
#endif

// Common internal macro to generate the static Diag event site and call
//
#define _VNDiagEvent(type, level, str, ...)                                                                \
    do {                                                                                                   \
        static const LdcDiagArg args_[] = {LdcDiagArgNone, VNForEach(_VNDiagArgumentType, ##__VA_ARGS__)}; \
        static const LdcDiagSite site_ = {                                                                 \
            type,      __FILE__, __LINE__,                                                                 \
            level,     str,      (sizeof(args_) / sizeof(LdcDiagArg)) - 1,                                 \
            args_ + 1, NULL,     LdcDiagArgNone};                                                          \
        ldcLogEvent(&site_, sizeof(LdcDiagValue) * ((sizeof(args_) / sizeof(LdcDiagArg)) - 1),             \
                    ##__VA_ARGS__);                                                                        \
    } while (0)

// Version that records argument strings
#define _VNDiagEventNames(type, level, str, ...)                                                           \
    do {                                                                                                   \
        static const LdcDiagArg args_[] = {LdcDiagArgNone, VNForEach(_VNDiagArgumentType, ##__VA_ARGS__)}; \
        static const char* const names_[] = {NULL, VNForEach(_VNDiagArgumentName, ##__VA_ARGS__)};         \
        static const LdcDiagSite site_ = {                                                                 \
            type,      __FILE__,   __LINE__,                                                               \
            level,     str,        (sizeof(args_) / sizeof(LdcDiagArg)) - 1,                               \
            args_ + 1, names_ + 1, LdcDiagArgNone};                                                        \
        ldcLogEvent(&site_, sizeof(LdcDiagValue) * ((sizeof(args_) / sizeof(LdcDiagArg)) - 1),             \
                    ##__VA_ARGS__);                                                                        \
    } while (0)

// Version of above with extra event id and names
#define _VNDiagEventIdNames(type, level, id, str, ...)                                                   \
    do {                                                                                                 \
        static const LdcDiagArg args_[] = {LdcDiagArgId, VNForEach(_VNDiagArgumentType, ##__VA_ARGS__)}; \
        static const char* const names_[] = {"id", VNForEach(_VNDiagArgumentName, ##__VA_ARGS__)};       \
        static const LdcDiagSite site_ = {type,  __FILE__, __LINE__,                                     \
                                          level, str,      (sizeof(args_) / sizeof(LdcDiagArg)),         \
                                          args_, names_,   LdcDiagArgNone};                              \
        ldcLogEvent(&site_, sizeof(LdcDiagValue) * ((sizeof(args_) / sizeof(LdcDiagArg))), id,           \
                    ##__VA_ARGS__);                                                                      \
    } while (0)

// Tracing macros - C
//
#if VN_SDK_FEATURE(TRACING)

#define VNTraceBegin(msg, ...) \
    _VNDiagEventNames(LdcDiagTypeTraceBegin, LdcLogLevelNone, msg, ##__VA_ARGS__)
#define VNTraceEnd() _VNDiagEventNames(LdcDiagTypeTraceEnd, LdcLogLevelNone, "")
#define VNTraceInstant(msg, ...) \
    _VNDiagEventNames(LdcDiagTypeTraceInstant, LdcLogLevelNone, msg, ##__VA_ARGS__)

#define VNTraceScopedBegin()                                       \
    static const LdcDiagSite _traceSite = {LdcDiagTypeTraceScoped, \
                                           __FILE__,               \
                                           __LINE__,               \
                                           LdcLogLevelNone,        \
                                           __func__,               \
                                           0,                      \
                                           NULL,                   \
                                           NULL,                   \
                                           LdcDiagArgId};          \
    ldcTracingScopedBegin(&_traceSite)
#define VNTraceScopedEnd() ldcTracingScopedEnd(&_traceSite)

#define VNTraceAsyncBegin(msg, id, ...) \
    _VNDiagEventIdNames(LdcDiagTypeTraceAsyncBegin, LdcLogLevelNone, id, msg, ...)
#define VNTraceAsyncEnd(msg, id, ...) \
    _VNDiagEventIdNames(LdcDiagTypeTraceAsyncEnd, LdcLogLevelNone, id, msg, ...)
#define VNTraceAsyncInstant(msg, id, ...) \
    _VNDiagEventIdNames(LdcDiagTypeTraceAsyncInstant, LdcLogLevelNone, id, msg, ...)
#else
#define VNTraceBegin(msg, ...) (void)(msg)
#define VNTraceEnd() (void)(0)
#define VNTraceInstant(msg, ...) (void)(msg)
#define VNTraceScopedBegin() (void)(0)
#define VNTraceScopedEnd() (void)(0)

#define VNTraceAsyncBegin(msg, id, ...) ((void)(msg), (void)(id))
#define VNTraceAsyncEnd(msg, id, ...) ((void)(msg), (void)(id))
#define VNTraceAsyncInstant(msg, id, ...) ((void)(msg), (void)(id))

#endif

#ifdef __cplusplus

// Tracing macros - C++

// Wrap TraceSite in a constructor
struct LdcDiagSiteWrapper : public LdcDiagSite
{
    LdcDiagSiteWrapper(const char* f, uint32_t l, const char* func)
    {
        type = LdcDiagTypeTraceScoped;
        file = f;
        line = l;
        str = func;
        level = LdcLogLevelNone;
    }
};

class LdcTraceScoped
{
public:
    LdcTraceScoped(const LdcDiagSite* site)
        : m_site(site)
    {
        ldcTracingScopedBegin(site);
    }
    ~LdcTraceScoped() { ldcTracingScopedEnd(m_site); }

    VNNoCopyNoMove(LdcTraceScoped);

private:
    const LdcDiagSite* m_site;
};

#if VN_SDK_FEATURE(TRACING)

// Scoped object to generate begin/end events
#define VNTraceScoped()                                                 \
    static LdcDiagSiteWrapper _traceSite(__FILE__, __LINE__, __func__); \
    LdcTraceScoped _traceScoped(&_traceSite);
#else
#define VNTraceScoped() (void)(0)
#endif
#endif

// Metrics macros
//
#if VN_SDK_FEATURE(METRICS)

#define _VNTraceMetric(type, name, value)                                                \
    do {                                                                                 \
        static const LdcDiagSite site = {                                                \
            LdcDiagTypeMetric, __FILE__, __LINE__, LdcLogLevelNone, name, 0, NULL, NULL, \
            LdcDiagArg##type};                                                           \
        ldcMetric##type(&site, value);                                                   \
    } while (0)

#define VNMetricInt32(name, value) _VNTraceMetric(Int32, name, (int32_t)(value))
#define VNMetricUInt32(name, value) _VNTraceMetric(UInt32, name, (uint32_t)(value))
#define VNMetricInt64(name, value) _VNTraceMetric(Int64, name, (int64_t)(value))
#define VNMetricUInt64(name, value) _VNTraceMetric(UInt64, name, (uint64_t)(value))
#define VNMetricFloat32(name, value) _VNTraceMetric(Float32, name, (float)(value))
#define VNMetricFloat64(name, value) _VNTraceMetric(Float64, name, (double)(value))

#else
#define VNMetricInt32(name, value) (void)(VNUnused(value))
#define VNMetricUInt32(name, value) (void)(VNUnused(value))
#define VNMetricInt64(name, value) (void)(VNUnused(value))
#define VNMetricUInt64(name, value) (void)(VNUnused(value))
#define VNMetricFloat32(name, value) (void)(VNUnused(value))
#define VNMetricFloat64(name, value) (void)(VNUnused(value))
#endif

// Implementations detail
#include "detail/diagnostics.h"

#endif // VN_LCEVC_COMMON_DIAGNOSTICS_H
