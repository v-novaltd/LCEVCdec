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

// Diagnostic handler that writes log records to stdout
//
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/printf_macros.h>
#include <LCEVC/common/stdlib.h>

// Append a formatted string to a character span - updating span start and size
//
// Returns false if overflow, and leaves span start/size unchanged.
//
typedef struct CharSpan
{
    char* start;
    size_t size;
} CharSpan;

static bool appendFormat(CharSpan* span, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int numChars = vsnprintf(span->start, span->size, fmt, args);
    va_end(args);
    if ((size_t)numChars > span->size) {
        return false;
    }

    span->start += numChars;
    span->size -= numChars;
    return true;
}
// Convert diagnostic event arguments into json dictionary string
//
static void argumentsFormat(CharSpan* dst, uint32_t count, const LdcDiagArg* types,
                            const char* const * names, const LdcDiagValue* values)
{
    appendFormat(dst, "{ ");
    for (uint32_t arg = 0; arg < count; arg++) {
        if (arg > 0) {
            appendFormat(dst, ", ");
        }

        switch (types[arg]) {
            case LdcDiagArgInt8:
                appendFormat(dst, "\"%s\": %hhd", names[arg], values[arg].valueInt8);
                break;
            case LdcDiagArgUInt8:
                appendFormat(dst, "\"%s\": %hhu", names[arg], values[arg].valueUInt8);
                break;
            case LdcDiagArgInt16:
                appendFormat(dst, "\"%s\": %hd", names[arg], values[arg].valueInt16);
                break;
            case LdcDiagArgUInt16:
                appendFormat(dst, "\"%s\": %hu", names[arg], values[arg].valueUInt16);
                break;
            case LdcDiagArgInt32:
                appendFormat(dst, "\"%s\": %d", names[arg], values[arg].valueInt32);
                break;
            case LdcDiagArgUInt32:
                appendFormat(dst, "\"%s\": %u", names[arg], values[arg].valueUInt32);
                break;
            case LdcDiagArgInt64:
                appendFormat(dst, "\"%s\": %" PRId64, names[arg], values[arg].valueInt64);
                break;
            case LdcDiagArgUInt64:
                appendFormat(dst, "\"%s\": %" PRIu64, names[arg], values[arg].valueUInt64);
                break;
#if VN_SDK_FEATURE(LDE_FLOATS)
            case LdcDiagArgFloat32:
                appendFormat(dst, "\"%s\": %g", names[arg], values[arg].valueFloat32);
                break;
            case LdcDiagArgFloat64:
                appendFormat(dst, "\"%s\": %g", names[arg], values[arg].valueFloat64);
                break;
#endif
            case LdcDiagArgId:
                appendFormat(dst, "\"%s\": %" PRIu64, names[arg], values[arg].id);
                break;
            case LdcDiagArgBool:
                appendFormat(dst, "\"%s\": %s", names[arg], values[arg].valueBool ? "true" : "false");
                break;
            case LdcDiagArgChar:
                appendFormat(dst, "\"%s\": %d", names[arg], values[arg].valueChar);
                break;
            case LdcDiagArgCharPtr:
                appendFormat(dst, "\"%s\": \"%s\"", names[arg],
                             values[arg].valueCharPtr ? values[arg].valueCharPtr : "null");
                break;
            case LdcDiagArgConstCharPtr:
                appendFormat(dst, "\"%s\": \"%s\"", names[arg],
                             values[arg].valueConstCharPtr ? values[arg].valueConstCharPtr : "null");
                break;
            case LdcDiagArgVoidPtr:
                appendFormat(dst, "\"%s\": \"%p\"", names[arg], values[arg].valueVoidPtr);
                break;
            case LdcDiagArgConstVoidPtr:
                appendFormat(dst, "\"%s\": \"%p\"", names[arg], values[arg].valueConstVoidPtr);
                break;
            default: break;
        }
    }
}

