/* Copyright (c) V-Nova International Limited 2022. All rights reserved.
 */
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
