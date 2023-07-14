/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "common/memory.h"
#include "common/platform.h"
#include "context.h"

#include <stdarg.h>

/*------------------------------------------------------------------------------*/

#define VN_FORMATBUFFER_SIZE() 16384
static VN_THREADLOCAL() char tlsFormatBuffer[2][VN_FORMATBUFFER_SIZE()];

/*------------------------------------------------------------------------------*/

typedef struct Logger
{
    Memory_t memory;
    perseus_decoder_log_callback callback;
    void* userData;
    bool enableLocation;
}* Logger_t;

/*------------------------------------------------------------------------------*/

bool logInitialize(Memory_t memory, Logger_t* logger, const LoggerSettings_t* settings)
{
    Logger_t result = VN_CALLOC_T(memory, struct Logger);

    if (!result) {
        return false;
    }

    result->memory = memory;

    if (settings) {
        result->callback = settings->callback;
        result->userData = settings->userData;
        result->enableLocation = settings->enableLocation;
    }

    *logger = result;

    return true;
}

void logRelease(Logger_t logger) { VN_FREE(logger->memory, logger); }

void logPrint(Logger_t logger, LogType_t type, const char* fn, uint32_t line, const char* format, ...)
{
    if (!logger->callback) {
        return;
    }

    /* Format initial buffer based upon user supplied args.*/
    va_list args;
    va_start(args, format);
    int32_t formatLength = vsnprintf(tlsFormatBuffer[0], VN_FORMATBUFFER_SIZE(), format, args);
    va_end(args);

    /* Format in line info if that is what the user wants. */
    int32_t outputIndex = 0;
    if (logger->enableLocation) {
        formatLength = snprintf(tlsFormatBuffer[1], VN_FORMATBUFFER_SIZE(), "%s (%u): %s", fn, line,
                                tlsFormatBuffer[0]);
        outputIndex = 1;
    }

    if ((formatLength > 0) && (formatLength < VN_FORMATBUFFER_SIZE())) {
        logger->callback(logger->userData, (int32_t)type, tlsFormatBuffer[outputIndex], (size_t)formatLength);
    }
}

/*------------------------------------------------------------------------------*/
