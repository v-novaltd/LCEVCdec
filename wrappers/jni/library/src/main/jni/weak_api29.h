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

// Manual weak re-declarations of API 29 (Android 10) SurfaceControl/Transaction
// and AHardwareBuffer_lockPlanes symbols.
//
// The NDK's own headers only mark these symbols weak when the translation
// unit is compiled with a target platform below API 29 (via their
// __INTRODUCED_IN availability annotation, tolerated as weak by the
// __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__ build define). If any build of
// this library - e.g. a CI/test harness configuration using a higher
// ANDROID_PLATFORM than this project's minimum - compiles at API 29+, the
// headers instead emit ordinary, hard-required symbol references, which then
// fail to dynamic-link on real devices running an older Android version
// ("cannot locate symbol"). Redeclaring the symbols here with an explicit
// weak attribute keeps the references weak in the compiled object code
// regardless of the compiling target level. Callers must still check each
// function for null before use.
#ifndef LCEVC_DEC_JNI_WEAK_API29_H
#define LCEVC_DEC_JNI_WEAK_API29_H

#include <android/hardware_buffer.h>
#include <android/surface_control.h>

#define WEAK_API29 extern "C" __attribute__((weak))

WEAK_API29 ASurfaceControl* ASurfaceControl_createFromWindow(ANativeWindow* parent, const char* debug_name);
WEAK_API29 void ASurfaceControl_release(ASurfaceControl* surface_control);

WEAK_API29 ASurfaceTransaction* ASurfaceTransaction_create();
WEAK_API29 void ASurfaceTransaction_delete(ASurfaceTransaction* transaction);
WEAK_API29 void ASurfaceTransaction_apply(ASurfaceTransaction* transaction);
WEAK_API29 void ASurfaceTransaction_setBuffer(ASurfaceTransaction* transaction,
                                              ASurfaceControl* surface_control,
                                              AHardwareBuffer* buffer, int acquire_fence_fd);
WEAK_API29 void ASurfaceTransaction_setOnComplete(ASurfaceTransaction* transaction, void* context,
                                                  ASurfaceTransaction_OnComplete func);
WEAK_API29 void ASurfaceTransaction_setGeometry(ASurfaceTransaction* transaction,
                                                ASurfaceControl* surface_control, const ARect& source,
                                                const ARect& destination, int32_t transform);
WEAK_API29 void ASurfaceTransaction_reparent(ASurfaceTransaction* transaction,
                                             ASurfaceControl* surface_control,
                                             ASurfaceControl* new_parent);

WEAK_API29 int AHardwareBuffer_lockPlanes(AHardwareBuffer* buffer, uint64_t usage, int32_t fence,
                                          const ARect* rect, AHardwareBuffer_Planes* outPlanes);

#undef WEAK_API29

#endif // LCEVC_DEC_JNI_WEAK_API29_H
