/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_TEST_UNIT_TEST_DECODER_H_
#define VN_API_TEST_UNIT_TEST_DECODER_H_

// Define this so that we can reach both the internal headers (which are NOT in a dll) AND some
// handy functions from the actual interface of the API (which normally WOULD be in a dll).
#define VNDisablePublicAPI

#include <LCEVC/lcevc_dec.h>

#include <array>
#include <cstddef>
#include <vector>

static const std::vector<int32_t> kAllEvents = {
    LCEVC_Log,
    LCEVC_Exit,
    LCEVC_CanSendBase,
    LCEVC_CanSendEnhancement,
    LCEVC_CanSendPicture,
    LCEVC_CanReceive,
    LCEVC_BasePictureDone,
    LCEVC_OutputPictureDone,
};
static const int32_t kResultsQueueCap = 24;
static const int32_t kUnprocessedCap = 100;

// These were generated by encoding the "footballer" yuv with the ERP, then printing to command
// line (in the format of a c++ intializer list).
static const size_t kArbitraryEnhancementSizes[3] = {253, 308, 80};
static const std::vector<uint8_t> kEnhancements[3] = {
    {
        0,   0,   1,   123, 255, 229, 7,   0,   4,   180, 0,   80,  0,   1,   64,  4,   64,
        225, 13,  53,  67,  224, 128, 31,  6,   216, 57,  80,  15,  209, 2,   73,  226, 20,
        50,  12,  37,  10,  13,  10,  12,  26,  1,   26,  2,   14,  36,  0,   1,   30,  12,
        12,  4,   66,  227, 129, 62,  85,  85,  85,  85,  85,  119, 217, 85,  24,  192, 129,
        234, 52,  194, 7,   194, 129, 210, 82,  194, 157, 110, 190, 52,  188, 157, 18,  188,
        60,  188, 131, 251, 59,  8,   192, 131, 249, 3,   194, 131, 251, 59,  11,  192, 131,
        179, 16,  194, 151, 30,  194, 132, 170, 15,  129, 6,   17,  138, 60,  39,  201, 8,
        162, 43,  169, 120, 175, 134, 4,   194, 24,  143, 255, 33,  169, 166, 187, 48,  166,
        6,   54,  97,  176, 193, 128, 1,   53,  107, 181, 0,   0,   192, 0,   48,  0,   219,
        48,  48,  20,  178, 80,  192, 12,  0,   0,   3,   0,   3,   0,   0,   3,   0,   0,
        3,   0,   0,   3,   0,   0,   3,   0,   0,   62,  216, 176, 87,  191, 74,  67,  210,
        34,  100, 69,  235, 217, 156, 136, 136, 141, 86,  59,  124, 27,  123, 229, 187, 121,
        254, 115, 124, 190, 119, 235, 217, 68,  85,  26,  210, 80,  207, 45,  24,  54,  152,
        7,   104, 78,  21,  4,   164, 7,   41,  211, 61,  232, 30,  102, 115, 14,  16,  71,
        202, 9,   57,  19,  135, 212, 134, 138, 53,  110, 53,  245, 105, 48,  128,
    },
    {
        0,   0,   1,   121, 255, 130, 0,   11,  57,  66,  227, 130, 34,  85,  85,  85,  85,  85,
        85,  89,  85,  192, 129, 78,  17,  207, 58,  103, 140, 248, 161, 4,   70,  140, 146, 37,
        211, 188, 151, 203, 2,   225, 12,  75,  140, 178, 39,  254, 51,  209, 53,  148, 171, 204,
        12,  83,  1,   152, 192, 1,   134, 2,   203, 187, 201, 177, 89,  118, 187, 22,  181, 128,
        0,   0,   5,   150, 5,   128, 1,   11,  44,  181, 128, 44,  0,   0,   176, 0,   0,   3,
        0,   0,   3,   0,   0,   3,   0,   0,   3,   0,   11,  11,  0,   90,  11,  192, 26,  244,
        23,  192, 215, 158, 169, 168, 53,  116, 94,  119, 219, 129, 175, 98,  90,  224, 182, 72,
        51,  4,   195, 181, 62,  215, 82,  231, 90,  53,  100, 165, 117, 89,  101, 217, 69,  161,
        8,   184, 88,  161, 8,   184, 44,  89,  4,   36,  99,  24,  86,  33,  8,   99,  99,  10,
        219, 67,  90,  113, 13,  0,   230, 87,  128, 198, 201, 136, 64,  35,  99,  213, 94,  245,
        221, 117, 85,  93,  89,  94,  173, 228, 234, 223, 199, 147, 64,  181, 135, 88,  148, 117,
        120, 32,  134, 19,  26,  195, 2,   117, 16,  214, 207, 105, 46,  117, 101, 146, 170, 187,
        190, 117, 95,  85,  60,  82,  109, 165, 54,  150, 219, 109, 180, 182, 153, 22,  112, 128,
        72,  0,   132, 11,  1,   130, 79,  1,   130, 60,  1,   131, 168, 103, 2,   64,  1,   151,
        0,   2,   136, 105, 1,   62,  1,   59,  1,   64,  1,   1,   1,   131, 48,  3,   64,  1,
        2,   1,   8,   1,   51,  4,   1,   1,   1,   2,   57,  1,   4,   3,   1,   2,   129, 124,
        1,   150, 105, 1,   58,  7,   129, 124, 1,   1,   1,   131, 58,  2,   61,  1,   131, 249,
        119, 128,
    },
    {
        0,   0,   1,   121, 255, 130, 0,   19,  13,  66,  227, 67,  85,  85,  85,  85,
        85,  85,  89,  85,  192, 28,  8,   197, 62,  144, 139, 235, 2,   194, 191, 241,
        140, 129, 113, 184, 224, 222, 131, 66,  44,  152, 5,   48,  254, 215, 236, 130,
        141, 236, 28,  0,   131, 183, 112, 1,   133, 31,  1,   132, 111, 1,   48,  4,
        133, 71,  1,   137, 113, 1,   133, 18,  1,   136, 93,  1,   132, 147, 98,  128,
    },
};

#endif // VN_API_TEST_UNIT_TEST_DECODER_H_