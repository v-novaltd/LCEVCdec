/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef LCEVC_H
#define LCEVC_H

#include <stdint.h>

#ifdef __cplusplus
#define LCEVC_EXTERN_C extern "C"
#else
#define LCEVC_EXTERN_C
#endif

#ifdef _WIN32
#ifdef VNDisablePublicAPI
#define VN_LCEVC_PublicAPI LCEVC_EXTERN_C
#else
#ifdef VNEnablePublicAPIExport
#define VN_LCEVC_PublicAPI LCEVC_EXTERN_C __declspec(dllexport)
#else
#define VN_LCEVC_PublicAPI LCEVC_EXTERN_C __declspec(dllimport)
#endif
#endif
#else
#define VN_LCEVC_PublicAPI LCEVC_EXTERN_C __attribute__((visibility("default")))
#endif

#define LCEVC_API VN_LCEVC_PublicAPI

/*! Helper macros to assist with concatenating inside the pre-processor.
 */
#define VN_LCEVC_Concat_Do(a, version) a##version
#define VN_LCEVC_Concat(a, version) VN_LCEVC_Concat_Do(a, version)

#endif // LCEVC_H
