/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_CONTEXT_H_
#define VN_DEC_CORE_CONTEXT_H_

#include "LCEVC/PerseusDecoder.h"
#include "common/cmdbuffer.h"
#include "common/log.h"
#include "common/profiler.h"
#include "common/threading.h"
#include "common/types.h"
#include "decode/dequant.h"
#include "decode/deserialiser.h"
#include "surface/surface.h"

/*------------------------------------------------------------------------------*/

typedef struct Memory* Memory_t;
typedef struct Dither* Dither_t;
typedef struct Sharpen* Sharpen_t;
typedef struct Time* Time_t;
typedef struct DecodeSerial* DecodeSerial_t;
typedef struct DecodeParallel* DecodeParallel_t;
typedef struct Stats* Stats_t;

/*------------------------------------------------------------------------------*/

typedef struct Highlight
{
    bool enabled;
    uint16_t valUnsigned;
    int16_t valSigned;
} Highlight_t;

/*! \brief Prepares highlight state with a value that is scaled for the target bit-depth. */
void highlightSetValue(Highlight_t* state, BitDepth_t depth, uint16_t value);

/*! \brief Contains several surfaces used for decoding depending upon the decoder
 *         configuration */
typedef struct PlaneSurfaces
{
    Surface_t temporalBuffer[2]; /**< Internal temporal buffers for tracking temporal state, one entry per field.*/
    Surface_t temporalBufferU8; /**< S8 representation of the internal temporal buffer for S8 configuration. */
    Surface_t basePixels;   /**< Store LOQ-1 residuals when generate_surfaces is true.  */
    Surface_t basePixelsU8; /**< S8 representation of the basePixels for S8 configuration. */
    Surface_t externalSurfaces[LOQEnhancedCount]; /**< Stores handle to externally supplied pointers to store residuals into. */
    Surface_t internalSurfaces[LOQMaxCount]; /**< Higher fixed-point precision surfaces for "precision" pipeline mode. */
    Surface_t loq2UpsampleTarget; /**< Internal target to upscale from LOQ-2 to LOQ-1. */
} PlaneSurfaces_t;

/*! \brief Primary decoder "object", contains all decoding state - responsible for many sins. */
typedef struct Context
{
    /* The following members are globally accessible in any module, so must
     * always be available. */

    Memory_t memory;
    ThreadManager_t threadManager;
    Logger_t log;
    ProfilerState_t* profiler;
    Time_t time;
    Stats_t stats;

    /* The following members should be hidden from modules, only accessible in
     * the API layer - modules should take a handle as input if they're dependent
     * @todo(bob): Make this a reality. */
    DecodeSerial_t decodeSerial;
    DecodeParallel_t decodeParallel;
    Dither_t dither;
    Sharpen_t sharpen;

    DeserialisedData_t deserialised;
    PlaneSurfaces_t planes[RCMaxPlanes];
    Surface_t upscaleIntermediateSurface;

    lcevc_hdr_info hdrInfo;
    lcevc_vui_info vuiInfo;

    DequantParams_t dequant; /**< Dequantisation settings for all planes, LOQ, layers, and temporal signal */

    BitDepth_t inputDepth[LOQMaxCount];  /**< The bitdepth for the input at each LOQ. */
    BitDepth_t outputDepth[LOQMaxCount]; /**< The bitdepth for the output at each LOQ. */
    FixedPoint_t inputFP[LOQMaxCount];   /**< The fixedpoint type for the input at each LOQ. */
    FixedPoint_t outputFP[LOQMaxCount];  /**< The fixedpoint type for the output at each LOQ. */
    FixedPoint_t applyFP[LOQMaxCount]; /**< Convenience, the fixedpoint type used for applying residuals. */

    bool useExternalSurfaces;
    bool generateSurfaces;
    bool convertS8;
    bool disableTemporalApply;
    bool useApproximatePA;
    bool useLogoOverlay;
    bool useOldCodeLengths;

    uint16_t logoOverlayPositionX;
    uint16_t logoOverlayPositionY;
    uint16_t logoOverlayDelay;
    uint32_t logoOverlayCount;

    perseus_pipeline_mode pipelineMode;

    CPUAccelerationFeatures_t cpuFeatures;
    bool started;
    Highlight_t highlightState[LOQEnhancedCount];

    const char* debugConfigPath;
    const char* dumpPath;
    uint8_t dumpSurfaces;

    bool generateCmdBuffers; /**< When true this generates command buffers - it will not write to surfaces passed in. */

    SurfaceDumpCache_t* surfaceDumpCache; /**< Cache for surface dumpers. */
    uint64_t deserialiseCount; /**< Helper for debugging - number of times the deserialise function has successfully completed. */

    Kernel_t preBakedPAKernel;

    bool useParallelDecode;
} Context_t;

