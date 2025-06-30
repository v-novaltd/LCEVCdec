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

#include "predict_timestamp.h"

#include <inttypes.h>
#include <LCEVC/sequencer/lcevc_container.h>
#include <stdlib.h>

// - Constants ------------------------------------------------------------------------------------

/// For now, this is a private constant. However, we may want to make it variable. In the lcevc
/// decoder utility library, uBaseDecoder.h mentions m_maxNumReorderFrames, which maybe could be
/// used to set this.
static const uint16_t kDeltaJumpCoefficient = 32;

/// Ideally, anything below 50 should be fine, since delta will be halved everytime (if updated).
static const uint16_t kPercentError = 25; // 25%

static const uint16_t kDefaultMaxNumReorderFrames = 16;

// - TimestampPredictor ---------------------------------------------------------------------------

/// This struct, and the associated functions, work out the gap between timestamps on the fly, and
/// use this to reorder LCEVC data. If you feed the timestamps in decode order (DTS order), this
/// struct will store the correct gap between timestamps. The associated functions will then use
/// this data to accept/reject timestamps.
/// TODO: make this code threadsafe
/// TODO: take "max reorder count" as an input somehow
typedef struct TimestampPredictor
{
    /// Stores last fed timestamp. Used to calculate delta between decode timestamp
    /// values
    uint64_t lastFedTimestamp;
    /// Stores last hinted timestamp. Used to see whether queried pts can be next
    /// or not
    uint64_t lastHintedTimestamp;

    /// Limits for how far one PTS can be from the last one, and still count as "next". If it's
    /// too far in the future, it's not next because there's one in between. If it's too close,
    /// it might be a duplicate frame, or an error.
    /// \see kPercentError
    uint64_t deltaLowerBound;
    uint64_t deltaUpperBound;

    /// Will count how many times calculated `delta` is the same. Once it hits zero, it's stable
    uint32_t deltaRepeatCount;

    /// A function pointer to allow clients to decide how to print timestamps. The default printer
    /// simply returns the timestamp as an unsigned 64bit decimal number.
    tsPrinter timestampPrintFn;

    /// The maximum number of frames that can be fed out of order before you have a contiguous
    /// block of frames. This should be set once, at creation time, and never reset.
    uint32_t maxNumReorderFrames;
} TimestampPredictor_t;

// Internal functions

static const char* defaultPrinter(char* dest, size_t destSize, uint64_t timestamp)
{
    snprintf(dest, destSize, "%" PRIu64 "", timestamp);
    return dest;
}

static void timestampPredictorReset(TimestampPredictor_t* predictor)
{
    predictor->lastFedTimestamp = kInvalidTimestamp;
    predictor->lastHintedTimestamp = kInvalidTimestamp;
    predictor->deltaLowerBound = 0;
    predictor->deltaUpperBound = 0;
    predictor->deltaRepeatCount = predictor->maxNumReorderFrames / 2;
}

static void timestampPredictorUpdateDelta(TimestampPredictor_t* predictor, uint64_t delta)
{
    if (delta == 0) {
        return;
    }

    if (predictor->deltaLowerBound == 0 || delta < predictor->deltaLowerBound) {
        uint64_t errorMargin = (delta * kPercentError) / 100;
        predictor->deltaLowerBound = delta - errorMargin;
        predictor->deltaUpperBound = delta + errorMargin;
        predictor->deltaRepeatCount = predictor->maxNumReorderFrames / 2;
        VN_SEQ_DEBUG("Delta updated. delta: %" PRIu64 "(%" PRIu64 "-%" PRIu64 ")\n", delta,
                     predictor->deltaLowerBound, predictor->deltaUpperBound);

    } else if (predictor->deltaRepeatCount > 0) {
        // New delta is equal-to-or-greater-than current one, so high chance that this is it.
        predictor->deltaRepeatCount--;
    }
}

// External functions

TimestampPredictor_t* timestampPredictorCreate(void)
{
    TimestampPredictor_t* out = calloc(1, sizeof(TimestampPredictor_t));
    out->timestampPrintFn = defaultPrinter;
    out->maxNumReorderFrames = kDefaultMaxNumReorderFrames;
    timestampPredictorReset(out);
    return out;
}

void timestampPredictorDestroy(TimestampPredictor_t* predictor) { free(predictor); }

