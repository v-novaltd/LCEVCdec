// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
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
#define VN_LCEVC_CHECK_RET(expr)         \
    do {                              \
        LCEVC_ReturnCode rc = (expr); \
        if (rc != LCEVC_Success)      \
            return rc;                \
    } while (0)


/* Functions to do the real work of LCEVC_CHECK() and LCEVC_AGAIN()
 */
void LCEVC_CheckFn(const char* file, int line, const char* expr, LCEVC_ReturnCode r);
bool LCEVC_AgainFn(const char* file, int line, const char* expr, LCEVC_ReturnCode r);

#ifdef __cplusplus
}
#endif

#endif
