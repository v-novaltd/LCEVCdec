/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_PLATFORM_H_
#define VN_DEC_CORE_PLATFORM_H_

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif /* __STDC_LIMIT_MACROS */

#include "lcevc_config.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if VN_OS(BROWSER)
#include <emscripten/emscripten.h>
#endif

/*------------------------------------------------------------------------------
 * Handle older MSVC library not containing these macros.
 *-----------------------------------------------------------------------------*/

#if VN_COMPILER(MSVC) && (_MSC_VER <= 1700)
#define PRId32 "d"
#define PRIu32 "u"
#define PRId64 "ld"
#define PRIx64 "llx"
#define PRIu64 "llu"
#else
#include <inttypes.h>
#endif

/*------------------------------------------------------------------------------
 * On MSVC < VS2015, snprintf doesn't exist
 *-----------------------------------------------------------------------------*/
#if VN_COMPILER(MSVC) && (_MSC_VER <= 1900)
#define snprintf(DEST, COUNT, FORMAT, ...) sprintf((DEST), (FORMAT), __VA_ARGS__)
#endif

/*------------------------------------------------------------------------------
 * Thread local storage.
 *-----------------------------------------------------------------------------*/
#if VN_OS(WINDOWS)
#define VN_THREADLOCAL() __declspec(thread)
#else
#define VN_THREADLOCAL() __thread
#endif

/*------------------------------------------------------------------------------
 * Variable alignment, v can contain a normal variable declaration, a contains the
 * alignment needed for this variable. E.g.
 *
 *		VNAlign(static const some_array[10], 16) = {0};
 *
 * This would align some_array to a 16 byte boundary and default it to 0.
 *-----------------------------------------------------------------------------*/
#if VN_OS(WINDOWS)
#define VN_ALIGN(v, a) __declspec(align(a)) v
#else
#define VN_ALIGN(v, a) v __attribute__((aligned(a)))
#endif

/*------------------------------------------------------------------------------
 * Preprocessor helpers.
 *-----------------------------------------------------------------------------*/
#define VN_CONCAT_HELPER(a, b) a##b
#define VN_CONCAT(a, b) VN_CONCAT_HELPER(a, b)

/*------------------------------------------------------------------------------
 * Common case for most functions is to return <0 upon failure, where the value
 * would indicate some error case, typically -1 is the general error case.
 * Otherwise we want to continue processing, this just saves the programmer from
 * having to put the check in lots of places where errors are captured. It is
 * assumed that the function using this macro has a variable declared as
 * "int32_t res;". And that the function returns an int32_t.
 *
 * @todo: Remove these macros.
 *-----------------------------------------------------------------------------*/
#define VN_CHECK(p) \
    res = (p);      \
    if (res < 0)    \
        return res;

#define VN_CHECKJ(p) \
    res = (p);       \
    if (res < 0)     \
        goto error_exit;

/*------------------------------------------------------------------------------
 * Used to flag a variable is unused such that the linter will no longer complain.
 *-----------------------------------------------------------------------------*/
#define VN_UNUSED(v) (void)(v)

#endif /* VN_DEC_CORE_PLATFORM_H_ */
