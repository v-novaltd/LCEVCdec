/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "predict_timehandle.h"

#include "lcevc_container.h"

#include <inttypes.h>
#include <stdlib.h>

// - Constants ------------------------------------------------------------------------------------

/// For now, this is a private constant. However, we may want to make it variable. In the lcevc
/// decoder utility library, uBaseDecoder.h mentions m_maxNumReorderFrames, which maybe could be
/// used to set this.
static const uint16_t kDeltaJumpCoefficient = 32;

/// Ideally, anything below 50 should be fine, since delta will be halved everytime (if updated).
static const uint16_t kPercentError = 25; // 25%

static const uint16_t kDefaultMaxNumReorderFrames = 16;

// - TimehandlePredictor --------------------------------------------------------------------------

/// This struct, and the assiciated functions, work out the gap between timehandles on the fly, and
/// use this to reorder LCEVC data. If you feed the timehandles in decode order (DTS order), this
/// struct will store the correct gap between timehandles. The associated functions will then use
/// this data to accept/reject timehandles.
/// TODO: make this code threadsafe
/// TODO: take "max reorder count" as an input somehow
typedef struct TimehandlePredictor
{
    /// Stores last fed timehandle. Used to calculate delta between decode timehandle
    /// values
    uint64_t lastFedTimehandle;
    /// Stores last hinted timehandle. Used to see whether queried pts can be next
    /// or not
    uint64_t lastHintedTimehandle;

    /// Limits for how far one PTS can be from the last one, and still count as "next". If it's
    /// too far in the future, it's not next because there's one in between. If it's too close,
    /// it might be a duplicate frame, or an error.
    /// \see kPercentError
    uint64_t deltaLowerBound;
    uint64_t deltaUpperBound;

    /// Will count how many times calculated `delta` is the same. Once it hits zero, it's stable
    uint32_t deltaRepeatCount;

    /// A function pointer to allow clients to decide how to print timehandles. The default printer
    /// simply returns the timehandle as an unsigned 64bit decimal number.
    thPrinter timehandlePrintFn;

    /// The maximum number of frames that can be fed out of order before you have a contiguous
    /// block of frames. This should be set once, at creation time, and never reset.
    uint32_t maxNumReorderFrames;
} TimehandlePredictor_t;

// Internal functions

static const char* defaultPrinter(char* dest, size_t destSize, uint64_t timehandle)
{
    snprintf(dest, destSize, "%" PRIu64 "", timehandle);
    return dest;
}

static void timehandlePredictorReset(TimehandlePredictor_t* predictor)
{
    predictor->lastFedTimehandle = kInvalidTimehandle;
    predictor->lastHintedTimehandle = kInvalidTimehandle;
    predictor->deltaLowerBound = 0;
    predictor->deltaUpperBound = 0;
    predictor->deltaRepeatCount = predictor->maxNumReorderFrames / 2;
}

static void timehandlePredictorUpdateDelta(TimehandlePredictor_t* predictor, uint64_t delta)
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

TimehandlePredictor_t* timehandlePredictorCreate()
{
    TimehandlePredictor_t* out = calloc(1, sizeof(TimehandlePredictor_t));
    out->timehandlePrintFn = defaultPrinter;
    out->maxNumReorderFrames = kDefaultMaxNumReorderFrames;
    timehandlePredictorReset(out);
    return out;
}

void timehandlePredictorDestroy(TimehandlePredictor_t* predictor) { free(predictor); }

void timehandlePredictorFeed(TimehandlePredictor_t* predictor, uint64_t timehandle)
{
    // note that these are uints so you can't just do abs(x-y) without risking bad conversions.
    const uint64_t largerTH =
        (timehandle > predictor->lastFedTimehandle ? timehandle : predictor->lastFedTimehandle);
    const uint64_t smallerTH =
        (timehandle > predictor->lastFedTimehandle ? predictor->lastFedTimehandle : timehandle);
    const uint64_t newDelta = largerTH - smallerTH;

    char th1Buf[32];
    char th2Buf[32];
    VN_SEQ_DEBUG("Feeding (%s) = last (%s) +/- %" PRIu64 ". old delta: (%" PRIu64 "-%" PRIu64 ")\n",
                 timehandlePredictorPrintTimehandle(predictor, th1Buf, 32, timehandle),
                 timehandlePredictorPrintTimehandle(predictor, th2Buf, 32, predictor->lastFedTimehandle),
                 newDelta, predictor->deltaLowerBound, predictor->deltaUpperBound);

    if (predictor->lastFedTimehandle != kInvalidTimehandle) {
        // FIXME: Next line should be using (m_maxNumReorder+1) instead of kDeltaJumpCoefficient,
        // but we don't have that information.
        if (predictor->deltaUpperBound &&
            (newDelta > (predictor->deltaUpperBound * kDeltaJumpCoefficient))) {
            // We had a big jump, so better reset everything
            VN_SEQ_WARNING("Detecting big jump. old delta: (%" PRIu64 "-%" PRIu64 ")\n",
                           predictor->deltaLowerBound, predictor->deltaUpperBound);
            timehandlePredictorReset(predictor);
        } else {
            timehandlePredictorUpdateDelta(predictor, newDelta);
        }
    }
    predictor->lastFedTimehandle = timehandle;

    // First timestamp in the stream, so use it to initialise m_lastHintedTimehandle
    if (predictor->lastHintedTimehandle == kInvalidTimehandle) {
        predictor->lastHintedTimehandle = timehandle;
    }
}

