/* Copyright (c) V-Nova International Limited 2024-2026. All rights reserved.
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
#include <LCEVC/common/stdlib.h>

/* Maximum number of handlers that can be registered
 */
#define VNDiagnosticsMaxHandlers 16

/*! Severity level of log messages
 */
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

/*! Type of tracing event
 */
typedef enum LdcDiagType
{
    LdcDiagTypeNone,

    // Logging
    LdcDiagTypeLog, ///! Default log message with deferred formatting
    LdcDiagTypeLogFormatted, ///! Format string at capture time - for 'complex' messages where performance is not an issue

    // Tracing
    LdcDiagTypeTraceBegin,   ///! Start named event
    LdcDiagTypeTraceEnd,     ///! Stop named event
    LdcDiagTypeTraceInstant, ///! Named event with no duration

    // Async events
    LdcDiagTypeTraceAsyncBegin,   ///! Async start
    LdcDiagTypeTraceAsyncEnd,     ///! Async end
    LdcDiagTypeTraceAsyncInstant, ///! Async instant

    // Metrics
    LdcDiagTypeMetric, ///! Record a sample of some named data

    // Memory
    LdcDiagTypeMemoryAllocate,   ///! Allocate a block memory
    LdcDiagTypeMemoryReallocate, ///! Reallocate a block memory
    LdcDiagTypeMemoryFree,       ///! Free a block of memory

    //
    LdcDiagTypeFlush, // Mark a 'flush' in buffer
} LdcDiagType;

/*! Type of argument associated with a diagnostic record
 */
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

/*! Describes a diagnostic's source location details, level, and associated values.
 *
 * This is usually created as a static constant by the various diagnostic event macros.
 */
typedef struct LdcDiagSite
{
    LdcDiagType type;       ///< Type of associated event
    const char* file;       ///< Source coordinates - file
    uint32_t line;          ///< Source coordinates - line
    LdcLogLevel level;      ///< Level for log messages
    const char* str;        ///< Event specific constant string (message/function/name/metric name)
    uint32_t argumentCount; ///< Description of any arguments - count
    const LdcDiagArg* argumentTypes;   ///< Type of each argument
    const char* const * argumentNames; ///< String for each argument
    LdcDiagArg valueType;              ///< Type of any value in record
} LdcDiagSite;

/*! Type specific values
 */
typedef union LdcDiagValue
{
    uint64_t id; ///< LdcDiagArgId

    bool valueBool;                ///< LdcDiagArgBool
    char valueChar;                ///< LdcDiagArgChar
    int8_t valueInt8;              ///< LdcDiagArgInt8
    uint8_t valueUInt8;            ///< LdcDiagArgUInt8
    int16_t valueInt16;            ///< LdcDiagArgInt16
    uint16_t valueUInt16;          ///< LdcDiagUint16
    int32_t valueInt32;            ///< LdcDiagArgInt32
    uint32_t valueUInt32;          ///< LdcDiagArgUInt32
    int64_t valueInt64;            ///< LdcDiagArgInt64
    uint64_t valueUInt64;          ///< LdcDiagArgUInt64
    char* valueCharPtr;            ///< LdcDiagArgCharPtr
    const char* valueConstCharPtr; ///< LdcDiagArgConstCharPtr
    void* valueVoidPtr;            ///< LdcDiagArgVoidPtr
    const void* valueConstVoidPtr; ///< LdcDiagArgConstVoidPtr
    float valueFloat32;            ///< LdcDiagArgFloat32
    double valueFloat64;           ///< LdcDiagArgFloat64

    uint64_t varDataOffset; ///< Offset into diagnostic buffer
} LdcDiagValue;

/*! Record in diagnostic ring buffer - 32 bytes
 */
typedef struct DiagRecord
{
    const LdcDiagSite* site; ///< Where the diagnostic was raised
    uint64_t timestamp;      ///< When the diagnostic was raised in nanoseconds 'system clock'
    uint32_t threadId;       ///< Thread that raised it
    uint32_t size; ///< Type dependent size associated with diagnostic - e.g. variable data size
    LdcDiagValue value; ///< Type dependent value associated with diagnostic - e.g. a metric or id
} LdcDiagRecord;

