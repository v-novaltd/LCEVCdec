/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#include "string_format.h"

#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/limit.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Classification of chars
//
typedef enum CharClass
{
    EOS,                  // \0
    Percent,              // %
    Point,                // .
    Digit,                // 0-9
    Asterix,              // *
    Plus,                 // +
    Minus,                // -
    Space,                // ' '
    Hash,                 // -
    LetterLength,         // lhLHz
    LetterFormatInt,      // di
    LetterFormatUnsigned, // oxX
    LetterFormatFloat,    // fFeEgGaA
    LetterFormatString,   // s
    LetterFormatChar,     // s
    LetterFormatPointer,  // p
    LetterOther,          //
    Other,                //
} CharClass;

// Per character classification table
//
static const enum CharClass charClasses[256];

// Parser state
//
typedef enum State
{
    Text,
    Flag,
    Width,
    Precision,
    Length,
    Format,
} State;

void ldcFormatParseInitialise(LdcFormatParser* parser, const char* format)
{
    //    VNCLear(parser);
    parser->format = format;
    parser->ptr = format;
    parser->argumentIndex = 0;
}

// Convert integer sizes to argument type
static inline LdcDiagArg signedArgType(size_t size)
{
    switch (size) {
        case 1: return LdcDiagArgInt8;
        case 2: return LdcDiagArgInt16;
        case 4: return LdcDiagArgInt32;
        case 8: return LdcDiagArgInt64;
        default: return LdcDiagArgNone;
    }
}

static inline LdcDiagArg unsignedArgType(size_t size)
{
    switch (size) {
        case 1: return LdcDiagArgUInt8;
        case 2: return LdcDiagArgUInt16;
        case 4: return LdcDiagArgUInt32;
        case 8: return LdcDiagArgUInt64;
        default: return LdcDiagArgNone;
    }
}