// Convert a tracing or metric record to Perfetto JSON
//
int ldcDiagnosticFormatJson(char* dstStart, uint32_t dstSize, const LdcDiagSite* site,
                            const LdcDiagRecord* record, uint32_t processId,
                            const LdcDiagValue* values, uint32_t valuesCount)
{
    VNUnused(valuesCount);
    static const char* valueName[] = {"value"};
    static const char* memoryNames[] = {"size", "address", "prevAdress"};

    // Optional things that can go into final record
    const char* phase = NULL;
    const char* name = NULL;
    const uint64_t* id = NULL;
    uint32_t argCount = 0;
    const LdcDiagArg* argTypes = NULL;
    const char* const * argNames = NULL;
    const LdcDiagValue* argValues = NULL;

    switch (site->type) {
        // Filter out records that do not generate any output
        case LdcDiagTypeNone:
        case LdcDiagTypeFlush:
        case LdcDiagTypeLog:
        case LdcDiagTypeLogFormatted: break;

        // Tracing
        case LdcDiagTypeTraceBegin:
            phase = "B";
            name = site->str;
            argCount = site->argumentCount;
            argTypes = site->argumentTypes;
            argNames = site->argumentNames;
            argValues = values;
            break;
        case LdcDiagTypeTraceEnd:
            //
            phase = "E";
            break;

        case LdcDiagTypeTraceInstant:
            phase = "i";
            name = site->str;
            argCount = site->argumentCount;
            argTypes = site->argumentTypes;
            argNames = site->argumentNames;
            argValues = values;
            break;

        // Async events
        case LdcDiagTypeTraceAsyncBegin:
            phase = "b";
            name = site->str;
            if (site->argumentCount > 0 && site->argumentTypes[0] == LdcDiagArgId) {
                id = &values[0].id;
                argCount = site->argumentCount - 1;
                argTypes = site->argumentTypes + 1;
                argNames = site->argumentNames + 1;
                argValues = values + 1;
            }
            break;
        case LdcDiagTypeTraceAsyncEnd:
            phase = "e";
            name = site->str;
            if (site->argumentCount > 0 && site->argumentTypes[0] == LdcDiagArgId) {
                id = &values[0].id;
            }
            break;
        case LdcDiagTypeTraceAsyncInstant:
            phase = "n";
            name = site->str;
            if (site->argumentCount > 0 && site->argumentTypes[0] == LdcDiagArgId) {
                id = &values[0].id;
                argCount = site->argumentCount - 1;
                argTypes = site->argumentTypes + 1;
                argNames = site->argumentNames + 1;
                argValues = values + 1;
            }
            break;

        // Metrics
        case LdcDiagTypeMetric:
            phase = "C";
            name = site->str;
            argCount = 1;
            argTypes = &site->valueType;
            argNames = valueName;
            argValues = &record->value;
            break;

        // Memory
        case LdcDiagTypeMemoryAllocate:
            phase = "HA";
            name = site->str;
            if (site->argumentCount == 3 && site->argumentTypes[0] == LdcDiagArgId) {
                id = &values[0].id;
                argCount = 2;
                argTypes = site->argumentTypes + 1;
                argNames = memoryNames;
                argValues = values + 1;
            }
            break;

        case LdcDiagTypeMemoryReallocate:
            phase = "HR";
            name = site->str;
            if (site->argumentCount == 4 && site->argumentTypes[0] == LdcDiagArgId) {
                id = &values[0].id;
                argCount = 3;
                argTypes = site->argumentTypes + 1;
                argNames = memoryNames;
                argValues = values + 1;
            }
            break;

        case LdcDiagTypeMemoryFree:
            phase = "HF";
            name = site->str;
            if (site->argumentCount == 3 && site->argumentTypes[0] == LdcDiagArgId) {
                id = &values[0].id;
                argCount = 2;
                argTypes = site->argumentTypes + 1;
                argNames = memoryNames;
                argValues = values + 1;
            }
            break;
    }

    if (!phase) {
        // This record does not go into trace
        return 0;
    }

    // Accumulate string in this span
    CharSpan dst = {dstStart, dstSize};

// Common part of record
#if VN_SDK_FEATURE(LDE_FLOATS)
    double microSeconds = ((double)record->timestamp) / 1000.0;
    appendFormat(&dst, "{\"ph\":\"%s\", \"ts\":%.3f, \"pid\":%u, \"tid\":%u", phase, microSeconds,
                 processId, record->threadId);
#else
    uint64_t microSeconds = record->timestamp / 1000;
    appendFormat(&dst, "{\"ph\":\"%s\", \"ts\":" PRIu64 ", \"pid\":%u, \"tid\":%u", phase,
                 microSeconds, processId, record->threadId);
#endif

    // Optional parts
    if (name) {
        appendFormat(&dst, ", \"name\":\"%s\"", name);
    }
    if (id) {
        appendFormat(&dst, ", \"id\":%" PRIu64 " ", *id);
    }
    if (argCount > 0 && argTypes != NULL && argValues != NULL) {
        appendFormat(&dst, ", \"args\": ");
        argumentsFormat(&dst, argCount, argTypes, argNames, argValues);
        appendFormat(&dst, "}");
    }

    if (appendFormat(&dst, "},\n")) {
        // Managed to fit end of record into destination - all good
        return (int)(dstSize - dst.size);
    }

    // Run out of space in destination - return 0 chars produced - no point in having
    // a broken record.
    return 0;
}

#if VN_SDK_FEATURE(STD_FILE)

static bool diagnosticHandlerTraceFile(void* user, const LdcDiagSite* site, const LdcDiagRecord* record,
                                       const LdcDiagValue* values, uint32_t valuesCount)
{
    FILE* output = user;

    if (site->type == LdcDiagTypeFlush) {
        fflush(output);
    }

    char buffer[4096] = {0}; // Could make thread local?
    if (ldcDiagnosticFormatJson(buffer, sizeof(buffer), site, record, VNGetProcessId(), values,
                                valuesCount) > 0) {
        fputs(buffer, output);
        return true;
    }
    return false;
}

bool ldcDiagTraceFileInitialize(const char* filename)
{
    FILE* output = fopen(filename, "wb");
    if (!output) {
        VNLogError("Cannot open trace file");
        return false;
    }

    fputs("[\n", output);

    ldcDiagnosticsHandlerPush(diagnosticHandlerTraceFile, output);

    return true;
}

bool ldcDiagTraceFileRelease(void)
{
    void* userData = 0;
    if (!ldcDiagnosticsHandlerPop(diagnosticHandlerTraceFile, &userData)) {
        VNLogError("Cannot pop diagnostics handler");
        return false;
    }

    // Recover file handle
    assert(userData);
    FILE* output = (FILE*)userData;

    // Terminate the log and close
    if (fputs("]\n", output) < 0 || fclose(output) < 0) {
        VNLogError("Cannot close trace file");
        return false;
    }

    return true;
}

#else

bool ldcDiagTraceFileInitialize(const char* filename)
{
    VNUnused(filename);
    return true;
}
bool ldcDiagTraceFileRelease(void) { return true; }

#endif // VN_SDK_FEATURE(FILE)
