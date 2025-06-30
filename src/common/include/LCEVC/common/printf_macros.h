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

// Make sure the PRxx macros are defined
//
#ifndef VN_LCEVC_COMMON_PRINTF_MACROS_H
#define VN_LCEVC_COMMON_PRINTF_MACROS_H

#ifdef _WIN32
#if !defined(__MINGW32__) && _MSC_VER <= 1700

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
#include <inttypes.h>
#endif
#else
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
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

#endif // VN_LCEVC_COMMON_PRINTF_MACROS_H
