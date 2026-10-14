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

// Common inline functions and macros for NEON upscalers.
//
// This header is deliberately configuration-independent. IN_FIXED_POINT,
// OUT_FIXED_POINT, APPLY_PA and DITHER are resolved by each loop header, so the
// common helpers can be included once while the loops are instantiated multiple
// times with different compile-time configurations.
//
// Clamping is handled by shifting 1 less, (13), doing a saturating pack, then shifting last step.
//
#ifndef UPSCALE_PLAYGROUND_COMMON_NEON_H
#define UPSCALE_PLAYGROUND_COMMON_NEON_H

#if !defined(__aarch64__)
#define vmlal_high_s16(acc, lhs, rhs) vmlal_s16((acc), vget_high_s16(lhs), vget_high_s16(rhs))
#define vqshrn_high_n_s32(low, high, shift) vcombine_s16((low), vqshrn_n_s32((high), (shift)))
#define vzip1q_s16(lhs, rhs) vzipq_s16((lhs), (rhs)).val[0]
#define vzip2q_s16(lhs, rhs) vzipq_s16((lhs), (rhs)).val[1]
#define vzip1q_s32(lhs, rhs) vzipq_s32((lhs), (rhs)).val[0]
#define vzip2q_s32(lhs, rhs) vzipq_s32((lhs), (rhs)).val[1]

static inline uint8x16_t neonVtbl1qU8(uint8x16_t src, uint8x16_t control)
{
    const uint8x8x2_t table = {{vget_low_u8(src), vget_high_u8(src)}};
    return vcombine_u8(vtbl2_u8(table, vget_low_u8(control)), vtbl2_u8(table, vget_high_u8(control)));
}

static inline uint8x16_t neonVtbl2qU8(uint8x16x2_t src, uint8x16_t control)
{
    const uint8x8x4_t table = {{vget_low_u8(src.val[0]), vget_high_u8(src.val[0]),
                                vget_low_u8(src.val[1]), vget_high_u8(src.val[1])}};
    return vcombine_u8(vtbl4_u8(table, vget_low_u8(control)), vtbl4_u8(table, vget_high_u8(control)));
}

#define vqtbl1q_u8(src, control) neonVtbl1qU8((src), (control))
#define vqtbl2q_u8(src, control) neonVtbl2qU8((src), (control))
#endif

#if defined(__GNUC__) || defined(__clang__)
#define NEON_LLVM_MCA_MARKER(text) __asm__ volatile(text ::: "memory")
#else
#define NEON_LLVM_MCA_MARKER(text) (void)0
#endif

static inline uint8_t* neonRowPtr(uint8_t* ptr, uint32_t stride, uint32_t height, int32_t y)
{
    if (y < 0) {
        y = 0;
    }
    if (y >= (int32_t)height) {
        y = (int32_t)height - 1;
    }
    return ptr + ((size_t)y * stride);
}

static inline int16x8_t neonPromoteU8x8(const uint8_t* ptr)
{
    return vreinterpretq_s16_u16(vsubq_u16(vshll_n_u8(vld1_u8(ptr), 7), vdupq_n_u16(0x4000)));
}

static inline int16x4_t neonPromoteU8x4(const uint8_t* ptr)
{
    uint8x8_t values = vdup_n_u8(0);
    values = vld1_lane_u8(ptr + 0, values, 0);
    values = vld1_lane_u8(ptr + 1, values, 1);
    values = vld1_lane_u8(ptr + 2, values, 2);
    values = vld1_lane_u8(ptr + 3, values, 3);
    return vreinterpret_s16_u16(vsub_u16(vget_low_u16(vshll_n_u8(values, 7)), vdup_n_u16(0x4000)));
}

static inline int16x8_t neonPromoteU16x8(const uint8_t* ptr, uint16_t shift)
{
    const uint16x8_t values = vld1q_u16((const uint16_t*)ptr);
    return vreinterpretq_s16_u16(
        vsubq_u16(vshlq_u16(values, vdupq_n_s16((int16_t)shift)), vdupq_n_u16(0x4000)));
}

