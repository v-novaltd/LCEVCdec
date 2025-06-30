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

#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/log.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

bool diagnosticsTestCLog(void)
{
    VNLogInfo("Test %c", 'a');
    VNLogInfo("Test %d", 42);
    VNLogInfo("Test %d", 0);
    VNLogInfo("Test %d", INT_MAX);
    VNLogInfo("Test %d", INT_MIN);
    VNLogInfo("Test %u", 9769876);
    VNLogInfo("Test %u", 0);
    //    VNLogInfo("Test %u", UINT_MAX);

    VNLogInfo("Test %s", "string");

    return true;
}

static void function2(void)
{
    VNTraceScopedBegin();

    VNLogInfo("function2()");

    VNTraceScopedEnd();
}

static void function1(void)
{
    VNTraceScopedBegin();

    VNLogInfo("function1()");

    function2();

    VNTraceScopedEnd();
}

bool diagnosticsTestCScope(void)
{
    function1();
    return true;
}

bool diagnosticsTestCMetrics(void)
{
    int32_t i32 = 44;
    uint32_t u32 = 45;
    int64_t i64 = 46;
    uint64_t u64 = 47;
    float f32 = 3.1f;
    float f64 = 8.0;

    VNMetricInt32("i32", i32);
    VNMetricUInt32("u32", u32);
    VNMetricInt64("i64", i64);
    VNMetricUInt64("u64", u64);
    VNMetricFloat32("f32", f32);
    VNMetricFloat64("f64", f64);

    return true;
}
