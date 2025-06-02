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

#ifndef VN_API_LOG_H_
#define VN_API_LOG_H_

#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

#if defined(__APPLE__)
#include <os/log.h>
#endif

/*------------------------------------------------------------------------------*/

#ifdef _WIN32
#if _MSC_VER <= 1700

#ifndef PRId32
#define PRId32 "d"
#endif

#ifndef PRIu32
#define PRIu32 "u"
#endif

#ifndef PRId64
#define PRId64 "ld"
#endif

#ifndef PRIx64
#define PRIx64 "llx"
#endif

#ifndef PRIu64
#define PRIu64 "llu"
#endif

#else
#include <cinttypes>
#endif
#else
#define __STDC_FORMAT_MACROS
#include <cinttypes>
#ifndef PRId32
#define PRId32 "d"
#endif

#ifndef PRIu32
#define PRIu32 "u"
#endif

#ifndef PRId64
#define PRId64 "ld"
#endif

#ifndef PRIx64
#define PRIx64 "llx"
#endif

#ifndef PRIu64
#define PRIu64 "lu"
#endif
#endif

// - Enums ----------------------------------------------------------------------------------------

// Higher numbers mean more logs.
enum class LogLevel
{
    Disabled,
    Fatal,
    Error,
    Warning,
    Info,
    Debug,
    Trace,

    Count
};
static_assert(LogLevel::Disabled == LogLevel{},
              "Please keep Disable as the default-constructed LogLevel (or else "
              "Logger::m_verbosities and DecoderConfig::m_logLevels will not be "
              "default-initialised to Disabled)");

// Note: the numeric values of this array do not matter, so feel free to reorder this array as
// desired, e.g. to keep it alphabetised. Generally, the component should simply be the name of the
// file or class which is reporting the log.
enum class LogComponent
{
    API,
    BufferManager,
    CoreDecoder,
    Decoder,
    DecoderConfig,
    Interface,
    LCEVCProcessor,
    Log,
    Picture,
    Threading,

    Count
};

enum class LogPrecision
{
    Nano,
    Micro,
    NoTimestamps,

    Count
};

// - Types and classes ----------------------------------------------------------------------------

using LogCallback = void (*)(void* userptr, LogLevel level, const char* msg);

// Slightly cheeky class to save us from static_casting to size_t all the time
class LogArr : public std::array<LogLevel, static_cast<size_t>(LogComponent::Count)>
{
    using BaseClass = std::array<LogLevel, static_cast<size_t>(LogComponent::Count)>;

public:
    LogArr() = default;
    LogLevel& operator[](LogComponent comp)
    {
        return BaseClass::operator[](static_cast<size_t>(comp));
    }
    const LogLevel& operator[](LogComponent comp) const
    {
        return BaseClass::operator[](static_cast<size_t>(comp));
    }
};

class Logger
{
public:
    Logger() = default;
    ~Logger();
    Logger(const Logger& other) = delete;
    Logger(Logger&& other) = delete;
    Logger& operator=(const Logger& other) = delete;
    Logger& operator=(Logger&& other) = delete;

    void setVerbosity(LogComponent comp, LogLevel level);
    void setCallback(LogCallback callback, void* userptr);
    bool getEnableStdout() const;
    void setEnableStdout(bool enable);
    void setTimestampPrecision(LogPrecision precision);

    void print(LogComponent comp, LogLevel level, const char* function, uint32_t line,
               const char* format, ...);
    void printv(LogComponent comp, LogLevel level, const char* function, uint32_t line,
                const char* format, va_list ap);

private:
    static int64_t getTicks(LogPrecision precision);

    LogCallback m_callback = nullptr;
    void* m_callbackUser = nullptr;
    bool m_enableStdout = false;
    LogPrecision m_timingPrecision = LogPrecision::Nano;
    LogArr m_verbosities = {};
#if defined(__APPLE__)
    os_log_t m_osLogObject = os_log_create("com.vnova.logger", "VNOVA");
#endif
};

extern Logger sLog;

// ------------------------------------------------------------------------------------------------

// Use this to log from within a lambda (since __func__ doesn't work right in lambdas)
#define VNLogCustomFnName(level, msg, fnName, ...)               \
    do {                                                         \
        sLog.print(kComp, level, fnName, 0, msg, ##__VA_ARGS__); \
    } while (0)

// You're expected to set kComp for the file in which you're triggering logs.
#define VNLog(level, msg, ...)                                            \
    do {                                                                  \
        sLog.print(kComp, level, __func__, __LINE__, msg, ##__VA_ARGS__); \
    } while (0)

// We treat error and fatal as "mandatory" logs, i.e. not avoided by VNEnableLog

#define VNLogFatal(msg, ...) VNLog(LogLevel::Fatal, msg, ##__VA_ARGS__)

#define VNLogError(msg, ...) VNLog(LogLevel::Error, msg, ##__VA_ARGS__)

#define VNLogWarning(msg, ...)                        \
    do {                                              \
        VNLog(LogLevel::Warning, msg, ##__VA_ARGS__); \
    } while (0)

#define VNLogInfo(msg, ...)                        \
    do {                                           \
        VNLog(LogLevel::Info, msg, ##__VA_ARGS__); \
    } while (0)

#define VNLogDebug(msg, ...)                        \
    do {                                            \
        VNLog(LogLevel::Debug, msg, ##__VA_ARGS__); \
    } while (0)

#define VNLogTrace(msg, ...)                        \
    do {                                            \
        VNLog(LogLevel::Trace, msg, ##__VA_ARGS__); \
    } while (0)

// ------------------------------------------------------------------------------------------------

// Helper to print iterable objects. Use sparingly (printing should be cheap).
template <typename T>
static inline std::string iterableToString(const T& iterable)
{
    std::string out = "{";
    for (const auto& item : iterable) {
        out += std::to_string(item);
        out += ", ";
    }
    out += "}";
    return out;
}

// ------------------------------------------------------------------------------------------------

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

// ------------------------------------------------------------------------------------------------

#endif // VN_API_LOG_H_
