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

#ifndef VN_LCEVC_COMMON_CHECK_H
#define VN_LCEVC_COMMON_CHECK_H

#include <LCEVC/common/log.h>
#include <LCEVC/common/platform.h>

// Check and return macros
//
#define VNCheck(op)                             \
    do {                                        \
        if (!(op)) {                            \
            VNLogError("Call failed: %s", #op); \
        }                                       \
    } while (0)

#define VNCheckB(op)                            \
    do {                                        \
        if (!(op)) {                            \
            VNLogError("Call failed: %s", #op); \
            return false;                       \
        }                                       \
    } while (0)

#define VNCheckP(op)                            \
    do {                                        \
        if (!(op)) {                            \
            VNLogError("Call failed: %s", #op); \
            return nullptr;                     \
        }                                       \
    } while (0)

#define VNCheckI(op)                            \
    do {                                        \
        if ((op) != 0) {                        \
            VNLogError("Call failed: %s", #op); \
            return -1;                          \
        }                                       \
    } while (0)

#endif // VN_LCEVC_COMMON_CHECK_H
