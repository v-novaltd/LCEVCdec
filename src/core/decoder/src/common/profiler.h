/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_PROFILER_H_
#define VN_DEC_CORE_PROFILER_H_

#include "common/types.h"

/*------------------------------------------------------------------------------*/

typedef int32_t ProfileID_t;
typedef struct ProfilerState ProfilerState_t;

int32_t profilerInitialise(Context_t* ctx);
void profilerRelease(Context_t* ctx);

void profilerRegisterThread(ProfilerState_t* profiler, const char* label);
ProfileID_t profilerRegisterProfile(ProfilerState_t* profiler, const char* label, const char* file,
                                    uint32_t line);
ProfileID_t profilerRetrieveProfile(ProfilerState_t* profiler, const char* file, uint32_t line,
                                    const char* labelFmt, ...);

void profilerTickStart(ProfilerState_t* profiler);
void profilerTickStop(ProfilerState_t* profiler);
void profilerProfileStart(ProfilerState_t* profiler, ProfileID_t id);
void profilerProfileStop(ProfilerState_t* profiler);
void profilerFlush(ProfilerState_t* profiler);

/*------------------------------------------------------------------------------
 * Macros to control compile time enable/disable of the profiler.
 *------------------------------------------------------------------------------*/

#define VN_PROFILER_VAR_PROFILE_ID() VN_CONCAT(kPerThreadProfilerProfileID, __LINE__)
#define VN_PROFILER_VAR_PROFILE_INITIALISED() VN_CONCAT(kPerThreadProfileInitialised, __LINE__)

#if VN_CORE_FEATURE(PROFILER)

#define VN_PROFILE_LABEL(label)                                                                           \
    static VN_THREADLOCAL() ProfileID_t VN_PROFILER_VAR_PROFILE_ID();                                     \
    static VN_THREADLOCAL() bool VN_PROFILER_VAR_PROFILE_INITIALISED() = false;                           \
    if (!VN_PROFILER_VAR_PROFILE_INITIALISED()) {                                                         \
        VN_PROFILER_VAR_PROFILE_ID() = profilerRegisterProfile(ctx->profiler, label, __FILE__, __LINE__); \
        VN_PROFILER_VAR_PROFILE_INITIALISED() = true;                                                     \
    }                                                                                                     \
    profilerProfileStart(ctx->profiler, VN_PROFILER_VAR_PROFILE_ID());

#define VN_PROFILE_LABEL_DYNAMIC(labelFmt, ...)                                                    \
    profilerProfileStart(ctx->profiler, profilerRetrieveProfile(ctx->profiler, __FILE__, __LINE__, \
                                                                labelFmt, ##__VA_ARGS__));

/* labeled profile entry point, use this for scoped calls with an identifying label*/
#define VN_PROFILE_START(label) VN_PROFILE_LABEL(label)

/* labeled profile entry point, use this for scoped calls with an identifying label that can be
 * formatted dynamically at runtime based upon arguments - this approach contains more system
 * overhead up front, and the profile will not be unique across threads. */
#define VN_PROFILE_START_DYNAMIC(labelFmt, ...) VN_PROFILE_LABEL_DYNAMIC(labelFmt, ##__VA_ARGS__)

/* Function profile entry point, use this for function calls, place at the top of the function */
#define VN_PROFILE_FUNCTION() VN_PROFILE_LABEL(__FUNCTION__)

/* This must be called when you want the current profile to stop, every call to start must be qualified with a stop */
#define VN_PROFILE_STOP() profilerProfileStop(ctx->profiler)

#else

#define VN_PROFILE_START(label)
#define VN_PROFILE_START_DYNAMIC(labelFmt, ...)
#define VN_PROFILE_FUNCTION()
#define VN_PROFILE_STOP()

#endif

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_PROFILER_H_ */
