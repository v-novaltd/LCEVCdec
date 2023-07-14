/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */

#include "common/stats.h"

#include "common/memory.h"
#include "common/time.h"

#include <assert.h>

/*------------------------------------------------------------------------------*/

/* clang-format off */
static const char* const kStatTypeNames[STStatsCount] = {
    "Index",
    "LOQDecode Start",
    "LOQDecode Stop",
    "Entropy Decode Start",
    "Entropy Decode Stop",
    "Generate Command Buffers Start",
    "Generate Command Buffers Stop",
    "Tile Clear Start",
    "Tile Clear Stop",
    "Apply Inter LOQ-0 Start",
    "Apply Inter LOQ-0 Stop",
    "Apply Inter LOQ-1 Start",
    "Apply Inter LOQ-1 Stop",
    "Apply Intra Start",
    "Apply Intra Stop",
    "Apply Temporal Buffer Start",
    "Apply Temporal Buffer Stop",
    "Tile Clear Count",
    "Inter Transform Count",
    "Intra Transform Count",

    "LOQ-0 Temporal Byte Size",

    "LOQ-0 Layer 0 Byte Size",
    "LOQ-0 Layer 1 Byte Size",
    "LOQ-0 Layer 2 Byte Size",
    "LOQ-0 Layer 3 Byte Size",
    "LOQ-0 Layer 4 Byte Size",
    "LOQ-0 Layer 5 Byte Size",
    "LOQ-0 Layer 6 Byte Size",
    "LOQ-0 Layer 7 Byte Size",
    "LOQ-0 Layer 8 Byte Size",
    "LOQ-0 Layer 9 Byte Size",
    "LOQ-0 Layer 10 Byte Size",
    "LOQ-0 Layer 11 Byte Size",
    "LOQ-0 Layer 12 Byte Size",
    "LOQ-0 Layer 13 Byte Size",
    "LOQ-0 Layer 14 Byte Size",
    "LOQ-0 Layer 15 Byte Size",

    "LOQ-1 Layer 0 Byte Size",
    "LOQ-1 Layer 1 Byte Size",
    "LOQ-1 Layer 2 Byte Size",
    "LOQ-1 Layer 3 Byte Size",
    "LOQ-1 Layer 4 Byte Size",
    "LOQ-1 Layer 5 Byte Size",
    "LOQ-1 Layer 6 Byte Size",
    "LOQ-1 Layer 7 Byte Size",
    "LOQ-1 Layer 8 Byte Size",
    "LOQ-1 Layer 9 Byte Size",
    "LOQ-1 Layer 10 Byte Size",
    "LOQ-1 Layer 11 Byte Size",
    "LOQ-1 Layer 12 Byte Size",
    "LOQ-1 Layer 13 Byte Size",
    "LOQ-1 Layer 14 Byte Size",
    "LOQ-1 Layer 15 Byte Size",
};
/* clang-format on */

typedef struct StatsOutputFile
{
    FILE* file;
    bool outputFirstFrame;
} StatsOutputFile_t;

typedef struct Stats
{
    Memory_t memory;
    Time_t time;
    bool enabled;
    uint64_t currentIndex;
    FrameStats_t freeList;
    StatsOutputFile_t outputFile;
}* Stats_t;

typedef struct FrameStats
{
    Stats_t owner;
    uint64_t index;
    uint64_t values[STStatsCount];
    FrameStats_t nextFrameStats;
}* FrameStats_t;

/*------------------------------------------------------------------------------*/

static void statsDump(StatsOutputFile_t* file, const FrameStats_t frameStats)
{
    if (!file->file) {
        return;
    }

    if (!file->outputFirstFrame) {
        // Write CSV headers.
        for (int32_t i = 0; i < STStatsCount; ++i) {
            fprintf(file->file, "%s,", kStatTypeNames[i]);
        }
        fprintf(file->file, "\n");

        file->outputFirstFrame = true;
    }

    for (int32_t i = 0; i < STStatsCount; ++i) {
        fprintf(file->file, "%" PRIu64 ",", frameStats->values[i]);
    }
    fprintf(file->file, "\n");
}

bool statsInitialize(Memory_t memory, Stats_t* stats, const StatsConfig_t* config)
{
    Stats_t result = VN_CALLOC_T(memory, struct Stats);

    if (!result) {
        return false;
    }

    result->memory = memory;
    result->time = config->time;
    result->enabled = config->enabled;

    /* @todo: Remove all I/O from the codec and expose stats through an API instead. */
    if (config->outputPath) {
        StatsOutputFile_t* file = &result->outputFile;
        file->file = fopen(config->outputPath, "w");

        if (!file->file) {
            statsRelease(result);
            return false;
        }
    }

    *stats = result;
    return true;
}

void statsRelease(Stats_t stats)
{
    if (stats) {
        if (stats->outputFile.file) {
            fclose(stats->outputFile.file);
        }

        FrameStats_t frameStats = stats->freeList;

        while (frameStats) {
            FrameStats_t next = frameStats->nextFrameStats;
            VN_FREE(stats->memory, frameStats);
            frameStats = next;
        }

        VN_FREE(stats->memory, stats);
    }
}

FrameStats_t statsNewFrame(Stats_t stats)
{
    assert(stats != NULL);

    if (!stats->enabled) {
        return NULL;
    }

    FrameStats_t result = NULL;

    if (stats->freeList) {
        result = stats->freeList;
        stats->freeList = result->nextFrameStats;
    } else {
        result = VN_CALLOC_T(stats->memory, struct FrameStats);
    }

    memorySet(result, 0, sizeof(struct FrameStats));
    result->owner = stats;

    statsRecordValue(result, STFrameIndex, stats->currentIndex);
    stats->currentIndex++;

    return result;
}

void statsEndFrame(FrameStats_t frameStats)
{
    if (!frameStats) {
        return;
    }

    assert(frameStats->owner != NULL);

    Stats_t stats = frameStats->owner;
    statsDump(&stats->outputFile, frameStats);

    /* Store frame stats for re-use. */
    frameStats->nextFrameStats = stats->freeList;
    stats->freeList = frameStats;
}

void statsRecordValue(FrameStats_t stats, StatType_t type, uint64_t value)
{
    if (!stats) {
        return;
    }

    assert(stats->owner != NULL);
    assert(type >= 0 && type < STStatsCount);
    stats->values[type] += value;
}

void statsRecordTime(FrameStats_t stats, StatType_t type)
{
    if (!stats) {
        return;
    }

    assert(stats->owner != NULL);
    assert(type >= 0 && type < STStatsCount);
    stats->values[type] = timeNowNano(stats->owner->time);
}

/*------------------------------------------------------------------------------*/