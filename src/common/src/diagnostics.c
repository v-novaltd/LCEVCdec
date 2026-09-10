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

#include "string_format.h"

#include <LCEVC/common/limit.h>
#include <LCEVC/common/memory.h>
//
#include <LCEVC/common/check.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/diagnostics_buffer.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/printf_macros.h>
#include <LCEVC/common/stdlib.h>
#include <LCEVC/common/threads.h>

// Capacity of diagnostic ring buffer in records
#define VN_CAPACITY 1024

// Total bytes of extra variable data associated with diagnostic ring buffer
#define VN_VAR_DATA_SIZE 1024 * 1024

// Maximum number of values for diagnostic records
//
// NB: This also sets the limit for pre-formatted character strings.
#define VN_MAX_RECORD_VAR_COUNT 64

// Pointer to state to use - either allocated with this address space, or
// passed in from parent library.
//
DiagnosticState* ldcDiagnosticsState = NULL;

// Local diagnostic state if none provided by parent
static DiagnosticState localState;

// Lock for diagnostic state pointer
static ThreadMutex localDiagnosticsMutex = VNThreadMutexInit;

static inline void localDiagnosticsLock(void) { threadMutexLock(&localDiagnosticsMutex); }

static inline void localDiagnosticsUnlock(void) { threadMutexUnlock(&localDiagnosticsMutex); }

// Special site used to mark scoped trace ends
const LdcDiagSite ldcDiagnosticsTraceScopedEndSite = {.type = LdcDiagTypeTraceEnd};

// Apply all registered handlers to the given record - starting with most recently registered
//
// NB: Calling code should arrange appropriate locking
//
static void applyDiagnosticsHandlers(const LdcDiagSite* site, const LdcDiagRecord* record,
                                     const LdcDiagValue* values, uint32_t valuesCount)
{
    for (uint32_t i = ldcDiagnosticsState->handlersCount; i-- != 0;) {
        if (ldcDiagnosticsState->handlers[i].handler == NULL) {
            continue;
        }
        if (ldcDiagnosticsState->handlers[i].handler(ldcDiagnosticsState->handlers[i].userData,
                                                     site, record, values, valuesCount)) {
            break;
        }
    }
}

#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)

// Special site used to mark a flush in buffer
static const LdcDiagSite kDiagnosticsFlushSite = {.type = LdcDiagTypeFlush};

static intptr_t diagnosticThreadFn(void* argument)
{
    DiagnosticState* state = (DiagnosticState*)argument;
    assert(state);

    while (true) {
        // Pop record from buffer (diagnostic buffer is thread safe)
        LdcDiagRecord rec;
        LdcDiagValue values[VN_MAX_RECORD_VAR_COUNT];
        size_t varDataSize = 0;
        ldcDiagnosticsBufferPop(&state->diagnosticsBuffer, &rec, (uint8_t*)&values, sizeof(values),
                                &varDataSize);

        if (rec.site == NULL) {
            // Shutdown from ldcDiagnosticsRelease
            break;
        }

        // All the handler work is done under the per state mutex
        threadMutexLock(&state->mutex);

        applyDiagnosticsHandlers(rec.site, &rec, values,
                                 minU32(rec.site->argumentCount, VNArraySize(values)));

        if (rec.site == &kDiagnosticsFlushSite && state->flushCount > 0) {
            // Got a flush record - signal anything waiting when all pending flushes have been seen
            state->flushCount--;
            if (state->flushCount == 0) {
                threadCondVarSignal(&state->flushed);
            }
        }

        threadMutexUnlock(&state->mutex);
    }

    return 0;
}
#endif

void ldcDiagnosticsInitialize(void* parentState)
{
    localDiagnosticsLock();

    DiagnosticState* state = ldcDiagnosticsState;
    if (state == NULL) {
        state = parentState ? (DiagnosticState*)parentState : &localState;
    }

    const unsigned int previousRefCount = atomicFetchAddU32(&state->refCount, 1);
    ldcDiagnosticsState = state;

    if (previousRefCount == 0) {
        // Diagnostic state needs initializing
#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)
        state->flushCount = 0;
        ldcDiagnosticsBufferInitialize(&state->diagnosticsBuffer, VN_CAPACITY, VN_VAR_DATA_SIZE,
                                       ldcMemoryAllocatorMalloc());
        threadMutexInitialize(&state->mutex);
        threadMutexLock(&state->mutex);
        threadCondVarInitialize(&state->flushed);
        VNCheck(threadCreate(&state->thread, diagnosticThreadFn, (void*)state) == 0);
#endif

        state->handlersCount = 0;
        state->maxLogLevel = LdcLogLevelInfo;
        state->initialized = true;

#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)
        threadMutexUnlock(&state->mutex);
#endif
    }

    localDiagnosticsUnlock();
}

