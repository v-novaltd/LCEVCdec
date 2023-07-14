// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
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
        return false;
    }

    return true;
}