static inline int16x4_t neonPromoteU16x4(const uint8_t* ptr, uint16_t shift)
{
    const uint16x4_t values = vld1_u16((const uint16_t*)ptr);
    return vreinterpret_s16_u16(vsub_u16(vshl_u16(values, vdup_n_s16((int16_t)shift)), vdup_n_u16(0x4000)));
}

static inline uint8x8_t neonDemoteU8x8(int16x8_t values)
{
    const int16x8_t rounded = vshrq_n_s16(vqaddq_s16(values, vdupq_n_s16(64)), 7);
    return vqmovun_s16(vaddq_s16(rounded, vdupq_n_s16(128)));
}

static inline uint16x8_t neonDemoteU16x8(int16x8_t values, uint16_t offset, uint16_t shift, uint16_t midpoint)
{
    const int16x8_t shifted =
        vshlq_s16(vqaddq_s16(values, vdupq_n_s16((int16_t)offset)), vdupq_n_s16(-(int16_t)shift));
    const int16x8_t converted = vaddq_s16(shifted, vdupq_n_s16((int16_t)midpoint));
    return vreinterpretq_u16_s16(
        vminq_s16(vmaxq_s16(converted, vdupq_n_s16(0)), vdupq_n_s16((int16_t)(2 * midpoint - 1))));
}

// Vertical filter pair: 5 input rows -> both filter phases (even = dest row 0,
// odd = dest row 1) as packed int16x8_t outputs (clamped).
//
// even = k0*r3 + k1*r2 + k2*r1 + k3*r0
// odd  = k0*r1 + k1*r2 + k2*r3 + k3*r4
//
// The two phases share the `round + k1*r2` accumulator init, computed once
// (int32 accumulation is exact, so regrouping preserves the reference result).
// Requires in scope: round (int32x4_t), k0..k3 (int16x4_t), kq0..kq3 (int16x8_t).
#define VERTICAL_FILTER_PAIR(vfEven, vfOdd, r0, r1, r2, r3, r4)                             \
    do {                                                                                    \
        const int32x4_t vfBaseLo = vmlal_s16(round, vget_low_s16((r2)), k1);                \
        const int32x4_t vfBaseHi = vmlal_high_s16(round, (r2), kq1);                        \
        int32x4_t evenLo = vmlal_s16(vfBaseLo, vget_low_s16((r3)), k0);                     \
        evenLo = vmlal_s16(evenLo, vget_low_s16((r1)), k2);                                 \
        evenLo = vmlal_s16(evenLo, vget_low_s16((r0)), k3);                                 \
        int32x4_t evenHi = vmlal_high_s16(vfBaseHi, (r3), kq0);                             \
        evenHi = vmlal_high_s16(evenHi, (r1), kq2);                                         \
        evenHi = vmlal_high_s16(evenHi, (r0), kq3);                                         \
        int32x4_t oddLo = vmlal_s16(vfBaseLo, vget_low_s16((r1)), k0);                      \
        oddLo = vmlal_s16(oddLo, vget_low_s16((r3)), k2);                                   \
        oddLo = vmlal_s16(oddLo, vget_low_s16((r4)), k3);                                   \
        int32x4_t oddHi = vmlal_high_s16(vfBaseHi, (r1), kq0);                              \
        oddHi = vmlal_high_s16(oddHi, (r3), kq2);                                           \
        oddHi = vmlal_high_s16(oddHi, (r4), kq3);                                           \
        (vfEven) = vshrq_n_s16(vqshrn_high_n_s32(vqshrn_n_s32(evenLo, 13), evenHi, 13), 1); \
        (vfOdd) = vshrq_n_s16(vqshrn_high_n_s32(vqshrn_n_s32(oddLo, 13), oddHi, 13), 1);    \
    } while (0)