void ldcDiagnosticsRelease(void)
{
    localDiagnosticsLock();

    DiagnosticState* state = ldcDiagnosticsState;
    if (state == NULL) {
        localDiagnosticsUnlock();
        return;
    }

    const unsigned int previousRefCount = atomicFetchSubU32(&state->refCount, 1);

    if (previousRefCount > 1) {
        // DiagnosticState is still in use
        localDiagnosticsUnlock();
        return;
    }

    if (previousRefCount == 0) {
        // Clamp use count to 0 - release called too many times
        atomicStoreU32(&state->refCount, 0);
        ldcDiagnosticsState = NULL;
        localDiagnosticsUnlock();
        return;
    }

    assert(state->initialized);

#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)
    threadMutexLock(&state->mutex);
#endif

    // Mark as not initialized so that any output from below does not get generated
    state->initialized = false;

    state->handlersCount = 0;
    for (uint32_t i = 0; i < VNDiagnosticsMaxHandlers; ++i) {
        state->handlers[i].handler = NULL;
        state->handlers[i].userData = NULL;
    }

#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)
    // Close down thread by sending a null entry, and waiting for it to finish.
    LdcDiagRecord rec = {0};
    ldcDiagnosticsBufferPush(&state->diagnosticsBuffer, &rec, NULL, 0);
    threadMutexUnlock(&state->mutex);
    threadJoin(&state->thread, NULL);

    // Clear up
    threadCondVarDestroy(&state->flushed);
    threadMutexDestroy(&state->mutex);
    ldcDiagnosticsBufferDestroy(&state->diagnosticsBuffer);
#endif

    ldcDiagnosticsState = NULL;
    localDiagnosticsUnlock();
}

void ldcDiagnosticsFlush(void)
{
    assert(ldcDiagnosticsState);
    assert(ldcDiagnosticsState->initialized);

#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC) && VN_SDK_FEATURE(THREADING)
    threadMutexLock(&ldcDiagnosticsState->mutex);

    // Mark flush as pending
    ldcDiagnosticsState->flushCount++;

    // Push 'flush' record into buffer
    LdcDiagRecord rec = {0};
    rec.site = &kDiagnosticsFlushSite;
    ldcDiagnosticsBufferPush(&ldcDiagnosticsState->diagnosticsBuffer, &rec, NULL, 0);

    while (ldcDiagnosticsState->flushCount != 0) {
        threadCondVarWait(&ldcDiagnosticsState->flushed, &ldcDiagnosticsState->mutex);
    }

    threadMutexUnlock(&ldcDiagnosticsState->mutex);
#endif
}

void ldcDiagnosticsLogLevel(LdcLogLevel maxLevel)
{
    assert(maxLevel < LdcLogLevelCount);

    ldcDiagnosticsState->maxLogLevel = maxLevel;
}

bool ldcDiagnosticsHandlerPush(LdcDiagHandler* handler, void* userData)
{
    if (handler == NULL) {
        return false;
    }

    localDiagnosticsLock();
    if (!ldcDiagnosticsState || ldcDiagnosticsState->handlersCount >= VNDiagnosticsMaxHandlers) {
        // Too many handlers
        localDiagnosticsUnlock();
        assert(0);
        return false;
    }

#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)
    threadMutexLock(&ldcDiagnosticsState->mutex);
#endif
    ldcDiagnosticsState->handlers[ldcDiagnosticsState->handlersCount].handler = handler;
    ldcDiagnosticsState->handlers[ldcDiagnosticsState->handlersCount].userData = userData;
    ldcDiagnosticsState->handlersCount++;
#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)
    threadMutexUnlock(&ldcDiagnosticsState->mutex);
#endif
    localDiagnosticsUnlock();
    return true;
}

