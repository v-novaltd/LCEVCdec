/* Copyright (c) V-Nova International Limited 2024-2025. All rights reserved.
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

#include <LCEVC/common/log.h>
#include <LCEVC/common/memory.h>
#include <LCEVC/common/printf_macros.h>
#include <LCEVC/enhancement/config_parser.h>
#include <LCEVC/enhancement/config_pool.h>

static const size_t kInitialGlobalPoolSize = 2;

/* Wrap a GlobalConfig and it's reference count together
 */
typedef struct WrappedGlobalConfig
{
    LdeGlobalConfig globalConfig; //**< The global config
    uint32_t referenceCount;      //**< Number of outstanding references, including 'latest'
} WrappedGlobalConfig;

// Allocate a new wrapped GlobaConfig
//
static WrappedGlobalConfig* allocateGlobalConfig(LdeConfigPool* configPool, const LdeGlobalConfig* initial)
{
    LdcMemoryAllocation allocation = {0};
    VNAllocate(configPool->allocator, &allocation, WrappedGlobalConfig);
    ldcVectorAppend(&configPool->globalConfigs, &allocation);
    WrappedGlobalConfig* wrapped = VNAllocationPtr(allocation, WrappedGlobalConfig);
    memcpy(&wrapped->globalConfig, initial, sizeof(LdeGlobalConfig));
    wrapped->referenceCount = 1;
    return wrapped;
}

// Reduce reference count to a wrapped GlobalConfig
//
static void releaseGlobalConfig(LdeConfigPool* configPool, WrappedGlobalConfig* wrapped)
{
    assert(wrapped);
    assert(wrapped->referenceCount > 0);

    if (--wrapped->referenceCount != 0) {
        return;
    }

    // This is no longer used - find allocation
    LdcMemoryAllocation* alloc =
        ldcVectorFindUnordered(&configPool->globalConfigs, ldcVectorCompareAllocationPtr, wrapped);

    if (!alloc) {
        VNLogWarning("Allocation not found in pool.");
        return;
    }

    // Release block and remove from index
    VNFree(configPool->allocator, alloc);
    ldcVectorRemoveReorder(&configPool->globalConfigs, alloc);
}

void ldeConfigPoolInitialize(LdcMemoryAllocator* allocator, LdeConfigPool* configPool,
                             LdeBitstreamVersion bitstreamVersion)
{
    configPool->allocator = allocator;

    ldcVectorInitialize(&configPool->globalConfigs, sizeof(LdcMemoryAllocation),
                        (uint32_t)kInitialGlobalPoolSize, allocator);

    // Start with empty 'latest' global config
    // Explicitly clear whole of structure so that any padding is 0
    LdeGlobalConfig defaultGlobalConfig;
    VNClear(&defaultGlobalConfig);

    // Set to default values
    ldeGlobalConfigInitialize(bitstreamVersion, &defaultGlobalConfig);

    WrappedGlobalConfig* latest = allocateGlobalConfig(configPool, &defaultGlobalConfig);

    configPool->latestGlobalConfig = &latest->globalConfig;
    configPool->quantMatrix.set = false;
    configPool->ditherEnabled = false;
}

void ldeConfigPoolRelease(LdeConfigPool* configPool)
{
    for (uint32_t i = 0; i < ldcVectorSize(&configPool->globalConfigs); ++i) {
        VNFree(configPool->allocator, ldcVectorAt(&configPool->globalConfigs, i));
    }

    ldcVectorDestroy(&configPool->globalConfigs);
}

bool ldeConfigPoolFrameInsert(LdeConfigPool* configPool, uint64_t timestamp,
                              const uint8_t* serialized, size_t serializedSize,
                              LdeGlobalConfig** globalConfigPtr, LdeFrameConfig* frameConfig)
{
    ldeFrameConfigInitialize(configPool->allocator, frameConfig);

    // Read stateful params into the next frame config
    if (configPool->quantMatrix.set) {
        memcpy((void*)&frameConfig->quantMatrix, &configPool->quantMatrix, sizeof(LdeQuantMatrix));
    }
    frameConfig->ditherEnabled = configPool->ditherEnabled;

    // Make a copy of the current global config
    // Copy using memcpy to make sure zero padding is copied over
    bool globalConfigWritten = false;
    LdeGlobalConfig next;
    memcpy(&next, configPool->latestGlobalConfig, sizeof(LdeGlobalConfig));

    if (!ldeConfigsParse(serialized, serializedSize, &next, frameConfig, &globalConfigWritten)) {
        VNLogError("Could not parse frame 0x%" PRIx64, timestamp);
        return false;
    }

    // Save stateful params back to the pool
    configPool->quantMatrix = frameConfig->quantMatrix;
    configPool->ditherEnabled = frameConfig->ditherEnabled;

    // Has global config changed?
    // Use memcmp to compare - which is why any padding needs to be zero
    if (globalConfigWritten && memcmp(&next, configPool->latestGlobalConfig, sizeof(LdeGlobalConfig))) {
        // We have a new config

        WrappedGlobalConfig* newLatest = allocateGlobalConfig(configPool, &next);

        // Take away 'latest' reference from old latest
        releaseGlobalConfig(configPool, (WrappedGlobalConfig*)(configPool->latestGlobalConfig));

        // Update latest
        configPool->latestGlobalConfig = &newLatest->globalConfig;
    }

    // Increase reference count of latest
    ((WrappedGlobalConfig*)(configPool->latestGlobalConfig))->referenceCount++;

    *globalConfigPtr = configPool->latestGlobalConfig;
    return true;
}

bool ldeConfigPoolFrameRelease(LdeConfigPool* configPool, LdeFrameConfig* frameConfig,
                               LdeGlobalConfig* globalConfig)
{
    releaseGlobalConfig(configPool, (WrappedGlobalConfig*)globalConfig);
    return true;
}

void ldeConfigPoolFramePassthrough(LdeConfigPool* configPool, LdeGlobalConfig** globalConfigPtr,
                                   LdeFrameConfig* frameConfig)
{
    ldeFrameConfigInitialize(configPool->allocator, frameConfig);

    // Make a copy of the current global config
    // Copy using memcpy to make sure zero padding is copied over
    LdeGlobalConfig next;
    memcpy(&next, configPool->latestGlobalConfig, sizeof(LdeGlobalConfig));

    // Increase reference count of latest
    ((WrappedGlobalConfig*)(configPool->latestGlobalConfig))->referenceCount++;
    *globalConfigPtr = configPool->latestGlobalConfig;
}