bool ldcFormatParseNext(LdcFormatParser* parser, LdcFormatElement* element)
{
    const char* start = parser->ptr;

    if (*start == '\0') {
        return false;
    }

    // Move over any plain text
    const char* ptr = start;
    while (*ptr && *ptr != '%') {
        ptr++;
    }

    if (ptr != start) {
        // Emit plain text chunk
        element->start = start;
        element->length = ptr - start;
        element->type = LdcDiagArgNone;
        element->argumentCount = 0;
        element->argumentIndex = 0;
        parser->ptr = ptr;
        return true;
    }

    // Got a format specifier - skip th e'%''
    start = ptr++;

    // For '*' width/precision
    size_t extraArgs = 0;
    unsigned int length = 0;

    // State machine for format specifier:
    //
    //  Flag->Width->Precision->Length->Format
    //
    // Stops at Format
    //
    enum State state = Flag;

    while (*ptr && state != Format) {
        // Classify the next character
        enum CharClass cc = charClasses[(unsigned char)*ptr];

        switch (state) {
            case Flag:
                if (cc == Minus || cc == Plus || cc == Space || cc == Hash || cc == Digit) {
                    ptr++;
                } else {
                    state = Width;
                }
                break;

            case Width:
                if (cc == Asterix) {
                    extraArgs++;
                    ptr++;
                } else if (cc == Digit) {
                    ptr++;
                } else if (cc == Point) {
                    state = Precision;
                    ptr++;
                } else {
                    state = Length;
                }
                break;

            case Precision:
                if (cc == Asterix) {
                    extraArgs++;
                    ptr++;
                } else if (cc == Digit) {
                    ptr++;
                } else {
                    state = Length;
                }
                break;

            case Length:
                if (cc == LetterLength) {
                    length = (length << 8) + *ptr;
                    ptr++;
                } else {
                    state = Format;
                }
                break;

            default: break;
        }
    }

    // Classify and move over specifier
    enum CharClass sc = charClasses[(unsigned char)*ptr++];
    if (sc == EOS) {
        return false;
    }

    // Derive type from length and format
    LdcDiagArg type = LdcDiagArgNone;

    switch (sc) {
        case LetterFormatInt:
            switch (length) {
                case ('h' << 8) + 'h': type = signedArgType(sizeof(char)); break;
                case ('l' << 8) + 'l': type = signedArgType(sizeof(long long int)); break;
                case 'h': type = signedArgType(sizeof(short int)); break;
                case 'l': type = signedArgType(sizeof(long int)); break;
                case 'j': type = signedArgType(sizeof(intmax_t)); break;
                case 'z': type = signedArgType(sizeof(size_t)); break;
                case 't': type = signedArgType(sizeof(ptrdiff_t)); break;
                default: type = signedArgType(sizeof(int)); break;
            }
            break;

        case LetterFormatUnsigned:
            switch (length) {
                case ('h' << 8) + 'h': type = unsignedArgType(sizeof(unsigned char)); break;
                case ('l' << 8) + 'l':
                    type = unsignedArgType(sizeof(unsigned long long int));
                    break;
                case 'h': type = unsignedArgType(sizeof(unsigned short int)); break;
                case 'l': type = unsignedArgType(sizeof(unsigned long int)); break;
                case 'j': type = unsignedArgType(sizeof(uintmax_t)); break;
                case 'z': type = unsignedArgType(sizeof(size_t)); break;
                case 't': type = unsignedArgType(sizeof(ptrdiff_t)); break;
                default: type = unsignedArgType(sizeof(unsigned int)); break;
            }
            break;

            // Does not support "Lf" (long double)
        case LetterFormatFloat:
            type = LdcDiagArgFloat64;
            break;
            // Does not support "lc" (wchar_t *)
        case LetterFormatString:
            type = LdcDiagArgConstCharPtr;
            break;
            // Does not support "ls" (wchar_t)
        case LetterFormatChar: type = LdcDiagArgChar; break;
        case LetterFormatPointer: type = LdcDiagArgConstVoidPtr; break;

        case Percent:
            // Literal percent – no arguments
            start++;
            // Fall through
        default:
            // Anything else
            type = LdcDiagArgNone;
            break;
    }

    // Fill in element details
    element->type = type;
    element->start = start;
    element->length = ptr - start;
    if (type != LdcDiagArgNone) {
        element->argumentIndex = parser->argumentIndex;
        element->argumentCount = 1 + extraArgs;
    } else {
        element->argumentCount = 0;
        element->argumentIndex = 0;
    }

    // Update parser
    parser->ptr = ptr;
    parser->argumentIndex += element->argumentCount;
    return true;
}

// Utility functions to get diagnostic values into the promoted vararg types
//
static inline int argumentAsInt(size_t idx, const LdcDiagArg* types, const LdcDiagValue* values);
static inline unsigned argumentAsUnsignedInt(size_t idx, const LdcDiagArg* types, const LdcDiagValue* values);
static inline int64_t argumentAsLongLongInt(size_t idx, const LdcDiagArg* types, const LdcDiagValue* values);
static inline uint64_t argumentAsUnsignedLongLongInt(size_t idx, const LdcDiagArg* types,
                                                     const LdcDiagValue* values);
static inline const char* argumentAsString(size_t idx, const LdcDiagArg* types, const LdcDiagValue* values);
static inline const void* argumentAsPointer(size_t idx, const LdcDiagArg* types, const LdcDiagValue* values);
static inline double argumentAsDouble(size_t idx, const LdcDiagArg* types, const LdcDiagValue* values);