#ifdef __cplusplus
extern "C"
{
#endif
/*! Diagnostic output handler callback.
 *
 * @param user User pointer provided when the handler was registered.
 * @param site Static metadata describing the event (type, level, args).
 * @param record Per-event data such as timestamp, thread id, and value/id.
 * @param values Array of argument values. For LdcDiagTypeLogFormatted this points at the formatted
 *               string buffer.
 * @param valuesCount Number of values.
 *
 * @return true to stop propagation to earlier handlers, false to continue.
 */
typedef bool LdcDiagHandler(void* user, const LdcDiagSite* site, const LdcDiagRecord* record,
                            const LdcDiagValue* values, uint32_t valuesCount);

/*! Diagnostics handler that writes log events to a stdio FILE*.
 *
 * Handles LdcDiagTypeLog and LdcDiagTypeLogFormatted records; returns true when handled.
 *
 * @param user User pointer provided when the handler was registered.
 * @param site Static metadata describing the event (type, level, args).
 * @param record Per-event data such as timestamp, thread id, and value/id.
 * @param values Array of argument values. For LdcDiagTypeLogFormatted this points at the formatted
 *               string buffer.
 * @param valuesCount Number of values.
 *
 * @return True if event was a Log message of some sort.
 */
bool ldcDiagHandlerStdio(void* user, const LdcDiagSite* site, const LdcDiagRecord* record,
                         const LdcDiagValue* values, uint32_t valuesCount);

/*! Initialize diagnostics.
 *
 * Reference-counted; the first call initializes state. If parentState is non-null, it is
 * used as the shared diagnostics state for this module; otherwise a local state is used.
 * Must be paired with ldcDiagnosticsRelease.
 *
 * @param parentState Optional shared state to use instead of local state.
 */
void ldcDiagnosticsInitialize(void* parentState);

/*! Get the active diagnostics state pointer.
 *
 * Useful for passing shared diagnostics state into other components.
 */
static inline void* ldcDiagnosticsStateGet(void);

/*! Release diagnostics.
 *
 * Decrements the reference count; the final release shuts down handlers and async resources.
 */
void ldcDiagnosticsRelease(void);

/*! Register a diagnostics handler.
 *
 * Handlers are invoked in LIFO order. Returns false if the handler is null or the
 * handler stack is full.
 *
 * @param handler Handler function to be added.
 * @param userData User data pointer that will be passed to handler.
 *
 * @return True if handler was added.
 */
bool ldcDiagnosticsHandlerPush(LdcDiagHandler* handler, void* userData);

/*! Remove a diagnostics handler.
 *
 * If userData is non-null, matches both handler and userData; otherwise matches the handler
 * only. On success, stores the matched userData in *userData when provided.
 *
 * @param handler Handler function to be removed.
 * @param userData Pointer to where userData is stored - if not NULL, used to match specific handler.
 *
 * @return True if handler was found and removed.
 */
bool ldcDiagnosticsHandlerPop(LdcDiagHandler* handler, void** userData);

/*! Flush pending diagnostics.
 *
 * In async builds, blocks until buffered events are processed. No-op for synchronous builds.
 */
void ldcDiagnosticsFlush(void);

/*! Set maximum reported log level.
 * Log events with level > maxLevel are dropped.
 *
 * @param maxLevel Upper limit for diagnostic log event recording.
 *
 */
void ldcDiagnosticsLogLevel(LdcLogLevel maxLevel);

/*! Format a diagnostic log record as a string.
 *
 * @param dst Destination buffer.
 * @param dstSize Size of destination buffer.
 * @param site Static metadata describing the event (type, level, args).
 * @param record Per-event data such as timestamp, thread id, and value/id.
 * @param values Array of argument values. For LdcDiagTypeLogFormatted this points at the formatted
 *               string buffer.
 * @param valuesCount Number of values.
 *
 * @return Number of characters written (excluding null terminator) or 0 on failure.
 */
int ldcDiagnosticFormatLog(char* dst, uint32_t dstSize, const LdcDiagSite* site,
                           const LdcDiagRecord* record, const LdcDiagValue* values, uint32_t valuesCount);

/*! Format a trace/metric record as a Perfetto JSON event.
 *
 * @param dst Destination buffer.
 * @param dstSize Size of destination buffer.
 * @param site Static metadata describing the event (type, level, args).
 * @param record Per-event data such as timestamp, thread id, and value/id.
 * @param processId The process ID to record in any trace events.
 * @param values Array of argument values. For LdcDiagTypeLogFormatted this points at the formatted string buffer.
 * @param valuesCount Number of values.
 *
 * @return Number of characters written or 0 if the event is not traceable or did not fit.
 */
int ldcDiagnosticFormatJson(char* dst, uint32_t dstSize, const LdcDiagSite* site,
                            const LdcDiagRecord* record, uint32_t processId,
                            const LdcDiagValue* values, uint32_t valuesCount);

/*! Open a trace output file and register a Perfetto JSON handler.
 *
 * Writes the JSON array header and pushes an internal trace-file handler. Pair with
 * ldcDiagTraceFileRelease to close and remove the handler.
 *
 * @param filename File name for newly created trace file.
 *
 * @return True is trace file was initialized successfully.
 */
bool ldcDiagTraceFileInitialize(const char* filename);

/*! Remove the trace-file handler and close the trace output file.
 */
bool ldcDiagTraceFileRelease(void);

#ifdef __cplusplus
}
#endif

