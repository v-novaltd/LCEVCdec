/* Copyright (c) V-Nova International Limited 2023-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#ifndef VN_CORE_SEQUENCING_PREDICT_TIMEHANDLE_H_
#define VN_CORE_SEQUENCING_PREDICT_TIMEHANDLE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// - Throwaway logging code -----------------------------------------------------------------------
// This will be eliminated when https://vnovaservices.atlassian.net/browse/DEC-219 is done.

typedef enum LogType
{
    LTError,
    LTInfo,
    LTWarning,
    LTDebug,
    LTVerbose,
    LTUnknown
} LogType;

static const LogType sLogLevel = LTError;

#define VN_SEQ_LOG_OUTPUT(type, msg, ...) \
    if (type <= sLogLevel)                \
        printf(msg, ##__VA_ARGS__);

#define VN_SEQ_LOG(type, msg, ...) VN_SEQ_LOG_OUTPUT(type, msg, ##__VA_ARGS__)
#define VN_SEQ_VERBOSE(msg, ...) VN_SEQ_LOG_OUTPUT(LTVerbose, msg, ##__VA_ARGS__)
#define VN_SEQ_DEBUG(msg, ...) VN_SEQ_LOG_OUTPUT(LTDebug, msg, ##__VA_ARGS__)
#define VN_SEQ_WARNING(msg, ...) VN_SEQ_LOG_OUTPUT(LTWarning, msg, ##__VA_ARGS__)
#define VN_SEQ_INFO(msg, ...) VN_SEQ_LOG_OUTPUT(LTInfo, msg, ##__VA_ARGS__)
#define VN_SEQ_ERROR(msg, ...) VN_SEQ_LOG_OUTPUT(LTError, msg, ##__VA_ARGS__)

// - Constants ------------------------------------------------------------------------------------

static const uint64_t kInvalidTimehandle = UINT64_MAX;

// - Struct and functions: TimehandlePredictor ----------------------------------------------------

typedef struct TimehandlePredictor TimehandlePredictor_t;

typedef const char* (*thPrinter)(char* dest, size_t destSize, uint64_t timehandle);

TimehandlePredictor_t* timehandlePredictorCreate(void);
void timehandlePredictorDestroy(TimehandlePredictor_t* predictor);

/// Feed timehandles in DECODE order. In other words, these are presentation-timehandles
/// (PTS+input_cc), but they're not in presentation order, they're in decode-timehandle
/// (DTS+input_cc) order.
void timehandlePredictorFeed(TimehandlePredictor_t* predictor, uint64_t timehandle);

/// Hint timehandles in PRESENTATION order (for example, by storing them in a sorted
/// container, and popping entries off the front of the list). This is typically done
/// either when Decode is called, or when you decide to decode a given timehandle
/// regardless of order.
/// \note: call this method before calling isNext, if you want isNext to work.
void timehandlePredictorHint(TimehandlePredictor_t* predictor, uint64_t timehandle);

/// Will predict whether the given timehandle can be the next in the stream or not.
/// \pre: previous timehandle should have been hinted in order for this to work.
bool timehandlePredictorIsNext(const TimehandlePredictor_t* predictor, uint64_t timehandle);

/// Sets the maxNumReorderFrames (that is, the maximum number of frames that you expect to feed in
/// out-of-order, before you've sent 1 full contiguous block of frames). Setting this resets the
/// predictor, since it invalidates the prediction algorithm for the existing timehandles.
void timehandlePredictorSetMaxNumReorderFrames(TimehandlePredictor_t* predictor,
                                               uint32_t maxNumReorderFrames);

/// Sets the timehandle predictor's timehandle printer (the function it will use when logging
/// timehandles).
void timehandlePredictorSetPrinter(TimehandlePredictor_t* predictor, thPrinter printer);

/// Uses the timehandle predictor's timehandle printer to populate a char* for client use.
const char* timehandlePredictorPrintTimehandle(const TimehandlePredictor_t* predictor, char* dest,
                                               size_t destSize, uint64_t timehandle);

#endif // VN_CORE_SEQUENCING_PREDICT_TIMEHANDLE_H_
