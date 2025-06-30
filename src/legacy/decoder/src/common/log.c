/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

#include "common/log.h"

#include "common/memory.h"
#include "common/platform.h"
#include "context.h"

#include <LCEVC/legacy/PerseusDecoder.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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
} * Logger_t;

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
