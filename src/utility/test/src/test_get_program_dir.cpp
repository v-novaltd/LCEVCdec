// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
#include "LCEVC/utility/get_program_dir.h"
//
#include "filesystem.h"
//
#include <gtest/gtest.h>

#if defined(_WIN32)
const char kProgramName[] = {"lcevc_dec_utility_test.exe"};
#else
const char kProgramName[] = {"lcevc_dec_utility_test"};
#endif

using namespace lcevc_dec::utility;

TEST(GetProgramDir, Test)
{
    filesystem::path pd = getProgramDirectory();

    EXPECT_TRUE(filesystem::exists(pd / kProgramName));

    filesystem::path pp = getProgramDirectory(kProgramName);

    EXPECT_TRUE(filesystem::exists(pp));

    EXPECT_TRUE(pp.is_absolute());
}
