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

#ifndef VN_LCEVC_UTILITY_PERFETTO_SDK_H
#define VN_LCEVC_UTILITY_PERFETTO_SDK_H

#include <LCEVC/common/diagnostics.h>
#include <perfetto.h>

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("lcevc.dec").SetDescription("LCEVC decoder tracing and metrics"),
    perfetto::Category("lcevc.dec.jni").SetDescription("LCEVC Android JNI call durations"),
    perfetto::Category("lcevc.dec.vulkan").SetDescription("LCEVC Vulkan pipeline tracing"));

// Initialize the process-wide Perfetto SDK and category registry.
void lcevcPerfettoSdkInitialize();

// Register/unregister the process-wide LCEVC diagnostics bridge while the shared diagnostics state
// is initialized.
bool lcevcPerfettoSdkRegister();
void lcevcPerfettoSdkUnregister();

bool lcevcPerfettoDiagnosticHandler(void* user, const LdcDiagSite* site, const LdcDiagRecord* record,
                                    const LdcDiagValue* values, uint32_t valuesCount);
void lcevcPerfettoResetDiagnosticHandler();

#endif // VN_LCEVC_UTILITY_PERFETTO_SDK_H
