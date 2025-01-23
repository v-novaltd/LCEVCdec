/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
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

#ifndef VN_DEC_CORE_LOG_H_
#define VN_DEC_CORE_LOG_H_

#include "common/platform.h"
#include "LCEVC/PerseusDecoder.h"

/*------------------------------------------------------------------------------*/

typedef struct Memory* Memory_t;

/*! \brief Opaque handle for the internal logger. */
typedef struct Logger* Logger_t;

/*------------------------------------------------------------------------------*/

typedef enum LogType
{
    LTError,
    LTWarning,
    LTInfo,
    LTDebug,
    LTVerbose,
    LTUnknown
} LogType_t;

/*! \brief Logging interface initialization settings. */
typedef struct LoggerSettings
{
    perseus_decoder_log_callback callback; /**< Pointer to function to call for each fully formatted log message. */
    void* userData;                        /**< Pointer to user data to pass through the callback */
    bool enableLocation; /**< If true all log messages are prefixed with the source location the log message. */
} LoggerSettings_t;

/*! \brief Create an instance of the logger. */
bool logInitialize(Memory_t memory, Logger_t* logger, const LoggerSettings_t* settings);

/*! \brief Destroy an instance of the logger. */
void logRelease(Logger_t logger);

/*! \brief Perform printf style logging using variadic arguments. */
void logPrint(Logger_t logger, LogType_t type, const char* fn, uint32_t line, const char* format, ...);

/*------------------------------------------------------------------------------
 Logging macros - these should be used for all internal logging.
 -----------------------------------------------------------------------------*/

#define VN_LOG_OUTPUT(logger, type, msg, ...) \
    logPrint(logger, type, __FILE__, __LINE__, msg, ##__VA_ARGS__)

#define VN_LOG(logger, type, msg, ...) VN_LOG_OUTPUT(logger, type, msg, ##__VA_ARGS__)
#define VN_VERBOSE(logger, msg, ...) VN_LOG_OUTPUT(logger, LTVerbose, msg, ##__VA_ARGS__)
#define VN_DEBUG(logger, msg, ...) VN_LOG_OUTPUT(logger, LTDebug, msg, ##__VA_ARGS__)
#define VN_INFO(logger, msg, ...) VN_LOG_OUTPUT(logger, LTInfo, msg, ##__VA_ARGS__)
#define VN_WARNING(logger, msg, ...) VN_LOG_OUTPUT(logger, LTWarning, msg, ##__VA_ARGS__)
#define VN_ERROR(logger, msg, ...) VN_LOG_OUTPUT(logger, LTError, msg, ##__VA_ARGS__)

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_LOG_H_ */