// Horizontal filter: 1 vertical result → 1 output row (8 interleaved odd/even samples, clamped)
#define HORIZONTAL_FILTER(h)                                                             \
    __extension__({                                                                      \
        const int16x4_t s1 = vget_low_s16(vextq_s16((h), (h), 1));                       \
        const int16x4_t s2 = vget_low_s16(vextq_s16((h), (h), 2));                       \
        const int16x4_t s3 = vget_low_s16(vextq_s16((h), (h), 3));                       \
        const int16x4_t s4 = vget_low_s16(vextq_s16((h), (h), 4));                       \
        int32x4_t odd = vmlal_s16(round, vget_low_s16((h)), k3);                         \
        odd = vmlal_s16(odd, s1, k2);                                                    \
        odd = vmlal_s16(odd, s2, k1);                                                    \
        odd = vmlal_s16(odd, s3, k0);                                                    \
        int32x4_t even = vmlal_s16(round, s1, k0);                                       \
        even = vmlal_s16(even, s2, k1);                                                  \
        even = vmlal_s16(even, s3, k2);                                                  \
        even = vmlal_s16(even, s4, k3);                                                  \
        const int16x4x2_t zip = vzip_s16(vqshrn_n_s32(odd, 13), vqshrn_n_s32(even, 13)); \
        vshrq_n_s16(vcombine_s16(zip.val[0], zip.val[1]), 1);                            \
    })

// Wide horizontal filter: 8 centers -> 16 interleaved outputs in two int16x8_t.
//
// Takes five 8-lane tap windows t0..t4 where tj covers vertical-filter samples
// [x-2+j .. x+5+j] for block start x (so t2 is the aligned block itself).
// Equivalent to HORIZONTAL_FILTER on windows [x-2..x+5] and [x+2..x+9], but
// every multiply is full width (vmlal_s16 + vmlal_high_s16), the narrows pair
// up via vqshrn_high, and the odd/even interleave is a single zip.
//
// Requires in scope: round (int32x4_t), k0..k3 (int16x4_t) and kq0..kq3
// (int16x8_t kernel broadcasts).
//
// Accumulate/narrow core shared by the X2 variants: computes the odd and even
// filter phases (8 lanes each, clamped) from five 8-lane tap windows.
#define HORIZONTAL_FILTER_X2_CORE(odd, even, t0, t1, t2, t3, t4)                 \
    int32x4_t oddLo = vmlal_s16(round, vget_low_s16((t0)), k3);                  \
    oddLo = vmlal_s16(oddLo, vget_low_s16((t1)), k2);                            \
    oddLo = vmlal_s16(oddLo, vget_low_s16((t2)), k1);                            \
    oddLo = vmlal_s16(oddLo, vget_low_s16((t3)), k0);                            \
    int32x4_t oddHi = vmlal_high_s16(round, (t0), kq3);                          \
    oddHi = vmlal_high_s16(oddHi, (t1), kq2);                                    \
    oddHi = vmlal_high_s16(oddHi, (t2), kq1);                                    \
    oddHi = vmlal_high_s16(oddHi, (t3), kq0);                                    \
    int32x4_t evenLo = vmlal_s16(round, vget_low_s16((t1)), k0);                 \
    evenLo = vmlal_s16(evenLo, vget_low_s16((t2)), k1);                          \
    evenLo = vmlal_s16(evenLo, vget_low_s16((t3)), k2);                          \
    evenLo = vmlal_s16(evenLo, vget_low_s16((t4)), k3);                          \
    int32x4_t evenHi = vmlal_high_s16(round, (t1), kq0);                         \
    evenHi = vmlal_high_s16(evenHi, (t2), kq1);                                  \
    evenHi = vmlal_high_s16(evenHi, (t3), kq2);                                  \
    evenHi = vmlal_high_s16(evenHi, (t4), kq3);                                  \
    const int16x8_t odd = vqshrn_high_n_s32(vqshrn_n_s32(oddLo, 13), oddHi, 13); \
    const int16x8_t even = vqshrn_high_n_s32(vqshrn_n_s32(evenLo, 13), evenHi, 13)

