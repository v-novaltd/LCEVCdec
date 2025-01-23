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

#include "LCEVC/utility/check.h"

#include "LCEVC/utility/types.h"

#include <fmt/core.h>

void LCEVC_CheckFn(const char* file, int line, const char* expr, LCEVC_ReturnCode r)
{
    if (r != LCEVC_Success) {
        fmt::print(stderr, "{}:{} '{}' failed: {}\n", file, line, expr, r);
        std::exit(EXIT_FAILURE);
    }
}

bool LCEVC_AgainFn(const char* file, int line, const char* expr, LCEVC_ReturnCode r)
{
    if (r == LCEVC_Again) {
        return false;
    }

    if (r != LCEVC_Success) {
        fmt::print(stderr, "{}:{} '{}' failed: {}\n", file, line, expr, r);
        std::exit(EXIT_FAILURE);
    }

    return true;
}

void utilityCheckFn(const char* file, int line, const char* expr, bool r, const char* msg)
{
    if (!r) {
        if (strcmp(msg, "") == 0) {
            fmt::print(stderr, "{}:{} '{}' returned {}\n", file, line, expr, r);
        } else {
            fmt::print(stderr, "{}:{} '{}' failed: {}\n", file, line, expr, msg);
        }
        std::exit(EXIT_FAILURE);
    }
}
