/* Copyright (c) V-Nova International Limited 2024-2026. All rights reserved.
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

// Used for for expanding empty lists
//
#define _VNForEachEmpty(_op, ...)

// Chain of macros that expand to "op(argIndex, argumentValue)" separated by 'sep' for n arguments where 'op' is a macro
//
#define _VNForEach1(_op, _sep, _idx, _arg, ...) _op(_idx, _arg)
#define _VNForEach2(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() _VNForEach1(_op, _sep, _idx + 1, __VA_ARGS__)
#define _VNForEach3(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach2(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach4(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach3(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach5(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach4(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach6(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach5(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach7(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach6(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach8(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach7(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach9(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach8(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach10(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach9(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach11(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach10(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach12(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach11(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach13(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach12(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach14(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach13(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach15(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach14(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach16(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach15(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach17(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach16(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach18(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach17(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach19(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach18(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach20(_op, _sep, _idx, _arg, ...) \
    _op(_idx, _arg) _sep() VNExpand(_VNForEach19(_op, _sep, _idx + 1, __VA_ARGS__))

// First step in above chain that adds a prefix
//
#define _VNForEach1Prefix(_op, _pfx, _sep, _idx, _arg, ...) _pfx() _op(_idx, _arg)
#define _VNForEach2Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() _VNForEach1(_op, _sep, _idx + 1, __VA_ARGS__)
#define _VNForEach3Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach2(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach4Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach3(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach5Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach4(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach6Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach5(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach7Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach6(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach8Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach7(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach9Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach8(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach10Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach9(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach11Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach10(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach12Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach11(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach13Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach12(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach14Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach13(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach15Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach14(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach16Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach15(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach17Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach16(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach18Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach17(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach19Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach18(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEach20Prefix(_op, _pfx, _sep, _idx, _arg, ...) \
    _pfx() _op(_idx, _arg) _sep() VNExpand(_VNForEach19(_op, _sep, _idx + 1, __VA_ARGS__))

// Chain of macros that expand to "op(argIndex, argumentValue0, argumentValue1)" separated by 'sep' for n arguments pairs where 'op' is a macro
//
#define _VNForEachPair1(_op, _sep, _idx, _arg0, _arg1, ...) _op(_idx, _arg0, _arg1)
#define _VNForEachPair2(_op, _sep, _idx, _arg0, _arg1, ...) \
    _op(_idx, _arg0, _arg1) _sep() _VNForEachPair1(_op, _sep, _idx + 1, __VA_ARGS__)
#define _VNForEachPair3(_op, _sep, _idx, _arg0, _arg1, ...) \
    _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair2(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair4(_op, _sep, _idx, _arg0, _arg1, ...) \
    _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair3(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair5(_op, _sep, _idx, _arg0, _arg1, ...) \
    _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair4(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair6(_op, _sep, _idx, _arg0, _arg1, ...) \
    _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair5(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair7(_op, _sep, _idx, _arg0, _arg1, ...) \
    _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair6(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair8(_op, _sep, _idx, _arg0, _arg1, ...) \
    _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair7(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair9(_op, _sep, _idx, _arg0, _arg1, ...) \
    _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair8(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair10(_op, _sep, _idx, _arg0, _arg1, ...) \
    _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair9(_op, _sep, _idx + 1, __VA_ARGS__))

// First step in above chain that adds a prefix
//
#define _VNForEachPair1Prefix(_op, _pfx, _sep, _idx, _arg0, _arg1, ...) \
    _pfx() _op(_idx, _arg0, _arg1)
#define _VNForEachPair2Prefix(_op, _pfx, _sep, _idx, _arg0, _arg1, ...) \
    _pfx() _op(_idx, _arg0, _arg1) _sep() _VNForEachPair1(_op, _sep, _idx + 1, __VA_ARGS__)
#define _VNForEachPair3Prefix(_op, _pfx, _sep, _idx, _arg0, _arg1, ...) \
    _pfx() _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair2(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair4Prefix(_op, _pfx, _sep, _idx, _arg0, _arg1, ...) \
    _pfx() _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair3(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair5Prefix(_op, _pfx, _sep, _idx, _arg0, _arg1, ...) \
    _pfx() _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair4(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair6Prefix(_op, _pfx, _sep, _idx, _arg0, _arg1, ...) \
    _pfx() _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair5(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair7Prefix(_op, _pfx, _sep, _idx, _arg0, _arg1, ...) \
    _pfx() _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair6(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair8Prefix(_op, _pfx, _sep, _idx, _arg0, _arg1, ...) \
    _pfx() _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair7(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair9Prefix(_op, _pfx, _sep, _idx, _arg0, _arg1, ...) \
    _pfx() _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair8(_op, _sep, _idx + 1, __VA_ARGS__))
#define _VNForEachPair10Prefix(_op, _pfx, _sep, _idx, _arg0, _arg1, ...) \
    _pfx() _op(_idx, _arg0, _arg1) _sep() VNExpand(_VNForEachPair9(_op, _sep, _idx + 1, __VA_ARGS__))

/*
 * Mechanism for selecting nth argument based on number of varargs (limited to 20)
 */
#define _VNNthArg(_Ignored, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, \
                  _17, _18, _19, _20, N, ...)                                                      \
    N

/*
 * Expands to op(n, arg) for each argument, prefixed by `pfx()` when not empty, and separated by `sep()`
 */
#define VNForEach(op, pfx, sep, ...)                                                               \
    VNExpand(_VNNthArg(_Ignored, ##__VA_ARGS__, _VNForEach20Prefix, _VNForEach19Prefix,            \
                       _VNForEach18Prefix, _VNForEach17Prefix, _VNForEach16Prefix,                 \
                       _VNForEach15Prefix, _VNForEach14Prefix, _VNForEach13Prefix,                 \
                       _VNForEach12Prefix, _VNForEach11Prefix, _VNForEach10Prefix,                 \
                       _VNForEach9Prefix, _VNForEach8Prefix, _VNForEach7Prefix, _VNForEach6Prefix, \
                       _VNForEach5Prefix, _VNForEach4Prefix, _VNForEach3Prefix, _VNForEach2Prefix, \
                       _VNForEach1Prefix, _VNForEachEmpty)(op, pfx, sep, 0, ##__VA_ARGS__))

/*
 * Expands to op(n, arg0, arg1) for each pair of arguments, prefixed by `pfx()` when not empty, and separated by `sep()`
 */
#define VNForEachPair(op, pfx, sep, ...)                                                         \
    VNExpand(_VNNthArg(_Ignored, ##__VA_ARGS__, _VNForEachPair10Prefix, _VNOddNumberOfArguments, \
                       _VNForEachPair9Prefix, _VNOddNumberOfArguments, _VNForEachPair8Prefix,    \
                       _VNOddNumberOfArguments, _VNForEachPair7Prefix, _VNOddNumberOfArguments,  \
                       _VNForEachPair6Prefix, _VNOddNumberOfArguments, _VNForEachPair5Prefix,    \
                       _VNOddNumberOfArguments, _VNForEachPair4Prefix, _VNOddNumberOfArguments,  \
                       _VNForEachPair3Prefix, _VNOddNumberOfArguments, _VNForEachPair2Prefix,    \
                       _VNOddNumberOfArguments, _VNForEachPair1Prefix, _VNOddNumberOfArguments,  \
                       _VNForEachEmpty)(op, pfx, sep, 0, ##__VA_ARGS__))

// Preprocessor helpers.
#define VNConcatHelper(a, b) a##b
#define VNConcat(a, b) VNConcatHelper(a, b)

#endif // VN_LCEVC_COMMON_CPP_TOOLS_H