// Use the parser to format a string using platform printf for each element
//
size_t ldcFormat(char* dst, uint32_t dstSize, const char* format, const LdcDiagArg* types,
                 const LdcDiagValue* values, size_t count)
{
    char* ptr = dst;
    LdcFormatParser parser;

    if (dstSize <= 1) {
        // No room
        return 0;
    }
    // Allow for terminator
    dstSize--;

    ldcFormatParseInitialise(&parser, format);

    LdcFormatElement element;

    while (ldcFormatParseNext(&parser, &element)) {
        // How much space is left?
        const size_t remaining = dstSize - (ptr - dst);
        if (remaining == 0) {
            break;
        }

        // Just transcribe text
        if (element.type == LdcDiagArgNone) {
            size_t size = minU32((uint32_t)element.length, (uint32_t)remaining);
            memcpy(ptr, element.start, size);
            ptr += size;
            continue;
        }

        // Are there enough arguments?
        if (element.argumentIndex + element.argumentCount > count) {
            break;
        }

        // Make a null terminated copy of the single specifier from the format string
        char specifier[32];
        size_t size = minU32((uint32_t)sizeof(specifier) - 1, (uint32_t)element.length);
        strncpy(specifier, element.start, size);
        specifier[size] = '\0';

        // Are there value arguments?
        if (values == NULL) {
            ptr += snprintf(ptr, remaining, "<No Values>");
            continue;
        }

        size = 0;
        switch (element.argumentCount) {
            case 1:
                // Pass single value to snprintf
                switch (element.type) {
                    case LdcDiagArgChar:
                    case LdcDiagArgInt8:
                    case LdcDiagArgInt16:
                    case LdcDiagArgInt32:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsInt(element.argumentIndex, types, values));
                        break;

                    case LdcDiagArgUInt8:
                    case LdcDiagArgUInt16:
                    case LdcDiagArgUInt32:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsUnsignedInt(element.argumentIndex, types, values));
                        break;

                    case LdcDiagArgInt64:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsLongLongInt(element.argumentIndex, types, values));
                        break;

                    case LdcDiagArgUInt64:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsUnsignedLongLongInt(element.argumentIndex, types, values));
                        break;

                    case LdcDiagArgCharPtr:
                    case LdcDiagArgConstCharPtr:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsString(element.argumentIndex, types, values));
                        break;

                    case LdcDiagArgVoidPtr:
                    case LdcDiagArgConstVoidPtr:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsPointer(element.argumentIndex, types, values));
                        break;

                    case LdcDiagArgFloat32:
                    case LdcDiagArgFloat64:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsDouble(element.argumentIndex, types, values));
                        break;

                    default: break;
                }
                break;

            case 2:
                // Pass value to snprintf and one extra integer (width or precision)
                switch (element.type) {
                    case LdcDiagArgChar:
                    case LdcDiagArgInt8:
                    case LdcDiagArgInt16:
                    case LdcDiagArgInt32:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsInt(element.argumentIndex, types, values),
                                        argumentAsInt(element.argumentIndex + 1, types, values));
                        break;

                    case LdcDiagArgUInt8:
                    case LdcDiagArgUInt16:
                    case LdcDiagArgUInt32:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsInt(element.argumentIndex, types, values),
                                        argumentAsUnsignedInt(element.argumentIndex + 1, types, values));
                        break;

                    case LdcDiagArgInt64:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsInt(element.argumentIndex, types, values),
                                        argumentAsLongLongInt(element.argumentIndex + 1, types, values));
                        break;

                    case LdcDiagArgUInt64:
                        size = snprintf(
                            ptr, remaining, specifier, argumentAsInt(element.argumentIndex, types, values),
                            argumentAsUnsignedLongLongInt(element.argumentIndex + 1, types, values));
                        break;

                    case LdcDiagArgCharPtr:
                    case LdcDiagArgConstCharPtr:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsInt(element.argumentIndex, types, values),
                                        argumentAsString(element.argumentIndex + 1, types, values));
                        break;

                    case LdcDiagArgVoidPtr:
                    case LdcDiagArgConstVoidPtr:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsInt(element.argumentIndex, types, values),
                                        argumentAsPointer(element.argumentIndex + 1, types, values));
                        break;

                    case LdcDiagArgFloat32:
                    case LdcDiagArgFloat64:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsInt(element.argumentIndex, types, values),
                                        argumentAsDouble(element.argumentIndex + 1, types, values));
                        break;

                    default: break;
                }
                break;

            case 3:
                // Pass value to snprintf and two extra integers (width and precision)
                switch (element.type) {
                    case LdcDiagArgChar:
                    case LdcDiagArgInt8:
                    case LdcDiagArgInt16:
                    case LdcDiagArgInt32:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsInt(element.argumentIndex, types, values),
                                        argumentAsInt(element.argumentIndex + 1, types, values),
                                        argumentAsInt(element.argumentIndex + 2, types, values));
                        break;

                    case LdcDiagArgUInt8:
                    case LdcDiagArgUInt16:
                    case LdcDiagArgUInt32:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsInt(element.argumentIndex, types, values),
                                        argumentAsInt(element.argumentIndex + 1, types, values),
                                        argumentAsUnsignedInt(element.argumentIndex + 2, types, values));
                        break;

                    case LdcDiagArgInt64:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsInt(element.argumentIndex, types, values),
                                        argumentAsInt(element.argumentIndex + 1, types, values),
                                        argumentAsLongLongInt(element.argumentIndex + 2, types, values));
                        break;

                    case LdcDiagArgUInt64:
                        size = snprintf(
                            ptr, remaining, specifier, argumentAsInt(element.argumentIndex, types, values),
                            argumentAsInt(element.argumentIndex + 1, types, values),
                            argumentAsUnsignedLongLongInt(element.argumentIndex + 2, types, values));
                        break;

                    case LdcDiagArgCharPtr:
                    case LdcDiagArgConstCharPtr:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsInt(element.argumentIndex, types, values),
                                        argumentAsInt(element.argumentIndex + 1, types, values),
                                        argumentAsString(element.argumentIndex + 2, types, values));
                        break;

                    case LdcDiagArgVoidPtr:
                    case LdcDiagArgConstVoidPtr:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsInt(element.argumentIndex, types, values),
                                        argumentAsInt(element.argumentIndex + 1, types, values),
                                        argumentAsPointer(element.argumentIndex + 2, types, values));
                        break;

                    case LdcDiagArgFloat32:
                    case LdcDiagArgFloat64:
                        size = snprintf(ptr, remaining, specifier,
                                        argumentAsInt(element.argumentIndex, types, values),
                                        argumentAsInt(element.argumentIndex + 1, types, values),
                                        argumentAsDouble(element.argumentIndex + 2, types, values));
                        break;

                    default: break;
                }
                break;
            default: assert(0);
        }

        // Bump destination by size of formatted element
        ptr += size;
    }

    // Terminate output
    *ptr = '\0';
    return ptr - dst;
}

