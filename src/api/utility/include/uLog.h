/* Copyright (c) V-Nova International Limited 2022-2023. All rights reserved. */
#pragma once

#include "uTypes.h"

#include <stdarg.h>

/*------------------------------------------------------------------------------*/
#ifndef VNEnableLog
#define VNEnableLog 1
#endif

typedef enum LogType
{
    LogType_Disabled,
    LogType_Error,
    LogType_Warning,
    LogType_Info,
    LogType_Debug,
    LogType_Verbose
} LogType;

typedef void (*LogCallback)(void* userptr, LogType type, const char* msg);

/*------------------------------------------------------------------------------*/

LogType log_get_verbosity();
void log_set_verbosity(LogType type);
void log_set_callback(LogCallback callback, void* userptr);
void log_set_filepath(const char* filepath);
bool log_get_enable_stdout();
void log_set_enable_stdout(bool bEnable);
void log_set_enable_function_names(bool bEnable);
void log_close();

void log_print(LogType type, const char* function, uint32_t line, const char* format, ...);
void log_printv(LogType type, const char* function, uint32_t line, const char* format, va_list ap);

#ifndef VN_ULOG_NO_MACROS

/*------------------------------------------------------------------------------*/

#define VNLogCustomFnName(type, msg, fnName, ...)       \
    do {                                                \
        log_print(type, fnName, 0, msg, ##__VA_ARGS__); \
    } while (0)

#define VNLog(type, msg, ...)                                        \
    do {                                                             \
        log_print(type, __FUNCTION__, __LINE__, msg, ##__VA_ARGS__); \
    } while (0)

#define VNLogError(msg, ...) VNLog(LogType_Error, msg, ##__VA_ARGS__)

#define VNLogWarning(msg, ...)                          \
    do {                                                \
        if (!!VNEnableLog) {                            \
            VNLog(LogType_Warning, msg, ##__VA_ARGS__); \
        }                                               \
    } while (0)

#define VNLogInfo(msg, ...)                          \
    do {                                             \
        if (!!VNEnableLog) {                         \
            VNLog(LogType_Info, msg, ##__VA_ARGS__); \
        }                                            \
    } while (0)

#define VNLogDebug(msg, ...)                          \
    do {                                              \
        if (!!VNEnableLog) {                          \
            VNLog(LogType_Debug, msg, ##__VA_ARGS__); \
        }                                             \
    } while (0)

#define VNLogVerbose(msg, ...)                          \
    do {                                                \
        if (!!VNEnableLog) {                            \
            VNLog(LogType_Verbose, msg, ##__VA_ARGS__); \
        }                                               \
    } while (0)

/*------------------------------------------------------------------------------*/

#define VNCheck(op)                           \
    if (!(op)) {                              \
        VNLogError("Call failed: %s\n", #op); \
    }

#define VNCheckB(op)                          \
    if (!(op)) {                              \
        VNLogError("Call failed: %s\n", #op); \
        return false;                         \
    }

#define VNCheckP(op)                          \
    if (!(op)) {                              \
        VNLogError("Call failed: %s\n", #op); \
        return nullptr;                       \
    }

#define VNCheckI(op)                          \
    if (!(op)) {                              \
        VNLogError("Call failed: %s\n", #op); \
        return -1;                            \
    }

/*------------------------------------------------------------------------------*/

#endif // VN_ULOG_NO_MACROS