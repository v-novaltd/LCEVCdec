/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_STATS_H_
#define VN_DEC_CORE_STATS_H_

#include "common/platform.h"

/*------------------------------------------------------------------------------*/

typedef struct Memory* Memory_t;
typedef struct Stats* Stats_t;
typedef struct FrameStats* FrameStats_t;
typedef struct Time* Time_t;

/*------------------------------------------------------------------------------*/

typedef enum StatType
{
    STFrameIndex = 0,
    STDecodeStart,
    STDecodeStop,
    STEntropyDecodeStart,
    STEntropyDecodeStop,
    STGenerateCommandBuffersStart,
    STGenerateCommandBuffersStop,
    STTileClearStart,
    STTileClearStop,
    STApplyInterLOQ0Start,
    STApplyInterLOQ0Stop,
    STApplyInterLOQ1Start,
    STApplyInterLOQ1Stop,
    STApplyIntraStart,
    STApplyIntraStop,
    STApplyTemporalBufferStart,
    STApplyTemporalBufferStop,
    STTileClearCount,
    STInterTransformCount,
    STIntraTransformCount,

    STLOQ0_TemporalByteSize,

    STLOQ0_LayerByteSize0,
    STLOQ0_LayerByteSize1,
    STLOQ0_LayerByteSize2,
    STLOQ0_LayerByteSize3,
    STLOQ0_LayerByteSize4,
    STLOQ0_LayerByteSize5,
    STLOQ0_LayerByteSize6,
    STLOQ0_LayerByteSize7,
    STLOQ0_LayerByteSize8,
    STLOQ0_LayerByteSize9,
    STLOQ0_LayerByteSize10,
    STLOQ0_LayerByteSize11,
    STLOQ0_LayerByteSize12,
    STLOQ0_LayerByteSize13,
    STLOQ0_LayerByteSize14,
    STLOQ0_LayerByteSize15,

    STLOQ1_LayerByteSize0,
    STLOQ1_LayerByteSize1,
    STLOQ1_LayerByteSize2,
    STLOQ1_LayerByteSize3,
    STLOQ1_LayerByteSize4,
    STLOQ1_LayerByteSize5,
    STLOQ1_LayerByteSize6,
    STLOQ1_LayerByteSize7,
    STLOQ1_LayerByteSize8,
    STLOQ1_LayerByteSize9,
    STLOQ1_LayerByteSize10,
    STLOQ1_LayerByteSize11,
    STLOQ1_LayerByteSize12,
    STLOQ1_LayerByteSize13,
    STLOQ1_LayerByteSize14,
    STLOQ1_LayerByteSize15,

    STStatsCount
} StatType_t;

typedef struct StatsConfig
{
    bool enabled;
    const char* outputPath;
    Time_t time;
} StatsConfig_t;

bool statsInitialize(Memory_t memory, Stats_t* stats, const StatsConfig_t* config);
void statsRelease(Stats_t stats);

FrameStats_t statsNewFrame(Stats_t stats);
void statsEndFrame(FrameStats_t frameStats);

void statsRecordValue(FrameStats_t stats, StatType_t type, uint64_t value);
void statsRecordTime(FrameStats_t stats, StatType_t type);

#define VN_FRAMESTATS_RECORD_FUNCTION(invocation, frameStats, type) \
    statsRecordTime(frameStats, VN_CONCAT(type, Start));            \
    invocation;                                                     \
    statsRecordTime(frameStats, VN_CONCAT(type, Stop))

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_STATS_H_ */