/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "uLog.h"

#include "uChrono.h"
#include "uPlatform.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#endif

#if defined(__APPLE__)
#include <os/log.h>
#endif

/*------------------------------------------------------------------------------*/

typedef struct LogContext
{
    LogCallback callback;
    void* callback_user;
    bool enable_stdout;
    bool enable_function_names;
    FILE* file_output;
    LogType verbosity;
#if defined(__APPLE__)
    os_log_t osLogObject;
#endif

    LogContext()
        : callback(nullptr)
        , callback_user(nullptr)
        , enable_stdout(false)
        , enable_function_names(true)
        , file_output(nullptr)
        , verbosity(LogType_Disabled)
    {
#if defined(__APPLE__)
        osLogObject = os_log_create("com.vnova.logger", "VNOVA");
#endif
    }

    ~LogContext()
    {
        if (file_output) {
            fclose(file_output);
        }
#if defined(__APPLE__)
        os_release(osLogObject);
#endif
    }
} Logger;

static LogContext sLog = LogContext();
static VNThreadLocal char sFormatBuffer[2][16384];

/*------------------------------------------------------------------------------*/

#ifdef __ANDROID__
static android_LogPriority GetAndroidLogPriority(const LogType type)
{
    switch (type) {
        case LogType_Disabled: return ANDROID_LOG_UNKNOWN;
        case LogType_Error: return ANDROID_LOG_ERROR;
        case LogType_Warning: return ANDROID_LOG_WARN;
        case LogType_Info: return ANDROID_LOG_INFO;
        case LogType_Debug: return ANDROID_LOG_DEBUG;
        case LogType_Verbose: return ANDROID_LOG_VERBOSE;
    }

    return ANDROID_LOG_UNKNOWN;
}
#endif

#if defined(__APPLE__)
static os_log_type_t GetAppleLogPriority(const LogType type)
{
    switch (type) {
        case LogType_Disabled:
        case LogType_Error: return OS_LOG_TYPE_FAULT;
        case LogType_Warning: return OS_LOG_TYPE_ERROR;
        case LogType_Info: return OS_LOG_TYPE_DEFAULT;
        case LogType_Debug: return OS_LOG_TYPE_DEBUG;
        case LogType_Verbose: return OS_LOG_TYPE_INFO;
    }
    return OS_LOG_TYPE_DEFAULT;
}
#endif

/*------------------------------------------------------------------------------*/

LogType log_get_verbosity() { return sLog.verbosity; }

void log_set_verbosity(LogType type) { sLog.verbosity = type; }

void log_set_callback(LogCallback callback, void* userptr)
{
    sLog.callback = callback;
    sLog.callback_user = userptr;
}

void log_set_filepath(const char* filepath)
{
    if (sLog.file_output)
        fclose(sLog.file_output);

    sLog.file_output = fopen(filepath, "w");
}

bool log_get_enable_stdout() { return sLog.enable_stdout; }

void log_set_enable_stdout(bool bEnable)
{
    sLog.enable_stdout = bEnable;
    VNLogVerbose("Log stdout set to: %s\n", bEnable ? "true" : "false");
}

void log_set_enable_function_names(bool bEnable)
{
    sLog.enable_function_names = bEnable;
    VNLogVerbose("Log enable function names set to: %s\n", bEnable ? "true" : "false");
}

void log_close()
{
    if (sLog.file_output) {
        fclose(sLog.file_output);
        sLog.file_output = 0;
    }
}

void log_print(LogType type, const char* function, uint32_t line, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    log_printv(type, function, line, format, args);
    va_end(args);
}

void log_printv(LogType type, const char* function, uint32_t line, const char* format, va_list ap)
{
    if (type > sLog.verbosity) {
        return;
    }

    int32_t count = vsnprintf(sFormatBuffer[0], 16384, format, ap);
    const char* output = nullptr;

    if (sLog.enable_function_names) {
        // Don't add timing info to debug line if a callback is set
        if (sLog.enable_stdout && (sLog.callback == nullptr)) {
            count = snprintf(sFormatBuffer[1], 16384, "[%" PRId64 "]%s (%u): %s",
                             lcevc_dec::utility::GetTime<lcevc_dec::utility::NanoSecond>(), function, line,
                             sFormatBuffer[0]);
        } else {
            count = snprintf(sFormatBuffer[1], 16384, "%s (%u): %s", function, line, sFormatBuffer[0]);
        }

        output = sFormatBuffer[1];
    } else {
        output = sFormatBuffer[0];
    }

    // If callback is set then that will handle any output of the message, don't do any more
    if (sLog.callback) {
        sLog.callback(sLog.callback_user, type, output);
    } else {
#ifdef __ANDROID__
        if (sLog.enable_stdout) {
            fwrite(output, sizeof(char), count, (type == LogType_Error) ? stderr : stdout);
            // do the flush or else the output is not in sync between stdout AND stderr and threads
            // this makes debugging difficult as stderr will comeout infront of stdout
            // Needed on Android as well now :(
            fflush((type == LogType_Error) ? stderr : stdout);
        } else {
            __android_log_write(GetAndroidLogPriority(type), "VNOVA-ANDROID", output);
        }
#elif defined(__APPLE__)
        if (type != LogType_Disabled) {
            os_log_with_type(sLog.osLogObject, GetAppleLogPriority(type), "%s", sFormatBuffer[1]);
        }
#else
#ifdef _WIN32
        // Only use stdout as using both stdout and stderr as this makes
        // debugging difficult as stderr will comeout infront of stdout
        FILE* out = stdout;
#else
        FILE* out = (type == LogType_Error) ? stderr : stdout;
#endif
        if (sLog.enable_stdout) {
            fwrite(output, sizeof(char), count, out);
        }
#ifdef _WIN32
        OutputDebugString(output);
#endif
#endif

        if (sLog.file_output) {
            fwrite(output, sizeof(char), count, sLog.file_output);
            fflush(sLog.file_output);
        }
    }
}

/*------------------------------------------------------------------------------*/