bool ldcDiagnosticsHandlerPop(LdcDiagHandler* handler, void** userData)
{
    if (handler == NULL) {
        return false;
    }

    localDiagnosticsLock();
    if (!ldcDiagnosticsState || ldcDiagnosticsState->handlersCount == 0) {
        // No more handlers
        localDiagnosticsUnlock();
        return false;
    }

    const void* inputUserData = (userData != NULL) ? *userData : NULL;
    // Look for a matching pointer, can't handle a NULL for both handler and userData

    bool ret = false;
#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)
    threadMutexLock(&ldcDiagnosticsState->mutex);
#endif
    for (uint32_t i = 0; i < ldcDiagnosticsState->handlersCount; i++) {
        if (ldcDiagnosticsState->handlers[i].handler == handler &&
            (inputUserData == NULL || ldcDiagnosticsState->handlers[i].userData == inputUserData)) {
            // Hand back the userData
            if (userData) {
                *userData = ldcDiagnosticsState->handlers[i].userData;
            }

            // Move the remaining handlers up and reduce the count
            ldcDiagnosticsState->handlersCount--;
            const uint32_t remaining = ldcDiagnosticsState->handlersCount - i;
            if (remaining) {
                memmove((void*)&ldcDiagnosticsState->handlers[i],
                        (void*)&ldcDiagnosticsState->handlers[i + 1],
                        remaining * sizeof(ldcDiagnosticsState->handlers[0]));
            }
            ret = true;
            break;
        }
    }
#if VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)
    threadMutexUnlock(&ldcDiagnosticsState->mutex);
#endif
    localDiagnosticsUnlock();
    assert(ret);

    return ret;
}

// Extract arguments from stack into an array of values
//
uint32_t ldcDiagnosticsCopyArguments(const LdcDiagSite* site, LdcDiagValue values[],
                                     uint32_t valuesCount, va_list args)
{
    const uint32_t count = minU32(site->argumentCount, valuesCount);

    for (uint32_t i = 0; i < count; ++i) {
        switch (site->argumentTypes[i]) {
            case LdcDiagArgId: {
                values[i].id = va_arg(args, uint64_t);
                break;
            }

            case LdcDiagArgBool: {
                values[i].valueBool = va_arg(args, int);
                break;
            }
            case LdcDiagArgChar: {
                values[i].valueChar = (char)va_arg(args, int);
                break;
            }
            case LdcDiagArgInt8: {
                values[i].valueInt8 = (int8_t)va_arg(args, int);
                break;
            }
            case LdcDiagArgUInt8: {
                values[i].valueUInt8 = (uint8_t)va_arg(args, unsigned int);
                break;
            }
            case LdcDiagArgInt16: {
                values[i].valueInt16 = (int16_t)va_arg(args, int);
                break;
            }
            case LdcDiagArgUInt16: {
                values[i].valueUInt16 = (uint16_t)va_arg(args, unsigned int);
                break;
            }
            case LdcDiagArgInt32: {
                values[i].valueInt32 = va_arg(args, int);
                break;
            }
            case LdcDiagArgUInt32: {
                values[i].valueUInt32 = va_arg(args, unsigned int);
                break;
            }
            case LdcDiagArgInt64: {
                values[i].valueInt64 = va_arg(args, int64_t);
                break;
            }
            case LdcDiagArgUInt64: {
                values[i].valueUInt64 = va_arg(args, uint64_t);
                break;
            }
            case LdcDiagArgCharPtr: {
                values[i].valueCharPtr = va_arg(args, char*);
                break;
            }
            case LdcDiagArgConstCharPtr: {
                values[i].valueConstCharPtr = va_arg(args, const char*);
                break;
            }
            case LdcDiagArgVoidPtr: {
                values[i].valueVoidPtr = va_arg(args, void*);
                break;
            }
            case LdcDiagArgConstVoidPtr: {
                values[i].valueConstVoidPtr = va_arg(args, const void*);
                break;
            }
#if VN_SDK_FEATURE(LDE_FLOATS)
            case LdcDiagArgFloat32: {
                // Float values are promoted to double in variadic calls.
                values[i].valueFloat32 = (float)va_arg(args, double);
                break;
            }
            case LdcDiagArgFloat64: {
                values[i].valueFloat64 = va_arg(args, double);
                break;
            }
#endif

            case LdcDiagArgNone:
                // Handles weird case with MSVC generating extra arguments in VNLog() expansion
                break;

            default: assert(0); break;
        }
    }

    return count;
}

#if !VN_SDK_FEATURE(DIAGNOSTICS_ASYNC)

// Apply diagnostic handlers whilst holding the overall diagnosticState lock
static inline void applyDiagnosticsHandlersLocked(const LdcDiagSite* site, const LdcDiagRecord* record,
                                                  const LdcDiagValue* values, uint32_t valuesCount)
{
    localDiagnosticsLock();
    applyDiagnosticsHandlers(site, record, values, valuesCount);
    localDiagnosticsUnlock();
}

