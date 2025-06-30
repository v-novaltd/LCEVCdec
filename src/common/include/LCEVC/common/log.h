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

#ifndef VN_LCEVC_COMMON_LOG_H
#define VN_LCEVC_COMMON_LOG_H

#include <LCEVC/common/diagnostics.h>

// Logging macros
//
// Normal log
#define VNLog(level, msg, ...) _VNDiagEvent(LdcDiagTypeLog, level, msg, ##__VA_ARGS__)

// Formatted log - does formatting on calling thread, but handles large numbers of arguments and dynamic strings
#define VNLogF(level, fmt, ...)                                                      \
    do {                                                                             \
        static const LdcDiagSite site = {                                            \
            LdcDiagTypeLogFormatted, __FILE__, __LINE__, level, NULL, 0, NULL, NULL, \
            LdcDiagArgConstCharPtr};                                                 \
        ldcLogEventFormatted(&site, fmt, ##__VA_ARGS__);                             \
    } while (0)

// Level specific macros - enabled by top level build option.
//
#if defined(VN_SDK_LOG_ENABLE_FATAL)
#define VNLogFatal(msg, ...)                         \
    do {                                             \
        VNLog(LdcLogLevelFatal, msg, ##__VA_ARGS__); \
        abort();                                     \
    } while (0)
#define VNLogFatalF(msg, ...)                         \
    do {                                              \
        VNLogF(LdcLogLevelFatal, msg, ##__VA_ARGS__); \
        abort();                                      \
    } while (0)
#else
#define VNLogFatal(msg, ...) VNUnused(msg)
#define VNLogFatalF(msg, ...) VNUnused(msg)
#endif

#if defined(VN_SDK_LOG_ENABLE_ERROR)
#define VNLogError(msg, ...)                         \
    do {                                             \
        VNLog(LdcLogLevelError, msg, ##__VA_ARGS__); \
    } while (0)
#define VNLogErrorF(msg, ...)                         \
    do {                                              \
        VNLogF(LdcLogLevelError, msg, ##__VA_ARGS__); \
    } while (0)
#else
#define VNLogError(msg, ...) VNUnused(msg)
#define VNLogErrorF(msg, ...) VNUnused(msg)
#endif

#if defined(VN_SDK_LOG_ENABLE_WARNING)
#define VNLogWarning(msg, ...)                         \
    do {                                               \
        VNLog(LdcLogLevelWarning, msg, ##__VA_ARGS__); \
    } while (0)
#define VNLogWarningF(msg, ...)                         \
    do {                                                \
        VNLogF(LdcLogLevelWarning, msg, ##__VA_ARGS__); \
    } while (0)
#else
#define VNLogWarning(msg, ...) VNUnused(msg)
#define VNLogWarningF(msg, ...) VNUnused(msg)
#endif

#if defined(VN_SDK_LOG_ENABLE_INFO)
#define VNLogInfo(msg, ...)                         \
    do {                                            \
        VNLog(LdcLogLevelInfo, msg, ##__VA_ARGS__); \
    } while (0)
#define VNLogInfoF(msg, ...)                         \
    do {                                             \
        VNLogF(LdcLogLevelInfo, msg, ##__VA_ARGS__); \
    } while (0)
#else
#define VNLogInfo(msg, ...) VNUnused(msg)
#define VNLogInfoF(msg, ...) VNUnused(msg)
#endif

#if defined(VN_SDK_LOG_ENABLE_DEBUG)
#define VNLogDebug(msg, ...)                         \
    do {                                             \
        VNLog(LdcLogLevelDebug, msg, ##__VA_ARGS__); \
    } while (0)
#define VNLogDebugF(msg, ...)                         \
    do {                                              \
        VNLogF(LdcLogLevelDebug, msg, ##__VA_ARGS__); \
    } while (0)
#else
#define VNLogDebug(msg, ...) VNUnused(msg)
#define VNLogDebugF(msg, ...) VNUnused(msg)
#endif

#if defined(VN_SDK_LOG_ENABLE_VERBOSE)
#define VNLogVerbose(msg, ...)                         \
    do {                                               \
        VNLog(LdcLogLevelVerbose, msg, ##__VA_ARGS__); \
    } while (0)
#define VNLogVerboseF(msg, ...)                         \
    do {                                                \
        VNLogF(LdcLogLevelVerbose, msg, ##__VA_ARGS__); \
    } while (0)
#else
#define VNLogVerbose(msg, ...) VNUnused(msg)
#define VNLogVerboseF(msg, ...) VNUnused(msg)
#endif

#endif // VN_LCEVC_COMMON_LOG_H