/* Tracing macros - C
 */
#if VN_SDK_FEATURE(TRACING)

/*! Begin a named trace duration event.
 */
#define VNTraceBegin(msg) _VNDiagEventNoArgs(LdcDiagTypeTraceBegin, LdcLogLevelNone, msg)
/*! Begin a named trace duration event with argument name/value pairs.
 *
 * Arguments are provided as: "name1", value1, "name2", value2, ...
 */
#define VNTraceBeginArgs(msg, ...) \
    _VNDiagEventNamedArgs(LdcDiagTypeTraceBegin, LdcLogLevelNone, msg, __VA_ARGS__)
/*! End the most recent trace duration event on the current thread.
 */
#define VNTraceEnd() ldcDiagEvent(&ldcDiagnosticsTraceScopedEndSite, 0)

/*! Begin a named trace duration event using the current function name.
 */
#define VNTraceScopedBegin() _VNDiagEvent(LdcDiagTypeTraceBegin, LdcLogLevelNone, __func__)
/*! Begin a named trace duration event using the current function name and arguments.
 *
 * Arguments are provided as: "name1", value1, "name2", value2, ...
 */
#define VNTraceScopedBeginArgs(...) \
    _VNDiagEventNamedArgs(LdcDiagTypeTraceBegin, LdcLogLevelNone, __func__, __VA_ARGS__)
/*! End the most recent trace duration event on the current thread.
 */
#define VNTraceScopedEnd() ldcDiagEvent(&ldcDiagnosticsTraceScopedEndSite, 0)

/*! Emit an instant trace event with the given name.
 */
#define VNTraceInstant(msg) _VNDiagEvent(LdcDiagTypeTraceInstant, LdcLogLevelNone, msg)
/*! Emit an instant trace event with argument name/value pairs.
 *
 * Arguments are provided as: "name1", value1, "name2", value2, ...
 */
#define VNTraceInstantArgs(msg, ...) \
    _VNDiagEventNamedArgs(LdcDiagTypeTraceInstant, LdcLogLevelNone, msg, __VA_ARGS__)

/*! Begin an async trace event with the given name and id.
 *
 * The id is used to correlate begin/end pairs.
 */
#define VNTraceAsyncBegin(msg, id) \
    _VNDiagEventId(LdcDiagTypeTraceAsyncBegin, LdcLogLevelNone, id, msg)
/*! Begin an async trace event with arguments and correlation id.
 *
 * Arguments are provided as: "name1", value1, "name2", value2, ...
 */
