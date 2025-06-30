/* Copyright (c) V-Nova International Limited 2023-2025. All rights reserved.
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

// Vector and matrix multiplication. A substitute for linmath.h. Must be C-compatible.
//
#ifndef VN_LCEVC_API_UTILITY_LINEAR_MATH_H
#define VN_LCEVC_API_UTILITY_LINEAR_MATH_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define LINEAR_MATH_H_DEFINE_VEC(n, type, prefix)                                                        \
    typedef type prefix##Vec##n[n];                                                                      \
                                                                                                         \
    static inline void prefix##Vec##n##_add(prefix##Vec##n out, const prefix##Vec##n in1,                \
                                            const prefix##Vec##n in2)                                    \
    {                                                                                                    \
        for (size_t i = 0; i < n; ++i) {                                                                 \
            out[i] = in1[i] + in2[i];                                                                    \
        }                                                                                                \
    }                                                                                                    \
    static inline void prefix##Vec##n##_sub(prefix##Vec##n out, const prefix##Vec##n in1,                \
                                            const prefix##Vec##n in2)                                    \
    {                                                                                                    \
        for (size_t i = 0; i < n; ++i) {                                                                 \
            out[i] = in1[i] - in2[i];                                                                    \
        }                                                                                                \
    }                                                                                                    \
    static inline void prefix##Vec##n##_scale(prefix##Vec##n out, const prefix##Vec##n in, double scale) \
    {                                                                                                    \
        for (size_t i = 0; i < n; ++i) {                                                                 \
            out[i] = (type)(scale * in[i]);                                                              \
        }                                                                                                \
    }                                                                                                    \
    static inline type prefix##Vec##n##_mul_inner(const prefix##Vec##n in1, const prefix##Vec##n in2)    \
    {                                                                                                    \
        type out = (type)0;                                                                              \
        for (size_t i = 0; i < n; ++i) {                                                                 \
            out += in2[i] * in1[i];                                                                      \
        }                                                                                                \
        return out;                                                                                      \
    }                                                                                                    \
    static inline double prefix##Vec##n##_len(const prefix##Vec##n in)                                   \
    {                                                                                                    \
        return sqrt((double)prefix##Vec##n##_mul_inner(in, in));                                         \
    }                                                                                                    \
    static inline void prefix##Vec##n##_norm(prefix##Vec##n out, const prefix##Vec##n in)                \
    {                                                                                                    \
        double k = 1.0 / prefix##Vec##n##_len(in);                                                       \
        prefix##Vec##n##_scale(out, in, k);                                                              \
    }                                                                                                    \
    static inline void prefix##Vec##n##_set(prefix##Vec##n out, const prefix##Vec##n in1)                \
    {                                                                                                    \
        for (size_t i = 0; i < n; ++i) {                                                                 \
            out[i] = in1[i];                                                                             \
        }                                                                                                \
    }                                                                                                    \
    static inline void prefix##Vec##n##_min(prefix##Vec##n out, const prefix##Vec##n in1,                \
                                            const prefix##Vec##n in2)                                    \
    {                                                                                                    \
        for (size_t i = 0; i < n; ++i) {                                                                 \
            out[i] = in1[i] < in2[i] ? in1[i] : in2[i];                                                  \
        }                                                                                                \
    }                                                                                                    \
    static inline void prefix##Vec##n##_max(prefix##Vec##n out, const prefix##Vec##n in1,                \
                                            const prefix##Vec##n in2)                                    \
    {                                                                                                    \
        for (size_t i = 0; i < n; ++i) {                                                                 \
            out[i] = in1[i] > in2[i] ? in1[i] : in2[i];                                                  \
        }                                                                                                \
    }                                                                                                    \
    static inline bool prefix##Vec##n##_equals(const prefix##Vec##n in1, const prefix##Vec##n in2)       \
    {                                                                                                    \
        for (size_t i = 0; i < n; i++) {                                                                 \
            if (in1[i] != in2[i]) {                                                                      \
                return false;                                                                            \
            }                                                                                            \
        }                                                                                                \
        return true;                                                                                     \
    }

LINEAR_MATH_H_DEFINE_VEC(2, double, D)
LINEAR_MATH_H_DEFINE_VEC(3, double, D)
LINEAR_MATH_H_DEFINE_VEC(4, double, D)
LINEAR_MATH_H_DEFINE_VEC(2, uint32_t, U)
LINEAR_MATH_H_DEFINE_VEC(3, uint32_t, U)
LINEAR_MATH_H_DEFINE_VEC(4, uint32_t, U)
LINEAR_MATH_H_DEFINE_VEC(2, int32_t, I)
LINEAR_MATH_H_DEFINE_VEC(3, int32_t, I)
LINEAR_MATH_H_DEFINE_VEC(4, int32_t, I)
LINEAR_MATH_H_DEFINE_VEC(2, int16_t, I16)
LINEAR_MATH_H_DEFINE_VEC(3, int16_t, I16)
LINEAR_MATH_H_DEFINE_VEC(4, int16_t, I16)

typedef double Mat4x4[16];

// clang-format off
static const Mat4x4 kIdentity = {
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0,
};
// clang-format on

#define LINEAR_MATH_DEFINE_MAT4X4_MULT(vecInType, vecOutType, vecOutElType)                            \
    static inline void mat4x4_mul_##vecInType##_to_##vecOutType(vecOutType vecOut, const Mat4x4 matIn, \
                                                                const vecInType vecIn)                 \
    {                                                                                                  \
        for (size_t j = 0; j < 4; ++j) {                                                               \
            double res = 0.0;                                                                          \
            for (size_t i = 0; i < 4; ++i) {                                                           \
                res += matIn[4 * j + i] * (double)(vecIn[i]);                                          \
            }                                                                                          \
            vecOut[j] = (vecOutElType)res;                                                             \
        }                                                                                              \
    }

LINEAR_MATH_DEFINE_MAT4X4_MULT(DVec4, I16Vec4, int16_t)
LINEAR_MATH_DEFINE_MAT4X4_MULT(DVec4, DVec4, double)
LINEAR_MATH_DEFINE_MAT4X4_MULT(I16Vec4, DVec4, double)

#endif // VN_LCEVC_API_UTILITY_LINEAR_MATH_H
