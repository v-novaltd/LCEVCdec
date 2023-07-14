/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
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

TimehandlePredictor_t* timehandlePredictorCreate();
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
                                               uint64_t maxNumReorderFrames);

/// Sets the timehandle predictor's timehandle printer (the function it will use when logging
/// timehandles).
void timehandlePredictorSetPrinter(TimehandlePredictor_t* predictor, thPrinter printer);

/// Uses the timehandle predictor's timehandle printer to populate a char* for client use.
const char* timehandlePredictorPrintTimehandle(const TimehandlePredictor_t* predictor, char* dest,
                                               size_t destSize, uint64_t timehandle);

#endif // VN_CORE_SEQUENCING_PREDICT_TIMEHANDLE_H_
