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

#ifndef VN_LCEVC_COMMON_CLOCK_H
#define VN_LCEVC_COMMON_CLOCK_H

#include <LCEVC/common/platform.h>
#include <stdint.h>

/*! @file
 * @brief Clock and time utilities.
 *
 * A custom implementation can be provided by defining VN_SDK_TIME_CUSTOM, and providing
 * `clock_custom.h` in the include path. The custom header should provide the static inline
 * implementations declared below.
 */

#ifdef __cplusplus
extern "C"
{
#endif

/*! Get monotonic time in nanoseconds.
 *
 * This is intended for timestamps and durations. It does not need to be related to wall-clock time.
 *
 * @return Monotonic time in nanoseconds.
 */
static inline uint64_t ldcClockMonotonicNanoseconds(void);

/*! Get wall-clock time in seconds.
 *
 * This is intended for seeding and other non-monotonic wall-clock uses.
 *
 * @return Wall-clock time in seconds, or 0 if unavailable.
 */
static inline uint64_t ldcClockWallSeconds(void);

/*! Get the current thread-deadline clock in microseconds with offset.
 *
 * The returned value is suitable for use with `threadCondVarWaitDeadline()`.
 *
 * @param[in] offset    Offset from current time.
 */
static inline uint64_t ldcClockDeadlineMicroseconds(int32_t offset);

// Implementation specific declarations
//
#if VN_SDK_FEATURE(TIME_CUSTOM)
#include "clock_custom.h"
#elif VN_OS(WINDOWS)
#include "detail/clock_win32.h"
#else
#include "detail/clock_unix.h"
#endif

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_COMMON_CLOCK_H