static inline int argumentAsInt(size_t idx, const LdcDiagArg* types, const LdcDiagValue* values)
{
    switch (types[idx]) {
        case LdcDiagArgBool: return values[idx].valueBool;
        case LdcDiagArgChar: return values[idx].valueChar;
        case LdcDiagArgInt8: return values[idx].valueInt8;
        case LdcDiagArgUInt8: return values[idx].valueUInt8;
        case LdcDiagArgInt16: return values[idx].valueInt16;
        case LdcDiagArgUInt16: return values[idx].valueUInt16;
        case LdcDiagArgInt32: return values[idx].valueInt32;
        case LdcDiagArgUInt32: return values[idx].valueUInt32;
        case LdcDiagArgInt64: return (int)values[idx].valueInt64;
        case LdcDiagArgUInt64: return (int)values[idx].valueUInt64;
        default: return 0;
    }
}

static inline unsigned int argumentAsUnsignedInt(size_t idx, const LdcDiagArg* types,
                                                 const LdcDiagValue* values)
{
    switch (types[idx]) {
        case LdcDiagArgBool: return values[idx].valueBool;
        case LdcDiagArgChar: return values[idx].valueChar;
        case LdcDiagArgInt8: return values[idx].valueInt8;
        case LdcDiagArgUInt8: return values[idx].valueUInt8;
        case LdcDiagArgInt16: return values[idx].valueInt16;
        case LdcDiagArgUInt16: return values[idx].valueUInt16;
        case LdcDiagArgInt32: return values[idx].valueInt32;
        case LdcDiagArgUInt32: return values[idx].valueUInt32;
        case LdcDiagArgInt64: return (unsigned int)values[idx].valueInt64;
        case LdcDiagArgUInt64: return (unsigned int)values[idx].valueUInt64;
        default: return 0;
    }
}

