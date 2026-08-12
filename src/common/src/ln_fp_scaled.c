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

#include <LCEVC/common/bitutils.h>
#include <LCEVC/common/ln_fp_scaled.h>

/* Calculate the natural log (ln) of in, scaled by a constant, using only integer operations.
 * Accurate to 12 bits of fractional precision and 4 bits of integer precision.
 */
int32_t lnFpScaled(int32_t in, int32_t scale, bool roundToZero)
{
    /* The reference is equivalent to:
     *
     *     float(ln(in) * 4096) * scale / 4096
     *
     * with the final division truncated toward zero.
     *
     * Compute ln(x) in fixed point using the atanh identity:
     *
     *     ln(x) = k*ln(2) + 2*(y + y^3/3 + y^5/5 + ...)
     *
     * where:
     *     k = round(log2(x)) in the logarithmic sense.
     *     p = 2^k
     *     y = (x - p) / (x + p).
     *
     * Start with n = floor(log2(x)) and round up when x >= 2^n*sqrt(2).
     *
     * This keeps |y| <= (sqrt(2)-1)/(sqrt(2)+1) ~= 0.1716, so the
     * first five terms, through y^9/9, are enough to determine the 12
     * fractional bits over the [1...32767] domain..
     */
    enum
    {
        Q = 33,
        REF_FRAC_BITS = 12,
        TERMS = 5
    };
    static const int64_t LN2_Q33 = 5954088943LL; /* floor(ln(2) * 2^33) */
    static const int64_t ONE_Q33 = 1LL << Q;

    assert(in > 0);

    const uint32_t x = (uint32_t)in;

    int n = 0;
    for (uint32_t t = x; t >>= 1;) {
        ++n;
    }

    /* k = n */
    int k = n;
    int64_t pow2 = 1LL << n;
    if ((int64_t)x * (int64_t)x >= 2 * pow2 * pow2) {
        ++k;
        pow2 <<= 1;
    }

    const int64_t y = (((int64_t)x - pow2) * ONE_Q33) / ((int64_t)x + pow2);
    const int64_t y2 = (y * y) / ONE_Q33;

    int64_t term = y;
    int64_t sum = term;
    for (int i = 1; i < TERMS; ++i) {
        term = (term * y2) / ONE_Q33;
        sum += term / (2 * i + 1);
    }

    const int64_t ln_q33 = (int64_t)k * LN2_Q33 + 2 * sum;
    const int64_t ln_q12 = ln_q33 >> (Q - REF_FRAC_BITS);

    // Calculate the result, rounded DOWN.
    int32_t res = (int32_t)((ln_q12 * (int64_t)scale) >> REF_FRAC_BITS);

    // If we want to round TOWARDS ZERO, then we just hack that in right here.
    if (roundToZero) {
        const bool dividesEvenly = (res * (1 << REF_FRAC_BITS)) == (ln_q12 * (int64_t)scale);
        if (res < 0 && !dividesEvenly) {
            res++;
        }
    }
    return res;
}