void timehandlePredictorHint(TimehandlePredictor_t* predictor, uint64_t timehandle)
{
    // note that these are uints so you can't just do abs(x-y) without risking bad conversions.
    const uint64_t largerTH =
        (timehandle > predictor->lastHintedTimehandle ? timehandle : predictor->lastHintedTimehandle);
    const uint64_t smallerTH =
        (timehandle > predictor->lastHintedTimehandle ? predictor->lastHintedTimehandle : timehandle);
    const uint64_t accurateDelta = largerTH - smallerTH;

    char th1Buf[32];
    char th2Buf[32];
    VN_SEQ_DEBUG("Hinting (%s) = last (%s + %" PRIu64 ")\n",
                 timehandlePredictorPrintTimehandle(predictor, th1Buf, 32, timehandle),
                 timehandlePredictorPrintTimehandle(predictor, th2Buf, 32, predictor->lastHintedTimehandle),
                 accurateDelta);

    if (predictor->lastHintedTimehandle == kInvalidTimehandle) {
        VN_SEQ_WARNING("hint called when no timehandles have been fed\n");
        return;
    }

    if (timehandle < predictor->lastHintedTimehandle) {
        // This doesn't happen with simple stream, so this means we had a backward jump but
        // failed to detect it by fed values, so just to be safe, we reset everything
        VN_SEQ_WARNING("Detecting backward jump. old delta: (%" PRIu64 "-%" PRIu64 ")\n",
                       predictor->deltaLowerBound, predictor->deltaUpperBound);
        timehandlePredictorReset(predictor);
    } else {
        // Since `hint` is called in presentation order, we're more sure about this delta, so
        // let's update. BUT practically, when we reach here, we should have already figured
        // out the delta, so this will just speed-up converging
        timehandlePredictorUpdateDelta(predictor, accurateDelta);
    }
    predictor->lastHintedTimehandle = timehandle;
}

bool timehandlePredictorIsNext(const TimehandlePredictor_t* predictor, uint64_t timehandle)
{
    if (predictor->deltaRepeatCount) {
        // We're not sure yet, so let's not jump to any conclusions
        return false;
    }

    if (timehandle == predictor->lastHintedTimehandle) {
        // This is typically the first in the stream
        return true;
    }

    if (timehandle < predictor->lastHintedTimehandle) {
        // This is a jump backward, we can't make any guarantees about such case
        return false;
    }

    // should NOT be abs (for example, if deltaLowerBound is 0, we want to spot and reject when the
    // timehandles go backwards)
    int64_t delta = (int64_t)(timehandle) - (int64_t)(predictor->lastHintedTimehandle);

    return (predictor->deltaLowerBound <= delta) && (delta <= predictor->deltaUpperBound);
}

void timehandlePredictorSetMaxNumReorderFrames(TimehandlePredictor_t* predictor, uint64_t maxNumReorderFrames)

{
    if (maxNumReorderFrames == 0) {
        maxNumReorderFrames = kDefaultMaxNumReorderFrames;
    }

    predictor->maxNumReorderFrames = maxNumReorderFrames;
    timehandlePredictorReset(predictor);
}

void timehandlePredictorSetPrinter(TimehandlePredictor_t* predictor, thPrinter printer)
{
    if (printer == NULL) {
        printer = defaultPrinter;
    }
    predictor->timehandlePrintFn = printer;
}

const char* timehandlePredictorPrintTimehandle(const TimehandlePredictor_t* predictor, char* dest,
                                               size_t destSize, uint64_t timehandle)
{
    return predictor->timehandlePrintFn(dest, destSize, timehandle);
}
