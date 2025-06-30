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

#ifndef VN_LCEVC_COMMON_PLATFORM_H
#define VN_LCEVC_COMMON_PLATFORM_H

#include <LCEVC/build_config.h>

#if VN_OS(WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

#elif VN_OS(BROWSER)
#include <emscripten/emscripten.h>
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif /* __STDC_LIMIT_MACROS */

#include <limits.h>

#define VN_UNUSED(x) (void)(x) // Deprecated
#define VNUnused(x) (void)(x)

// Thread local storage
//
#if VN_COMPILER(MSVC)
#define VNThreadLocal() __declspec(thread)
#else
#define VNThreadLocal() __thread
#endif

// Variable alignment, v can contain a normal variable declaration, a contains the
// alignment needed for this variable. E.g.
//
//		VnAlign(static const some_array[10], 16) = {0};
//
// This would align some_array to a 16 byte boundary and default it to 0.
//
#if VN_COMPILER(MSVC)
#define VnAlign(v, a) __declspec(align(a)) v
#else
#define VnAlign(v, a) v __attribute__((aligned(a)))
#endif

#if defined(__cplusplus)
#define VNAlignof(T) alignof(T)
#else
#define VNAlignof(T) _Alignof(T)
#endif

#if defined(__cplusplus)
#define VNAlignof(T) alignof(T)
#else
#define VNAlignof(T) _Alignof(T)
#endif

// Thread and process ID
//
#if VN_OS(WINDOWS)
#define VNGetThreadId() GetCurrentThreadId()
#define VNGetProcessId() GetCurrentProcessId()

#elif VN_OS(APPLE)
#include <pthread.h>
#include <unistd.h>
static inline uint64_t VNGetThreadId(void)
{
    uint64_t tid;
    pthread_threadid_np(NULL, &tid);
    return tid;
}
#define VNGetProcessId() getpid()
#elif VN_OS(BROWSER)
#include <emscripten/threading.h>
#define VNGetThreadId() (uint64_t) pthread_self()
#define VNGetProcessId() 0
#else
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#define VNGetThreadId() syscall(SYS_gettid)
#define VNGetProcessId() getpid()
#endif

// alloca
//
#if VN_OS(WINDOWS)
#include <malloc.h>
#else
#include <alloca.h>
#endif
#endif // VN_LCEVC_COMMON_PLATFORM_H
