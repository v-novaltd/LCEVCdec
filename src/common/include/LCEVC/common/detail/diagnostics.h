/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#include <LCEVC/common/diagnostics_buffer.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/threads.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
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

    // Global maximum log level
    //
    LdcLogLevel maxLogLevel;

    //
    bool initialized;

#if VN_OS(WINDOWS)
    LARGE_INTEGER performanceCounterFrequency;
#endif

#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)
    Thread thread;
    ThreadMutex mutex;
    uint32_t flushCount;
    ThreadCondVar flushed;

    LdcDiagnosticsBuffer diagnosticsBuffer;
#endif
} DiagnosticState;

extern DiagnosticState* ldcDiagnosticsState;

//
static inline void* ldcDiagnosticsStateGet(void) { return ldcDiagnosticsState; }

// Get a time for diagnostic records
//
static inline uint64_t ldcDiagnosticGetTimestamp(void)
{
#if VN_OS(WINDOWS)
    assert(ldcDiagnosticsState->performanceCounterFrequency.QuadPart != 0);

    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    return (currentTime.QuadPart * 1000000000) / ldcDiagnosticsState->performanceCounterFrequency.QuadPart;
#else
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime);
    return ((uint64_t)(currentTime.tv_sec) * 1000000000) + (uint64_t)currentTime.tv_nsec;
#endif
}

// Fill in LdcDiagRecord
static inline void ldcDiagnosticsRecordSet(LdcDiagRecord* record, const LdcDiagSite* site)
{
    record->site = site;
    record->timestamp = ldcDiagnosticGetTimestamp();
    record->threadId = (uint32_t)VNGetThreadId();
}

// Fill in entire LdcDiagRecord
static inline void ldcDiagnosticsRecordSetAll(LdcDiagRecord* record, const LdcDiagSite* site,
                                              int id, uint32_t size)
{
    record->site = site;
    record->timestamp = ldcDiagnosticGetTimestamp();
    record->threadId = (uint32_t)VNGetThreadId();
    record->size = size;
    record->value.id = id;
}

// Out of line utility to copy VA args into diag record variable data
void ldcDiagnosticsCopyArguments(const LdcDiagSite* site, LdcDiagValue values[], va_list args);

#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)

// The inline implementations of the underlying diagnostics entry points
//
static inline void ldcLogEvent(const LdcDiagSite* site, size_t valuesSize, ...)
{
    if (!ldcDiagnosticsState->initialized || site->level > ldcDiagnosticsState->maxLogLevel) {
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
        ldcDiagnosticsCopyArguments(site, values, args);
        va_end(args);

        ldcDiagnosticsBufferPushEnd(&ldcDiagnosticsState->diagnosticsBuffer);
    }
}

static inline void ldcLogEventFormatted(const LdcDiagSite* site, const char* fmt, ...)
{
    if (site->level > ldcDiagnosticsState->maxLogLevel) {
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

static inline void ldcTracingScopedBegin(const LdcDiagSite* site)
{
    if (!ldcDiagnosticsState->initialized) {
        return;
    }

    LdcDiagRecord* rec = ldcDiagnosticsBufferPushBegin(&ldcDiagnosticsState->diagnosticsBuffer, 0);
    ldcDiagnosticsRecordSetAll(rec, site, 1, 0);
    ldcDiagnosticsBufferPushEnd(&ldcDiagnosticsState->diagnosticsBuffer);
}

static inline void ldcTracingScopedEnd(const LdcDiagSite* site)
{
    if (!ldcDiagnosticsState->initialized) {
        return;
    }

    LdcDiagRecord* rec = ldcDiagnosticsBufferPushBegin(&ldcDiagnosticsState->diagnosticsBuffer, 0);
    ldcDiagnosticsRecordSetAll(rec, site, 0, 0);
    ldcDiagnosticsBufferPushEnd(&ldcDiagnosticsState->diagnosticsBuffer);
}

static inline void ldcTracingEvent(const LdcDiagSite* site, size_t valuesSize, ...)
{
    if (!ldcDiagnosticsState->initialized) {
        return;
    }

    if (valuesSize == 0) {
        // Special case this path so that inlining can simplify this fn
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
        ldcDiagnosticsCopyArguments(site, values, args);
        va_end(args);

        ldcDiagnosticsBufferPushEnd(&ldcDiagnosticsState->diagnosticsBuffer);
    }
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

#endif // VN_LCEVC_COMMON_DETAIL_DIAGNOSTICS_H
