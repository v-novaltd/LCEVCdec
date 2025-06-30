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

#ifndef VN_LCEVC_LEGACY_PREDICT_TIMESTAMP_H
#define VN_LCEVC_LEGACY_PREDICT_TIMESTAMP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// - Throwaway logging code -----------------------------------------------------------------------
// This will be eliminated when https://vnovaservices.atlassian.net/browse/DEC-219 is done.

typedef enum LogType
{
    LTError,
    LTWarning,
    LTInfo,
    LTDebug,
    LTVerbose,
    LTUnknown
} LogType;

static const LogType sLogLevel = LTError;

#define VN_SEQ_LOG_OUTPUT(type, msg, ...) \
    if (type <= sLogLevel) {              \
        printf(msg, ##__VA_ARGS__);       \
    }

#define VN_SEQ_LOG(type, msg, ...) VN_SEQ_LOG_OUTPUT(type, msg, ##__VA_ARGS__)
#define VN_SEQ_VERBOSE(msg, ...) VN_SEQ_LOG_OUTPUT(LTVerbose, msg, ##__VA_ARGS__)
#define VN_SEQ_DEBUG(msg, ...) VN_SEQ_LOG_OUTPUT(LTDebug, msg, ##__VA_ARGS__)
#define VN_SEQ_WARNING(msg, ...) VN_SEQ_LOG_OUTPUT(LTWarning, msg, ##__VA_ARGS__)
#define VN_SEQ_INFO(msg, ...) VN_SEQ_LOG_OUTPUT(LTInfo, msg, ##__VA_ARGS__)
#define VN_SEQ_ERROR(msg, ...) VN_SEQ_LOG_OUTPUT(LTError, msg, ##__VA_ARGS__)

// - Constants ------------------------------------------------------------------------------------

static const uint64_t kInvalidTimestamp = UINT64_MAX;

// - Struct and functions: TimestampPredictor -----------------------------------------------------

typedef struct TimestampPredictor TimestampPredictor_t;

typedef const char* (*tsPrinter)(char* dest, size_t destSize, uint64_t timestamp);

TimestampPredictor_t* timestampPredictorCreate(void);
void timestampPredictorDestroy(TimestampPredictor_t* predictor);

/// Feed timestamps in DECODE order. In other words, these are presentation-timestamps
/// (PTS+input_cc), but they're not in presentation order, they're in decode-timestamp
/// (DTS+input_cc) order.
void timestampPredictorFeed(TimestampPredictor_t* predictor, uint64_t timestamp);

/// Hint timestamps in PRESENTATION order (for example, by storing them in a sorted
/// container, and popping entries off the front of the list). This is typically done
/// either when Decode is called, or when you decide to decode a given timestamp
/// regardless of order.
/// \note: call this method before calling isNext, if you want isNext to work.
void timestampPredictorHint(TimestampPredictor_t* predictor, uint64_t timestamp);

/// Will predict whether the given timestamp can be the next in the stream or not.
/// \pre: previous timestamp should have been hinted in order for this to work.
bool timestampPredictorIsNext(const TimestampPredictor_t* predictor, uint64_t timestamp);

/// Sets the maxNumReorderFrames (that is, the maximum number of frames that you expect to feed in
/// out-of-order, before you've sent 1 full contiguous block of frames). Setting this resets the
/// predictor, since it invalidates the prediction algorithm for the existing timestamps.
void timestampPredictorSetMaxNumReorderFrames(TimestampPredictor_t* predictor, uint32_t maxNumReorderFrames);

/// Sets the timestamp predictor's timestamp printer (the function it will use when logging
/// timestamps).
void timestampPredictorSetPrinter(TimestampPredictor_t* predictor, tsPrinter printer);

/// Uses the timestamp predictor's timestamp printer to populate a char* for client use.
const char* timestampPredictorPrintTimestamp(const TimestampPredictor_t* predictor, char* dest,
                                             size_t destSize, uint64_t timestamp);

#endif // VN_LCEVC_LEGACY_PREDICT_TIMESTAMP_H
