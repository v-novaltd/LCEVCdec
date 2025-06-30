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

#ifndef VN_LCEVC_COMMON_CPP_TOOLS_H
#define VN_LCEVC_COMMON_CPP_TOOLS_H

#define VNExpand(x) x

#define _VNNthArg(_Ignored, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N

#define _VNForEach0(_op, ...)
#define _VNForEach1(_op, _idx, _arg) _op(_idx, _arg)
#define _VNForEach2(_op, _idx, _arg, ...) _op(_idx, _arg) _VNForEach1(_op, _idx + 1, __VA_ARGS__)
#define _VNForEach3(_op, _idx, _arg, ...) \
    _op(_idx, _arg) VNExpand(_VNForEach2(_op, _idx + 1, __VA_ARGS__))
#define _VNForEach4(_op, _idx, _arg, ...) \
    _op(_idx, _arg) VNExpand(_VNForEach3(_op, _idx + 1, __VA_ARGS__))
#define _VNForEach5(_op, _idx, _arg, ...) \
    _op(_idx, _arg) VNExpand(_VNForEach4(_op, _idx + 1, __VA_ARGS__))
#define _VNForEach6(_op, _idx, _arg, ...) \
    _op(_idx, _arg) VNExpand(_VNForEach5(_op, _idx + 1, __VA_ARGS__))
#define _VNForEach7(_op, _idx, _arg, ...) \
    _op(_idx, _arg) VNExpand(_VNForEach6(_op, _idx + 1, __VA_ARGS__))
#define _VNForEach8(_op, _idx, _arg, ...) \
    _op(_idx, _arg) VNExpand(_VNForEach7(_op, _idx + 1, __VA_ARGS__))
#define _VNForEach9(_op, _idx, _arg, ...) \
    _op(_idx, _arg) VNExpand(_VNForEach8(_op, _idx + 1, __VA_ARGS__))
#define _VNForEach10(_op, _idx, _arg, ...) \
    _op(_idx, _arg) VNExpand(_VNForEach9(_op, _idx + 1, __VA_ARGS__))

#define _VNForEach1Sep(_op, _sep, _idx, _arg, ...) _op(_idx, _arg)
#define _VNForEach2Sep(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() _VNForEach1Sep(_op, _sep, _idx + 1, __VA_ARGS__)
#define _VNForEach3Sep(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach2Sep(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach4Sep(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach3Sep(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach5Sep(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach4Sep(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach6Sep(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach5Sep(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach7Sep(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach6Sep(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach8Sep(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach7Sep(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach9Sep(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach8Sep(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach10Sep(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach9Sep(_op, _sep, _idx + 1, __VA_ARGS__))

/*
 * Expands to op(n, arg) for each argument
 */
#define VNForEach(op, ...)                                                              \
    VNExpand(_VNNthArg(_Ignored, ##__VA_ARGS__, _VNForEach10, _VNForEach9, _VNForEach8, \
                       _VNForEach7, _VNForEach6, _VNForEach5, _VNForEach4, _VNForEach3, \
                       _VNForEach2, _VNForEach1, _VNForEach0)(op, 0, ##__VA_ARGS__))

/*
 * Expands to op(n, arg) for each argument separated by `sep`
 */
#define VNForEachSeperated(op, sep, ...)                                                               \
    VNExpand(_VNNthArg(_Ignored, ##__VA_ARGS__, _VNForEach10Sep, _VNForEach9Sep, _VNForEach8Sep,       \
                       _VNForEach7Sep, _VNForEach6Sep, _VNForEach5Sep, _VNForEach4Sep, _VNForEach3Sep, \
                       _VNForEach2Sep, _VNForEach1Sep, _VNForEach0)(op, sep, 0, __VA_ARGS__))

/*
 * Expands to the number of arguments
 */
#define VNNumArgs(...) \
    VNExpand(_VNNthArg(_Ignored, ##__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))

// Preprocessor helpers.
#define VNConcatHelper(a, b) a##b
#define VNConcat(a, b) VNConcatHelper(a, b)

#endif // VN_LCEVC_COMMON_CPP_TOOLS_H
