/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

#include "log.h"

#include <LCEVC/utility/chrono.h>
#include <LCEVC/utility/enum_map.h>
#include <LCEVC/utility/threading.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#endif

#include <cstddef>

// ------------------------------------------------------------------------------------------------

using namespace lcevc_dec::utility;

// -Static variables-------------------------------------------------------------------------------

static const LogComponent kComp = LogComponent::Log;

static VNThreadLocal char sFormatBuffer[2][16384];

constexpr EnumMapArr<LogComponent, static_cast<size_t>(LogComponent::Count)> kLogComponentMap{
    LogComponent::API,
    "API",
    LogComponent::BufferManager,
    "BufferManager",
    LogComponent::CoreDecoder,
    "CoreDecoder",
    LogComponent::Decoder,
    "Decoder",
    LogComponent::DecoderConfig,
    "DecoderConfig",
    LogComponent::Interface,
    "Interface",
    LogComponent::LCEVCProcessor,
    "LCEVCProcessor",
    LogComponent::Log,
    "Log",
    LogComponent::Picture,
    "Picture",
    LogComponent::Threading,
    "Threading",
};
static_assert(!kLogComponentMap.isMissingEnums(),
              "kLogComponentMap is missing an entry for some enum.");

// -Platform functions-----------------------------------------------------------------------------

#ifdef __ANDROID__
static android_LogPriority GetAndroidLogPriority(const LogLevel level)
{
    switch (level) {
        case LogLevel::Disabled: return ANDROID_LOG_SILENT;
        case LogLevel::Fatal: return ANDROID_LOG_FATAL;
        case LogLevel::Error: return ANDROID_LOG_ERROR;
        case LogLevel::Warning: return ANDROID_LOG_WARN;
        case LogLevel::Info: return ANDROID_LOG_INFO;
        case LogLevel::Debug: return ANDROID_LOG_DEBUG;
        case LogLevel::Trace: return ANDROID_LOG_VERBOSE;
        case LogLevel::Count: break;
    }

    return ANDROID_LOG_UNKNOWN;
}
#elif defined(__APPLE__)
static os_log_type_t GetAppleLogPriority(const LogLevel level)
{
    switch (level) {
        case LogLevel::Disabled:
        case LogLevel::Fatal:
        case LogLevel::Error: return OS_LOG_TYPE_FAULT;
        case LogLevel::Warning: return OS_LOG_TYPE_ERROR;
        case LogLevel::Info: return OS_LOG_TYPE_DEFAULT;
        case LogLevel::Debug: return OS_LOG_TYPE_INFO;
        case LogLevel::Trace: return OS_LOG_TYPE_DEBUG;
        case LogLevel::Count: break;
    }
    return OS_LOG_TYPE_DEFAULT;
}
#else // Linux, Windows
static bool isSevere(LogLevel level)
{
    return level == LogLevel::Error || level == LogLevel::Fatal;
}
#endif

// -Logger-----------------------------------------------------------------------------------------

Logger sLog = Logger();

Logger::~Logger()
{
    if (m_enableStdout) {
        fflush(stdout);
    } else {
#ifdef __ANDROID__
        // No platform-specific flush/release function
#elif defined(__APPLE__)
        os_release(m_osLogObject);
#else // Linux, Windows
        fflush(stderr);
#endif
    }
}

void Logger::setVerbosity(LogComponent comp, LogLevel level) { m_verbosities[comp] = level; }

void Logger::setCallback(LogCallback callback, void* userptr)
{
    m_callback = callback;
    m_callbackUser = userptr;
}

bool Logger::getEnableStdout() const { return m_enableStdout; }

void Logger::setEnableStdout(bool enable)
{
    m_enableStdout = enable;
    VNLogTrace("enableStdout set to: %s\n", enable ? "true" : "false");
}

void Logger::setTimestampPrecision(LogPrecision precision) { m_timingPrecision = precision; }

void Logger::print(LogComponent comp, LogLevel level, const char* function, uint32_t line,
                   const char* format, ...)
{
    va_list args;
    va_start(args, format);
    Logger::printv(comp, level, function, line, format, args);
    va_end(args);
}

void Logger::printv(LogComponent comp, LogLevel level, const char* function, uint32_t line,
                    const char* format, va_list ap)
{
    if (level > m_verbosities[comp]) {
        return;
    }

    vsnprintf(sFormatBuffer[0], 16384, format, ap);
    const char* output = nullptr;

    const char* compName = enumToString(kLogComponentMap, comp).data();
    if (compName == nullptr) {
        compName = "unknown";
    }
    int32_t count = 0;
    if (m_timingPrecision != LogPrecision::NoTimestamps) {
        count = snprintf(sFormatBuffer[1], 16384, "[%" PRId64 "]%s, %s (%u) - %s",
                         getTicks(m_timingPrecision), compName, function, line, sFormatBuffer[0]);
    } else {
        count = snprintf(sFormatBuffer[1], 16384, "%s, %s (%u) - %s", compName, function, line,
                         sFormatBuffer[0]);
    }
    output = sFormatBuffer[1];

    // For Windows, ALWAYS send it to the debugger (for example, the Visual Studio console, or the
    // system debugger if enabled).
#ifdef _WIN32
    OutputDebugString(output);
#endif

    // If callback is set then that will handle any output of the message, don't do any more
    if (m_callback) {
        m_callback(m_callbackUser, level, output);
        return;
    }

    // If stdout is enabled, send log straight there
    if (m_enableStdout) {
        fwrite(output, sizeof(char), count, stdout);
        return;
    }

    // No callback or stdout, so send to platform-specific log destination. Note that we do use
    // stderr, but we DON'T mix it with stdout (logs from a given run should not be split between
    // two locations).
#ifdef __ANDROID__
    __android_log_write(GetAndroidLogPriority(level), "VNOVA-ANDROID", output);
#elif defined(__APPLE__)
    os_log_with_type(m_osLogObject, GetAppleLogPriority(level), "%s", sFormatBuffer[1]);
#else // Linux, Windows
    if (isSevere(level)) {
        fwrite(output, sizeof(char), count, stderr);
    }
#endif
}

int64_t Logger::getTicks(LogPrecision precision)
{
    switch (precision) {
        case LogPrecision::Micro: return getTime<MicroSecond>();
        case LogPrecision::Nano: return getTime<NanoSecond>();
        case LogPrecision::NoTimestamps:
        case LogPrecision::Count: break;
    }
    return -1;
}

// ------------------------------------------------------------------------------------------------
