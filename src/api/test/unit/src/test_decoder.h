/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef VN_API_TEST_UNIT_TEST_DECODER_H_
#define VN_API_TEST_UNIT_TEST_DECODER_H_

// Define this so that we can reach both the internal headers (which are NOT in a dll) AND some
// handy functions from the actual interface of the API (which normally WOULD be in a dll).
#define VNDisablePublicAPI

#include <LCEVC/lcevc_dec.h>

#include <cstddef>
#include <vector>

static const int32_t kResultsQueueCap = 24;
static const int32_t kUnprocessedCap = 100;

#endif // VN_API_TEST_UNIT_TEST_DECODER_H_
