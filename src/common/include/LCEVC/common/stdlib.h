/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

#ifndef VN_LCEVC_COMMON_STDLIB_H
#define VN_LCEVC_COMMON_STDLIB_H

#include <LCEVC/build_config.h>

/*! @file
 * @brief Standard library
 *
 * A custom replacement for the standard library can be provided by defining VN_SDK_STDLIB_CUSTOM,
 * and providing `stdlib_custom.h` in the include path. The custom header should provide:
 * - All basic sized integer types (`int64_t`, `uint32_t`, etc)
 * - All basic memory functions (`malloc`, `realloc`, `free`, etc)
 * - Aligned alloc, matching the signature:
 *      void* aligned_alloc(size_t alignment, size_t size)
 * - A `printf` function
 * - Typedefs defining intmax_t and uintmax_t (for example, `typedef int64_t intmax_t`)
 * - Integer limits:
 *   - UINT8_MAX, UINT16_MAX, UINT32_MAX, and UINT64_MAX
 *   - The equivalent max and min constants for signed ints
 * - Format strings, if different from those in platform.h (for example, `#define PRIu64 "llu"`)
 * - A quick-sort function or macro, matching the signature
 *      void qsort(void* ptr, size_t count, size_t size, int (*comp)(const void*, const void*))
 * - An `assert` function or macro, which accepts a boolean parameter.
 */

#if VN_SDK_FEATURE(STDLIB_CUSTOM)
#include "stdlib_custom.h"
#else
#include "detail/stdlib_c.h"
#endif

#endif // VN_LCEVC_COMMON_STDLIB_H
