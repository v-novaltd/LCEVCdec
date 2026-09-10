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

// Manual weak re-declaration of the API 29 (Android 10) AHardwareBuffer_lockPlanes
// symbol.
//
// The NDK's own <android/hardware_buffer.h> only marks this symbol weak when
// the translation unit is compiled with a target platform below API 29 (via
// its __INTRODUCED_IN availability annotation, tolerated as weak by the
// __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__ build define). If any build of this
// library - e.g. a CI/test harness configuration using a higher
// ANDROID_PLATFORM than this project's minimum - compiles at API 29+, the
// header instead emits an ordinary, hard-required symbol reference, which
// then fails to dynamic-link on real devices running an older Android
// version ("cannot locate symbol"). Redeclaring the symbol here with an
// explicit weak attribute keeps the reference weak in the compiled object
// code regardless of the compiling target level. Callers must still check the
// function for null before use.
#ifndef VN_LCEVC_PIPELINE_CPU_WEAK_API29_H
#define VN_LCEVC_PIPELINE_CPU_WEAK_API29_H

#include <android/hardware_buffer.h>

#define WEAK_API29 extern "C" __attribute__((weak))

WEAK_API29 int AHardwareBuffer_lockPlanes(AHardwareBuffer* buffer, uint64_t usage, int32_t fence,
                                          const ARect* rect, AHardwareBuffer_Planes* outPlanes);

#undef WEAK_API29

#endif // VN_LCEVC_PIPELINE_CPU_WEAK_API29_H