void timestampPredictorFeed(TimestampPredictor_t* predictor, uint64_t timestamp)
{
    // do nothing if its the same as last time
    if (predictor->lastFedTimestamp == timestamp) {
        return;
    }

    // note that these are uints so you can't just do abs(x-y) without risking bad conversions.
    const uint64_t largerTH =
        (timestamp > predictor->lastFedTimestamp ? timestamp : predictor->lastFedTimestamp);
    const uint64_t smallerTH =
        (timestamp > predictor->lastFedTimestamp ? predictor->lastFedTimestamp : timestamp);
    const uint64_t newDelta = largerTH - smallerTH;

    // Don't do any of the work, or declare things on the stack if we are not going to do te debug
    if (LTDebug <= sLogLevel) {
        char th1Buf[32];
        char th2Buf[32];
        VN_SEQ_DEBUG("Feeding (%s) = last (%s) +/- %" PRIu64 ". old delta: (%" PRIu64 "-%" PRIu64 ")\n",
                     timestampPredictorPrintTimestamp(predictor, th1Buf, 32, timestamp),
                     timestampPredictorPrintTimestamp(predictor, th2Buf, 32, predictor->lastFedTimestamp),
                     newDelta, predictor->deltaLowerBound, predictor->deltaUpperBound);
    }

    if (predictor->lastFedTimestamp != kInvalidTimestamp) {
        // FIXME: Next line should be using (m_maxNumReorder+1) instead of kDeltaJumpCoefficient,
        // but we don't have that information.
        if (predictor->deltaUpperBound &&
            (newDelta > (predictor->deltaUpperBound * kDeltaJumpCoefficient))) {
            // We had a big jump, so better reset everything
            VN_SEQ_WARNING("Detecting big jump. old delta: (%" PRIu64 "-%" PRIu64 ")\n",
                           predictor->deltaLowerBound, predictor->deltaUpperBound);
            timestampPredictorReset(predictor);
        } else {
            timestampPredictorUpdateDelta(predictor, newDelta);
        }
    }
    // reset the counter if the PTS seems to behaving in an odd way.
    // we expect them to be out-of order which should mean that this test should never pass
    // unless we are being fed hi -> low
    if (predictor->deltaRepeatCount && (predictor->lastFedTimestamp > timestamp) &&
        (predictor->lastHintedTimestamp > timestamp)) {
        predictor->deltaRepeatCount = predictor->maxNumReorderFrames / 2;
    }
    predictor->lastFedTimestamp = timestamp;

    // First timestamp in the stream, so use it to initialise m_lastHintedTimestamp
    if (predictor->lastHintedTimestamp == kInvalidTimestamp) {
        predictor->lastHintedTimestamp = timestamp;
    }
}

void timestampPredictorHint(TimestampPredictor_t* predictor, uint64_t timestamp)
{
    // No need to do anything if it's the same as last time
    if (predictor->lastHintedTimestamp == timestamp) {
        return;
    }
    // note that these are uints so you can't just do abs(x-y) without risking bad conversions.
    const uint64_t largerTH =
        (timestamp > predictor->lastHintedTimestamp ? timestamp : predictor->lastHintedTimestamp);
    const uint64_t smallerTH =
        (timestamp > predictor->lastHintedTimestamp ? predictor->lastHintedTimestamp : timestamp);
    const uint64_t accurateDelta = largerTH - smallerTH;

    // Don't do any of the work, or declare things on the stack if we are not going to do te debug
    if (LTDebug <= sLogLevel) {
        char th1Buf[32];
        char th2Buf[32];
        VN_SEQ_DEBUG("Hinting (%s) = last (%s + %" PRIu64 ")\n",
                     timestampPredictorPrintTimestamp(predictor, th1Buf, 32, timestamp),
                     timestampPredictorPrintTimestamp(predictor, th2Buf, 32, predictor->lastHintedTimestamp),
                     accurateDelta);
    }

    if (predictor->lastHintedTimestamp == kInvalidTimestamp) {
        VN_SEQ_WARNING("hint called when no timestamps have been fed\n");
        return;
    }

    if (timestamp < predictor->lastHintedTimestamp) {
        // This doesn't happen with simple stream, so this means we had a backward jump but
        // failed to detect it by fed values, so just to be safe, we reset everything
        VN_SEQ_WARNING("Detecting backward jump. old delta: (%" PRIu64 "-%" PRIu64 ")\n",
                       predictor->deltaLowerBound, predictor->deltaUpperBound);
        timestampPredictorReset(predictor);
    } else {
        // Since `hint` is called in presentation order, we're more sure about this delta, so
        // let's update. BUT practically, when we reach here, we should have already figured
        // out the delta, so this will just speed-up converging
        timestampPredictorUpdateDelta(predictor, accurateDelta);
    }
    predictor->lastHintedTimestamp = timestamp;
}

bool timestampPredictorIsNext(const TimestampPredictor_t* predictor, uint64_t timestamp)
{
    if (predictor->deltaRepeatCount) {
        // We're not sure yet, so let's not jump to any conclusions
        return false;
    }

    if (timestamp == predictor->lastHintedTimestamp) {
        // This is typically the first in the stream
        return true;
    }

    if (timestamp < predictor->lastHintedTimestamp) {
        // This is a jump backward, we can't make any guarantees about such case
        return false;
    }

    // should NOT be abs (for example, if deltaLowerBound is 0, we want to spot and reject when the
    // timestamps go backwards)
    int64_t delta = (int64_t)(timestamp) - (int64_t)(predictor->lastHintedTimestamp);
    if (delta < 0) {
        return false;
    }

    return (predictor->deltaLowerBound <= (uint64_t)delta) &&
           ((uint64_t)delta <= predictor->deltaUpperBound);
}

void timestampPredictorSetMaxNumReorderFrames(TimestampPredictor_t* predictor, uint32_t maxNumReorderFrames)

{
    if (maxNumReorderFrames == 0) {
        maxNumReorderFrames = kDefaultMaxNumReorderFrames;
    }

    predictor->maxNumReorderFrames = maxNumReorderFrames;
    timestampPredictorReset(predictor);
}

void timestampPredictorSetPrinter(TimestampPredictor_t* predictor, tsPrinter printer)
{
    if (printer == NULL) {
        printer = defaultPrinter;
    }
    predictor->timestampPrintFn = printer;
}

const char* timestampPredictorPrintTimestamp(const TimestampPredictor_t* predictor, char* dest,
                                             size_t destSize, uint64_t timestamp)
{
    return predictor->timestampPrintFn(dest, destSize, timestamp);
}