// All events are synchronous
//
void ldcDiagEvent(const LdcDiagSite* site, size_t valuesSize, ...)
{
    if (!site || site->level > ldcDiagnosticsState->maxLogLevel) {
        return;
    }

    LdcDiagRecord record = {0};
    ldcDiagnosticsRecordSet(&record, site);

    // Extract arguments into a value array
    LdcDiagValue values[VN_MAX_RECORD_VAR_COUNT];

    va_list args;
    va_start(args, valuesSize);
    const uint32_t count = ldcDiagnosticsCopyArguments(site, values, VNArraySize(values), args);
    va_end(args);

    applyDiagnosticsHandlersLocked(site, &record, values, count);
}

void ldcDiagEventFormatted(const LdcDiagSite* site, const char* fmt, ...)
{
    if (!site || site->level > ldcDiagnosticsState->maxLogLevel) {
        return;
    }

    LdcDiagRecord record = {0};
    ldcDiagnosticsRecordSet(&record, site);

    char buffer[4096];

    va_list args;
    va_start(args, fmt);
    record.size = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    applyDiagnosticsHandlersLocked(site, &record, (LdcDiagValue*)buffer,
                                   sizeof(buffer) / sizeof(LdcDiagValue));
}

void ldcTracingScopedBegin(const LdcDiagSite* site)
{
    LdcDiagRecord record = {0};
    ldcDiagnosticsRecordSetAll(&record, site, 1, 0);

    applyDiagnosticsHandlersLocked(site, &record, NULL, 0);
}

void ldcTracingScopedEnd(const LdcDiagSite* site)
{
    LdcDiagRecord record = {0};
    ldcDiagnosticsRecordSetAll(&record, site, 0, 0);

    applyDiagnosticsHandlersLocked(site, &record, NULL, 0);
}

void ldcTracingEvent(const LdcDiagSite* site, size_t valuesSize, ...)
{
    LdcDiagRecord record = {0};
    ldcDiagnosticsRecordSet(&record, site);

    // Extract arguments into a value array
    LdcDiagValue values[VN_MAX_RECORD_VAR_COUNT];

    va_list args;
    va_start(args, valuesSize);
    const uint32_t count = ldcDiagnosticsCopyArguments(site, values, VNArraySize(values), args);
    va_end(args);

    applyDiagnosticsHandlersLocked(site, &record, values, count);
}

void ldcMetricInt32(const LdcDiagSite* site, int32_t value)
{
    LdcDiagRecord record = {0};
    ldcDiagnosticsRecordSetAll(&record, site, 0, 0);
    record.value.valueInt32 = value;

    applyDiagnosticsHandlers(site, &record, NULL, 0);
}

void ldcMetricUInt32(const LdcDiagSite* site, uint32_t value)
{
    LdcDiagRecord record = {0};
    ldcDiagnosticsRecordSetAll(&record, site, 0, 0);
    record.value.valueUInt32 = value;

    applyDiagnosticsHandlersLocked(site, &record, NULL, 0);
}

void ldcMetricInt64(const LdcDiagSite* site, int64_t value)
{
    LdcDiagRecord record = {0};
    ldcDiagnosticsRecordSetAll(&record, site, 0, 0);
    record.value.valueInt64 = value;

    applyDiagnosticsHandlersLocked(site, &record, NULL, 0);
}

void ldcMetricUInt64(const LdcDiagSite* site, uint64_t value)
{
    LdcDiagRecord record = {0};
    ldcDiagnosticsRecordSetAll(&record, site, 0, 0);
    record.value.valueUInt64 = value;

    applyDiagnosticsHandlersLocked(site, &record, NULL, 0);
}

void ldcMetricFloat32(const LdcDiagSite* site, float value)
{
    LdcDiagRecord record = {0};
    ldcDiagnosticsRecordSetAll(&record, site, 0, 0);
    record.value.valueFloat32 = value;

    applyDiagnosticsHandlersLocked(site, &record, NULL, 0);
}

void ldcMetricFloat64(const LdcDiagSite* site, double value)
{
    LdcDiagRecord record = {0};
    ldcDiagnosticsRecordSetAll(&record, site, 0, 0);
    record.value.valueFloat64 = value;

    applyDiagnosticsHandlersLocked(site, &record, NULL, 0);
}

#endif