static inline int64_t argumentAsLongLongInt(size_t idx, const LdcDiagArg* types, const LdcDiagValue* values)
{
    switch (types[idx]) {
        case LdcDiagArgBool: return values[idx].valueBool;
        case LdcDiagArgChar: return values[idx].valueChar;
        case LdcDiagArgInt8: return values[idx].valueInt8;
        case LdcDiagArgUInt8: return values[idx].valueUInt8;
        case LdcDiagArgInt16: return values[idx].valueInt16;
        case LdcDiagArgUInt16: return values[idx].valueUInt16;
        case LdcDiagArgInt32: return values[idx].valueInt32;
        case LdcDiagArgUInt32: return values[idx].valueUInt32;
        case LdcDiagArgInt64: return values[idx].valueInt64;
        case LdcDiagArgUInt64: return values[idx].valueUInt64;
        default: return 0;
    }
}

static inline uint64_t argumentAsUnsignedLongLongInt(size_t idx, const LdcDiagArg* types,
                                                     const LdcDiagValue* values)
{
    switch (types[idx]) {
        case LdcDiagArgBool: return values[idx].valueBool;
        case LdcDiagArgChar: return values[idx].valueChar;
        case LdcDiagArgInt8: return values[idx].valueInt8;
        case LdcDiagArgUInt8: return values[idx].valueUInt8;
        case LdcDiagArgInt16: return values[idx].valueInt16;
        case LdcDiagArgUInt16: return values[idx].valueUInt16;
        case LdcDiagArgInt32: return values[idx].valueInt32;
        case LdcDiagArgUInt32: return values[idx].valueUInt32;
        case LdcDiagArgInt64: return values[idx].valueInt64;
        case LdcDiagArgUInt64: return values[idx].valueUInt64;
        default: return 0;
    }
}

static inline const char* argumentAsString(size_t idx, const LdcDiagArg* types, const LdcDiagValue* values)
{
    switch (types[idx]) {
        case LdcDiagArgCharPtr: return values[idx].valueCharPtr;
        case LdcDiagArgConstCharPtr: return values[idx].valueConstCharPtr;
        default: return "?";
    }
}

static inline const void* argumentAsPointer(size_t idx, const LdcDiagArg* types, const LdcDiagValue* values)
{
    switch (types[idx]) {
        case LdcDiagArgVoidPtr: return values[idx].valueCharPtr;
        case LdcDiagArgConstVoidPtr: return values[idx].valueConstVoidPtr;
        default: return NULL;
    }
}

static inline double argumentAsDouble(size_t idx, const LdcDiagArg* types, const LdcDiagValue* values)
{
    switch (types[idx]) {
        case LdcDiagArgFloat32: return values[idx].valueFloat32;
        case LdcDiagArgFloat64: return values[idx].valueFloat64;
        default: return 0.0;
    }
}

