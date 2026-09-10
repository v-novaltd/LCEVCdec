/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

#include <gtest/gtest.h>
#include <LCEVC/common/ln_fp_scaled.h>

#include <cmath>

// Calculate the natural log (ln) of stepWidth, scaled by a constant
static double referenceLn(int32_t stepWidth)
{
    const uint8_t integerLogSw = (uint8_t)(floor(log(stepWidth)));
    /* The maximum value of stepWidth is 32768, and ln(32768) = 10.3972. Therefore, integerLogSw
     * should really never exceed 10, but all that really matters is that it remains within 4
     * bits, i.e. less than 16. */
    assert(integerLogSw < 16);
    const double fractionalLogSw = floor((log(stepWidth) - integerLogSw) * 4096) / 4096;
    return integerLogSw + fractionalLogSw;
}

TEST(lnFpScaled, Test)
{
    // Arbitrary example scaling constants based on values in dequant.c
    static const int32_t kCValues[] = {5242, -5242, 39};
    static const int32_t kD = 99614;

    for (const int32_t scale : kCValues) {
        for (int32_t x = 1; x < 32767; ++x) {
            const int32_t got = lnFpScaled(x, scale, true);
            const int32_t ref = (int32_t)(referenceLn(x) * scale);
            EXPECT_EQ(got, ref) << "mismatched with input " << x << " and scale factor " << scale;

            // Additional unit tests for the specific use-case in dequant.c. In order to match the
            // reference code, you can't round towards zero; you have to strictly round down. You
            // also can't subtract the multiplication; you have to add the negated-multiplication.
            const int64_t gotLogByLayerSW = (int64_t)(kD + lnFpScaled(x, -scale, false));
            const int64_t refLogByLayerSW = (int64_t)(kD - (double)scale * referenceLn(x));
            EXPECT_EQ(gotLogByLayerSW, refLogByLayerSW)
                << "mismatched logByLayerSw with input " << x << " and scale factor " << scale;
        }
    }
}