#define VNTraceAsyncBeginArgs(msg, id, ...) \
    _VNDiagEventIdNamedArgs(LdcDiagTypeTraceAsyncBegin, LdcLogLevelNone, id, msg, __VA_ARGS__)

/*! End an async trace event with the given name and id.
 */
#define VNTraceAsyncEnd(msg, id) \
    _VNDiagEventIdNoArgs(LdcDiagTypeTraceAsyncEnd, LdcLogLevelNone, id, msg)

/*! Emit an instant async trace event with the given name and id.
 */
#define VNTraceAsyncInstant(msg, id) \
    _VNDiagEventId(LdcDiagTypeTraceAsyncInstant, LdcLogLevelNone, id, msg)
/*! Emit an instant async trace event with arguments and correlation id.
 *
 * Arguments are provided as: "name1", value1, "name2", value2, ...
 */
#define VNTraceAsyncInstantArgs(msg, id, ...) \
    _VNDiagEventIdNamedArgs(LdcDiagTypeTraceAsyncInstant, LdcLogLevelNone, id, msg, __VA_ARGS__)

#else

#define VNTraceBegin(msg) (void)(msg)
#define VNTraceBeginArgs(msg, ...) (void)(msg)
#define VNTraceEnd() (void)(0)

#define VNTraceScopedBegin() (void)(0)
#define VNTraceScopedBeginArgs(...) (void)(0)
#define VNTraceScopedEnd() (void)(0)

#define VNTraceInstant(msg) (void)(msg)
#define VNTraceInstantArgs(msg, ...) (void)(msg)

#define VNTraceAsyncBegin(msg, id) ((void)(msg), (void)(id))
#define VNTraceAsyncBeginArgs(msg, id, ...) ((void)(msg), (void)(id))
#define VNTraceAsyncEnd(msg, id) ((void)(msg), (void)(id))

#define VNTraceAsyncInstant(msg, id) ((void)(msg), (void)(id))
#define VNTraceAsyncInstantArgs(msg, id, ...) ((void)(msg), (void)(id))

#endif

#ifdef __cplusplus

/* Tracing macros - C++
 */
#if VN_SDK_FEATURE(TRACING)

/* Scoped object to generate begin/end events
 * NB: Trailing semicolon is missing
 */

/*! Emit a scoped trace duration event for the current function.
 *
 * Emits a begin event immediately and an end event when the scope exits.
 */
#define VNTraceScoped()                                             \
    _VNDiagEvent(LdcDiagTypeTraceBegin, LdcLogLevelNone, __func__); \
    LdcTraceScoped _traceScoped

/*! Emit a scoped trace duration event with named arguments.
 *
 * Emits a begin event immediately and an end event when the scope exits.
 * Arguments are provided as: "name1", value1, "name2", value2, ...
 */
#define VNTraceScopedArgs(...)                                                              \
    _VNDiagEventNamedArgs(LdcDiagTypeTraceBegin, LdcLogLevelNone, __func__, ##__VA_ARGS__); \
    LdcTraceScoped _traceScoped

#else
#define VNTraceScoped() (void)(0)
#define VNTraceScopedArgs(...) (void)(0)
#endif
#endif

/* Metrics macros
 */
#if VN_SDK_FEATURE(METRICS)

/*! Record a signed 32-bit metric sample with the given name.
 */
#define VNMetricInt32(name, value) _VNTraceMetric(Int32, name, (int32_t)(value))
/*! Record an unsigned 32-bit metric sample with the given name.
 */
#define VNMetricUInt32(name, value) _VNTraceMetric(UInt32, name, (uint32_t)(value))
/*! Record a signed 64-bit metric sample with the given name.
 */
#define VNMetricInt64(name, value) _VNTraceMetric(Int64, name, (int64_t)(value))
/*! Record an unsigned 64-bit metric sample with the given name.
 */
#define VNMetricUInt64(name, value) _VNTraceMetric(UInt64, name, (uint64_t)(value))
/*! Record a 32-bit floating point metric sample with the given name.
 */
#define VNMetricFloat32(name, value) _VNTraceMetric(Float32, name, (float)(value))
/*! Record a 64-bit floating point metric sample with the given name.
 */
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