/*------------------------------------------------------------------------------*/

/*! \brief Configures the decoder surface formats based upon the signaled
 *         bit-depths and decoder settings.
 *
 * \param ctx The decoder to configure. */
void contextSetDepths(Context_t* ctx);

/*! \brief Initialise the PlaneSurfaces_t stored on the decoder instance to a default
 *         unused state.
 *
 *  \param ctx The decoder to configure. */
void contextPlaneSurfacesInitialise(Context_t* ctx);

/*! \brief Releases any allocated resources from the decoder instance.
 *
 *  \note The decoder lazily allocates resources as it needs them, so this function
 *        may perform varying amounts of work. */
void contextPlaneSurfacesRelease(Context_t* ctx);

/*! \brief Determines if the decoder is using internal surfaces for a given LOQ.
 *
 *  The decoder has a mode of operation where the input unsigned formats are stored
 *  and processed as higher precision signed fixed-point formats.
 *
 *  It does this by copying the input on the first API call to internally allocated
 *  surfaces. It then works on the internally allocated surfaces, ignoring any user
 *  input, for the during of the decoding process - it finally copies out (and converts)
 *  back to the usngined format on the last API call (high).
 *
 *  This function is a helper used to control the above mentioned behaviour.
 *
 * \param ctx The decoder instance.
 * \param loq The LOQ to check for internal surface usage.
 *
 * \return True if internal surfaces are being used for the given LOQ, otherwise
 *         false. */
bool contextLOQUsingInternalSurfaces(Context_t* ctx, LOQIndex_t loq);

/*! \brief Copies between the user surfaces and the internally allocated surfaces.
 *
 *  \param ctx The decoder instance
 *  \param src The source surfaces to copy from/to, this is restricted to luma &
 *             chroma planes for YUV.
 *  \param loq The LOQ to perform copying on.
 *  \param fromSrc Whether this function copies from src to internal surfaces, or
 *                 vice versa.
 *
 * \return 0 on success, otherwise -1 */
int32_t contextInternalSurfacesImageCopy(Context_t* ctx, Surface_t src[3], LOQIndex_t loq, bool fromSrc);

/*! \brief Prepares the temporal & convert surfaces for use during decoding.
 *
 *  This function may do nothing, it is only preparing the surfaces if certain
 *  settings have been applied.
 *
 *  \param ctx The decoder instance
 *
 *  \return 0 on success, otherwise -1. */
int32_t contextTemporalConvertSurfacesPrepare(Context_t* ctx);

/* \brief Prepares a target surface for upscaling from LOQ-2 to LOQ-1.
 *
 *  This only prepares the surface if the bitstream signals `scaling mode level-1`
 *  as not 0D.
 *
 *  \param ctx The decoder instance
 *
 *  \return 0 on success, otherwise -1. */
int32_t contextLOQ2TargetSurfacePrepare(Context_t* ctx);

/* \brief Prepares the surfaces for external surface input
 *
 *  This function does not allocate any surface memory, it just prepares the
 *  surface state.
 *
 * \param ctx The decoder instance */
void contextExternalSurfacesPrepare(Context_t* ctx);

/* \brief Retrieves the dequantization parameters requires for decoding enhancement
 *         data for a given plane and LOQ.
 *
 *  This function asserts the passed in parameters are correct.
 *
 *  \param ctx        The decoder instance
 *  \param planeIndex The plane to obtain parameters for.
 *  \param loq        The LOQ to obtain parameters for.
 *
 *  \return Pointer to the dequantization parameters to use. */
const Dequant_t* contextGetDequant(Context_t* ctx, int32_t planeIndex, LOQIndex_t loq);

/*------------------------------------------------------------------------------*/

#endif