// Planar form: tap windows advance one sample per tap; outputs interleave
// odd/even per 16-bit lane.
#define HORIZONTAL_FILTER_X2(destA, destB, t0, t1, t2, t3, t4)              \
    do {                                                                    \
        HORIZONTAL_FILTER_X2_CORE(odd, even, (t0), (t1), (t2), (t3), (t4)); \
        (destA) = vshrq_n_s16(vzip1q_s16(odd, even), 1);                    \
        (destB) = vshrq_n_s16(vzip2q_s16(odd, even), 1);                    \
    } while (0)

// Interleaved (UV pair) form: the same multiply core run directly on
// interleaved data with tap windows advancing one *pair* (two lanes) per tap.
// Lane i of tj is sample channel(i%2)[pair(i/2) - 2 + j], so odd/even come out
// as [oU0,oV0,oU1,oV1,...] / [eU0,eV0,...] and the output interleave is a
// 32-bit zip, giving [oU0,oV0,eU0,eV0, ...] — the NV12 store layout directly.
#define HORIZONTAL_FILTER_X2_UV(destA, destB, t0, t1, t2, t3, t4)                   \
    do {                                                                            \
        HORIZONTAL_FILTER_X2_CORE(odd, even, (t0), (t1), (t2), (t3), (t4));         \
        const int32x4_t odd32 = vreinterpretq_s32_s16(odd);                         \
        const int32x4_t even32 = vreinterpretq_s32_s16(even);                       \
        (destA) = vshrq_n_s16(vreinterpretq_s16_s32(vzip1q_s32(odd32, even32)), 1); \
        (destB) = vshrq_n_s16(vreinterpretq_s16_s32(vzip2q_s32(odd32, even32)), 1); \
    } while (0)

// PA relies on source/base and filtered values being in [-16384, 16383].
// Keep the implementations configuration-independent; APPLY_PA is evaluated
// when each loop expands these wrappers, not when this header is included.
static inline void neonPredictedAverage(int16x8_t* row0, int16x8_t* row1, int16x4_t base)
{
    const int32x4_t sum = vaddq_s32(vpaddlq_s16(*row0), vpaddlq_s16(*row1));
    const int16x4_t adjust = vsub_s16(base, vrshrn_n_s32(sum, 2));
    const int16x4x2_t adjust4 = vzip_s16(adjust, adjust);
    const int16x8_t adjust8 = vcombine_s16(adjust4.val[0], adjust4.val[1]);
    *row0 = vqaddq_s16(*row0, adjust8);
    *row1 = vqaddq_s16(*row1, adjust8);
}

static inline void neonPredictedAverage1D(int16x8_t* row, int16x4_t base)
{
    const int16x4_t adjust = vsub_s16(base, vrshrn_n_s32(vpaddlq_s16(*row), 1));
    const int16x4x2_t adjust4 = vzip_s16(adjust, adjust);
    *row = vqaddq_s16(*row, vcombine_s16(adjust4.val[0], adjust4.val[1]));
}

static inline void neonPredictedAverageUV(int16x8_t* row0, int16x8_t* row1, int16x4_t base)
{
    const int32x4_t sum_lo = vaddl_s16(vget_low_s16(*row0), vget_low_s16(*row1));
    const int32x4_t sum_hi = vaddl_s16(vget_high_s16(*row0), vget_high_s16(*row1));
    const int32x2_t block0 = vadd_s32(vget_low_s32(sum_lo), vget_high_s32(sum_lo));
    const int32x2_t block1 = vadd_s32(vget_low_s32(sum_hi), vget_high_s32(sum_hi));
    const int16x4_t adjust = vsub_s16(base, vrshrn_n_s32(vcombine_s32(block0, block1), 2));
    const int32x2_t adjust32 = vreinterpret_s32_s16(adjust);
    const int32x2x2_t adjust_zip = vzip_s32(adjust32, adjust32);
    const int16x8_t adjust8 = vreinterpretq_s16_s32(vcombine_s32(adjust_zip.val[0], adjust_zip.val[1]));
    *row0 = vqaddq_s16(*row0, adjust8);
    *row1 = vqaddq_s16(*row1, adjust8);
}