// Char classification down here to keep it out of the way
static const enum CharClass charClasses[256] = {
    EOS,                  //   0 ' '
    Other,                //   1 ' '
    Other,                //   2 ' '
    Other,                //   3 ' '
    Other,                //   4 ' '
    Other,                //   5 ' '
    Other,                //   6 ' '
    Other,                //   7 ' '
    Other,                //   8 ' '
    Other,                //   9 ' '
    Other,                //  10 ' '
    Other,                //  11 ' '
    Other,                //  12 ' '
    Other,                //  13 ' '
    Other,                //  14 ' '
    Other,                //  15 ' '
    Other,                //  16 ' '
    Other,                //  17 ' '
    Other,                //  18 ' '
    Other,                //  19 ' '
    Other,                //  20 ' '
    Other,                //  21 ' '
    Other,                //  22 ' '
    Other,                //  23 ' '
    Other,                //  24 ' '
    Other,                //  25 ' '
    Other,                //  26 ' '
    Other,                //  27 ' '
    Other,                //  28 ' '
    Other,                //  29 ' '
    Other,                //  30 ' '
    Other,                //  31 ' '
    Space,                //  32 ' '
    Other,                //  33 '!'
    Other,                //  34 '"'
    Hash,                 //  35 '#'
    Other,                //  36 '$'
    Percent,              //  37 '%'
    Other,                //  38 '&'
    Other,                //  39 '''
    Other,                //  40 '('
    Other,                //  41 ')'
    Asterix,              //  42 '*'
    Plus,                 //  43 '+'
    Other,                //  44 ','
    Minus,                //  45 '-'
    Point,                //  46 '.'
    Other,                //  47 '/'
    Digit,                //  48 '0'
    Digit,                //  49 '1'
    Digit,                //  50 '2'
    Digit,                //  51 '3'
    Digit,                //  52 '4'
    Digit,                //  53 '5'
    Digit,                //  54 '6'
    Digit,                //  55 '7'
    Digit,                //  56 '8'
    Digit,                //  57 '9'
    Other,                //  58 ':'
    Other,                //  59 ';'
    Other,                //  60 '<'
    Other,                //  61 '='
    Other,                //  62 '>'
    Other,                //  63 '?'
    Other,                //  64 '@'
    LetterFormatFloat,    //  65 'A'
    Other,                //  66 'B'
    Other,                //  67 'C'
    Other,                //  68 'D'
    LetterFormatFloat,    //  69 'E'
    LetterFormatFloat,    //  70 'F'
    LetterFormatFloat,    //  71 'G'
    Other,                //  72 'H'
    Other,                //  73 'I'
    Other,                //  74 'J'
    Other,                //  75 'K'
    LetterLength,         //  76 'L'
    Other,                //  77 'M'
    Other,                //  78 'N'
    Other,                //  79 'O'
    Other,                //  80 'P'
    Other,                //  81 'Q'
    Other,                //  82 'R'
    Other,                //  83 'S'
    Other,                //  84 'T'
    Other,                //  85 'U'
    Other,                //  86 'V'
    Other,                //  87 'W'
    LetterFormatUnsigned, //  88 'X'
    Other,                //  89 'Y'
    Other,                //  90 'Z'
    Other,                //  91 '['
    Other,                //  92 '\'
    Other,                //  93 ']'
    Other,                //  94 '^'
    Other,                //  95 '_'
    Other,                //  96 '`'
    LetterFormatFloat,    //  97 'a'
    Other,                //  98 'b'
    LetterFormatChar,     //  99 'c'
    LetterFormatInt,      // 100 'd'
    LetterFormatFloat,    // 101 'e'
    LetterFormatFloat,    // 102 'f'
    LetterFormatFloat,    // 103 'g'
    LetterLength,         // 104 'h'
    LetterFormatInt,      // 105 'i'
    LetterLength,         // 106 'j'
    Other,                // 107 'k'
    LetterLength,         // 108 'l'
    Other,                // 109 'm'
    Other,                // 110 'n'
    LetterFormatUnsigned, // 111 'o'
    LetterFormatPointer,  // 112 'p'
    Other,                // 113 'q'
    Other,                // 114 'r'
    LetterFormatString,   // 115 's'
    LetterLength,         // 116 't'
    LetterFormatUnsigned, // 117 'u'
    Other,                // 118 'v'
    Other,                // 119 'w'
    LetterFormatUnsigned, // 120 'x'
    Other,                // 121 'y'
    LetterLength,         // 122 'z'
    Other,                // 123 '{'
    Other,                // 124 '|'
    Other,                // 125 '}'
    Other,                // 126 '~'
    Other,                // 127 ''
    Other,                // 128 ' '
    Other,                // 129 ' '
    Other,                // 130 ' '
    Other,                // 131 ' '
    Other,                // 132 ' '
    Other,                // 133 ' '
    Other,                // 134 ' '
    Other,                // 135 ' '
    Other,                // 136 ' '
    Other,                // 137 ' '
    Other,                // 138 ' '
    Other,                // 139 ' '
    Other,                // 140 ' '
    Other,                // 141 ' '
    Other,                // 142 ' '
    Other,                // 143 ' '
    Other,                // 144 ' '
    Other,                // 145 ' '
    Other,                // 146 ' '
    Other,                // 147 ' '
    Other,                // 148 ' '
    Other,                // 149 ' '
    Other,                // 150 ' '
    Other,                // 151 ' '
    Other,                // 152 ' '
    Other,                // 153 ' '
    Other,                // 154 ' '
    Other,                // 155 ' '
    Other,                // 156 ' '
    Other,                // 157 ' '
    Other,                // 158 ' '
    Other,                // 159 ' '
    Other,                // 160 ' '
    Other,                // 161 ' '
    Other,                // 162 ' '
    Other,                // 163 ' '
    Other,                // 164 ' '
    Other,                // 165 ' '
    Other,                // 166 ' '
    Other,                // 167 ' '
    Other,                // 168 ' '
    Other,                // 169 ' '
    Other,                // 170 ' '
    Other,                // 171 ' '
    Other,                // 172 ' '
    Other,                // 173 ' '
    Other,                // 174 ' '
    Other,                // 175 ' '
    Other,                // 176 ' '
    Other,                // 177 ' '
    Other,                // 178 ' '
    Other,                // 179 ' '
    Other,                // 180 ' '
    Other,                // 181 ' '
    Other,                // 182 ' '
    Other,                // 183 ' '
    Other,                // 184 ' '
    Other,                // 185 ' '
    Other,                // 186 ' '
    Other,                // 187 ' '
    Other,                // 188 ' '
    Other,                // 189 ' '
    Other,                // 190 ' '
    Other,                // 191 ' '
    Other,                // 192 ' '
    Other,                // 193 ' '
    Other,                // 194 ' '
    Other,                // 195 ' '
    Other,                // 196 ' '
    Other,                // 197 ' '
    Other,                // 198 ' '
    Other,                // 199 ' '
    Other,                // 200 ' '
    Other,                // 201 ' '
    Other,                // 202 ' '
    Other,                // 203 ' '
    Other,                // 204 ' '
    Other,                // 205 ' '
    Other,                // 206 ' '
    Other,                // 207 ' '
    Other,                // 208 ' '
    Other,                // 209 ' '
    Other,                // 210 ' '
    Other,                // 211 ' '
    Other,                // 212 ' '
    Other,                // 213 ' '
    Other,                // 214 ' '
    Other,                // 215 ' '
    Other,                // 216 ' '
    Other,                // 217 ' '
    Other,                // 218 ' '
    Other,                // 219 ' '
    Other,                // 220 ' '
    Other,                // 221 ' '
    Other,                // 222 ' '
    Other,                // 223 ' '
    Other,                // 224 ' '
    Other,                // 225 ' '
    Other,                // 226 ' '
    Other,                // 227 ' '
    Other,                // 228 ' '
    Other,                // 229 ' '
    Other,                // 230 ' '
    Other,                // 231 ' '
    Other,                // 232 ' '
    Other,                // 233 ' '
    Other,                // 234 ' '
    Other,                // 235 ' '
    Other,                // 236 ' '
    Other,                // 237 ' '
    Other,                // 238 ' '
    Other,                // 239 ' '
    Other,                // 240 ' '
    Other,                // 241 ' '
    Other,                // 242 ' '
    Other,                // 243 ' '
    Other,                // 244 ' '
    Other,                // 245 ' '
    Other,                // 246 ' '
    Other,                // 247 ' '
    Other,                // 248 ' '
    Other,                // 249 ' '
    Other,                // 250 ' '
    Other,                // 251 ' '
    Other,                // 252 ' '
    Other,                // 253 ' '
    Other,                // 254 ' '
    Other,                // 255 ' '
};
