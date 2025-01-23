/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
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

// Helper macros for checking return codes from LCEVC_DEC API functions.
//
#ifndef VN_LCEVC_UTILITY_CHECK_H
#define VN_LCEVC_UTILITY_CHECK_H

#include "LCEVC/lcevc_dec.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 * \brief Check if an expression returns an LCEVC error
 *
 * If there is an error prints summary to `stderr` and exits.
 *
 * @param[in]       expr    Some expression that returns LCEVC_ReturnCode
 */
#define VN_LCEVC_CHECK(expr) LCEVC_CheckFn(__FILE__, __LINE__, #expr, expr)

/*!
 * \brief Check if an expression returns an LCEVC error other then LCEVC_AGain
 *
 * If there is an error prints summary to `stderr` and exits.
 *
 * @param[in]       expr    Some expression that returns LCEVC_ReturnCode
 * @return                  true if result was LCEVC_Success
 *                          false if result was LCEVC_Again or an error
 */
#define VN_LCEVC_AGAIN(expr) LCEVC_AgainFn(__FILE__, __LINE__, #expr, expr)

/*!
 * \brief Check if an expression returns an LCEVC error and return it immediately
 *
 * @param[in]       expr    Some expression that returns LCEVC_ReturnCode
 */
#define VN_LCEVC_CHECK_RET(expr)      \
    do {                              \
        LCEVC_ReturnCode rc = (expr); \
        if (rc != LCEVC_Success)      \
            return rc;                \
    } while (0)

/*!
 * \brief Check if a utility function returns a true or false
 *
 * If there is an error prints summary to `stderr` and exits.
 *
 * @param[in]       expr    Some expression that returns LCEVC_ReturnCode
 */
#define VN_UTILITY_CHECK(expr) utilityCheckFn(__FILE__, __LINE__, #expr, expr)
#define VN_UTILITY_CHECK_MSG(expr, msg) utilityCheckFn(__FILE__, __LINE__, #expr, expr, msg)

/* Functions to do the real work of LCEVC_CHECK(), LCEVC_AGAIN() and UTILITY_CHECK()
 */
void LCEVC_CheckFn(const char* file, int line, const char* expr, LCEVC_ReturnCode r);
bool LCEVC_AgainFn(const char* file, int line, const char* expr, LCEVC_ReturnCode r);
void utilityCheckFn(const char* file, int line, const char* expr, bool r, const char* msg = "");

#ifdef __cplusplus
}
#endif

#endif