static inline void neonPredictedAverageUV1D(int16x8_t* row, int16x4_t base)
{
    const int32x4_t row32 = vreinterpretq_s32_s16(*row);
    const int32x2x2_t parts = vuzp_s32(vget_low_s32(row32), vget_high_s32(row32));
    const int32x4_t sums =
        vaddl_s16(vreinterpret_s16_s32(parts.val[0]), vreinterpret_s16_s32(parts.val[1]));
    const int16x4_t adjust = vsub_s16(base, vrshrn_n_s32(sums, 1));
    const int32x2_t adjust32 = vreinterpret_s32_s16(adjust);
    const int32x2x2_t adjust_zip = vzip_s32(adjust32, adjust32);
    const int16x8_t adjust8 = vreinterpretq_s16_s32(vcombine_s32(adjust_zip.val[0], adjust_zip.val[1]));
    *row = vqaddq_s16(*row, adjust8);
}

#define NEON_PA_DISABLED() (!(APPLY_PA))

#define PREDICTED_AVERAGE(row0, row1, base)                 \
    do {                                                    \
        if (!NEON_PA_DISABLED()) {                          \
            neonPredictedAverage(&(row0), &(row1), (base)); \
        }                                                   \
    } while (0)
#define PREDICTED_AVERAGE_1D(row, base)             \
    do {                                            \
        if (!NEON_PA_DISABLED()) {                  \
            neonPredictedAverage1D(&(row), (base)); \
        }                                           \
    } while (0)
#define PREDICTED_AVERAGE_UV(row0, row1, base)                \
    do {                                                      \
        if (!NEON_PA_DISABLED()) {                            \
            neonPredictedAverageUV(&(row0), &(row1), (base)); \
        }                                                     \
    } while (0)
#define PREDICTED_AVERAGE_UV_1D(row, base)            \
    do {                                              \
        if (!NEON_PA_DISABLED()) {                    \
            neonPredictedAverageUV1D(&(row), (base)); \
        }                                             \
    } while (0)

static inline int16x8_t neonDitherAddCodec(int16x8_t values, const uint16_t** buffer_ptr,
                                           uint16_t multiplier, int16x8_t offset, int16x8_t shift)
{
    uint16x8_t buffer = vld1q_u16(*buffer_ptr);
    *buffer_ptr += 8;
    const uint32x4_t lo = vmull_n_u16(vget_low_u16(buffer), multiplier);
#if defined(__aarch64__)
    const uint32x4_t hi = vmull_high_n_u16(buffer, multiplier);
    buffer = vuzp2q_u16(vreinterpretq_u16_u32(lo), vreinterpretq_u16_u32(hi));
#else
    const uint32x4_t hi = vmull_n_u16(vget_high_u16(buffer), multiplier);
    buffer = vuzpq_u16(vreinterpretq_u16_u32(lo), vreinterpretq_u16_u32(hi)).val[1];
#endif
    const int16x8_t scaled = vshlq_s16(vqsubq_s16(offset, vreinterpretq_s16_u16(buffer)), shift);
    return vqaddq_s16(values, scaled);
}

#define DITHER_ADD(row, dither_ptr, scale)                                                        \
    do {                                                                                          \
        if ((DITHER) && ditherEnabled) {                                                          \
            (row) = neonDitherAddCodec((row), &(dither_ptr), (scale), ditherOffset, ditherShift); \
        }                                                                                         \
    } while (0)

