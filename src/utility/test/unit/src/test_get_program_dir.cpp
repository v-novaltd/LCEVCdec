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

#include "LCEVC/utility/filesystem.h"
#include "LCEVC/utility/get_program_dir.h"
//
#include <gtest/gtest.h>

#if defined(_WIN32)
const char kProgramName[] = {"lcevc_dec_utility_test_unit.exe"};
#else
const char kProgramName[] = {"lcevc_dec_utility_test_unit"};
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
