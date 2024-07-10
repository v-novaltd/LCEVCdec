/* Copyright (c) V-Nova International Limited 2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef VN_API_THREADING_H_
#define VN_API_THREADING_H_

#include <string_view>

#if defined(_WIN32)
#include <Windows.h>
#define VNThreadLocal __declspec(thread)
#define VN_TO_THREAD_NAME(x) L##x
#else
#define VNThreadLocal __thread
#define VN_TO_THREAD_NAME(x) x
#endif

namespace lcevc_dec::decoder {
#if defined(WIN32)
void setThreadName(std::wstring_view name);
#else
void setThreadName(std::string_view name);
#endif
} // namespace lcevc_dec::decoder

#endif // VN_API_THREADING_H_