#define STORE(dest_ptr0, dest_ptr1, dest_row0a, dest_row0b, dest_row1a, dest_row1b) \
    do {                                                                            \
        STORE_VECTOR((dest_ptr0), 0, (dest_row0a));                                 \
        STORE_VECTOR((dest_ptr0), 8, (dest_row0b));                                 \
        STORE_VECTOR((dest_ptr1), 0, (dest_row1a));                                 \
        STORE_VECTOR((dest_ptr1), 8, (dest_row1b));                                 \
    } while (0)

// Shuffle bytes from two 16x8 registers
static inline int16x8_t shuffle2(int16x8_t v0, int16x8_t v1, uint8x16_t control)
{
    const uint8x16x2_t src = {{vreinterpretq_u8_s16(v0), vreinterpretq_u8_s16(v1)}};
    return vreinterpretq_s16_u8(vqtbl2q_u8(src, control));
}

// Edge-replication shuffle controls shared by the NEON upscalers.
//
// Planar left edge: builds the tap windows [-2..5] (left sample replicated
// twice) and [-1..6] (replicated once) from the first 8-sample block.
static const uint8x16_t leftEdgeShuf0 = {0, 1, 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const uint8x16_t leftEdgeShuf1 = {0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};

// Interleaved (UV pair, 4-byte unit) left edge: t0 window [pair(-2)..pair(1)]
// and t1 window [pair(-1)..pair(2)] with pair 0 replicated into the phantoms.
static const uint8x16_t leftEdgeShufT0 = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7};
static const uint8x16_t leftEdgeShufT1 = {0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

// Interleaved right edge: t3 window [pair(nP-3)..pair(nP)] and t4 window
// [pair(nP-2)..pair(nP+1)] with the last pair replicated into the phantoms.
static const uint8x16_t rightEdgeShufT3 = {4,  5,  6,  7,  8,  9,  10, 11,
                                           12, 13, 14, 15, 12, 13, 14, 15};
static const uint8x16_t rightEdgeShufT4 = {8,  9,  10, 11, 12, 13, 14, 15,
                                           12, 13, 14, 15, 12, 13, 14, 15};

// Planar right edge vqtbl2q controls that select the inputs for the two
// horizontal filters on the right edge from the last two carried vectors.
// Indexed by how much the right edge overhangs the 8-aligned body end.
//
// If the overhang is <= 4, the edge writes are moved back to recalculate and
// overwrite the last 16 dest samples of the body. This handles two cases:
//
//   - Overhang of 1: the last body block did not see the correct
//     edge-replicated input, so the last dest values would be wrong.
//
//   - Overhang <= 4: the destination only has space for one 16-sample write,
//     so moving back avoids a partial overwrite of the following row.
//
static const uint8x16_t rightEdgeControlA[8] = {
    // Replicated last sample in first slot
    /* 1 */ {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 16, 17},
    /* 2 */ {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19},
    /* 3 */ {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19},
    /* 4 */ {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19},

    /* 5 */ {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 24, 25},
    /* 6 */ {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
    /* 7 */ {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
    /* 8 */ {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27},
};

static const uint8x16_t rightEdgeControlB[8] = {
    /* 1 */ {12, 13, 14, 15, 16, 17, 16, 17, 16, 17, 16, 17, 16, 17, 16, 17},
    /* 2 */ {12, 13, 14, 15, 16, 17, 18, 19, 18, 19, 18, 19, 18, 19, 18, 19},
    /* 3 */ {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 20, 21, 20, 21, 20, 21},
    /* 4 */ {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 22, 23, 22, 23},

    /* 5 */ {20, 21, 22, 23, 24, 25, 24, 25, 24, 25, 24, 25, 24, 25, 24, 25},
    /* 6 */ {20, 21, 22, 23, 24, 25, 26, 27, 26, 27, 26, 27, 26, 27, 26, 27},
    /* 7 */ {20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 28, 29, 28, 29, 28, 29},
    /* 8 */ {20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 30, 31, 30, 31},
};

#endif // UPSCALE_PLAYGROUND_COMMON_NEON_H
