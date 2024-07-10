/* Copyright (c) V-Nova International Limited 2022-2024. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. ANY ONWARD
 * DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO THE
 * EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include "common/dither.h"
#include "common/memory.h"
#include "common/simd.h"
#include "common/stats.h"
#include "common/time.h"
#include "common/types.h"
#include "context.h"
#include "core_version.h"
#include "decode/decode_parallel.h"
#include "decode/decode_serial.h"
#include "decode/deserialiser.h"
#include "LCEVC/PerseusDecoder.h"
#include "surface/blit.h"
#include "surface/sharpen.h"
#include "surface/upscale.h"

#if VN_CORE_FEATURE(OVERLAY_IMAGE)
#include "surface/overlay.h"
#endif

#include <assert.h>

#if VN_CORE_FEATURE(EMSCRIPTEN_TRACING)
#include <emscripten/trace.h>
#endif

/*-----------------------------------------------------------------------------*/

typedef struct perseus_decoder_impl
{
    Context_t* m_context;
} perseus_decoder_impl;

/*-----------------------------------------------------------------------------*/

/* @todo: Can probably remove this. */
#if VN_OS(WINDOWS)
#define VN_CALLCONV __cdecl
#else
#define VN_CALLCONV
#endif

/*-----------------------------------------------------------------------------*/

static inline bool shouldUpscaleApplyDither(Context_t* ctx)
{
    return ditherIsEnabled(ctx->dither) && !sharpenIsEnabled(ctx->sharpen);
}

static inline void surfacesFromImage(Context_t* ctx, LOQIndex_t loq, const perseus_image* image,
                                     Surface_t* surfaces, uint32_t planeCount)
{
    /* Convert from external API to internal. */
    const BitDepth_t depth = bitdepthFromAPI(image->depth);
    const Interleaving_t ilv = interleavingFromAPI(image->ilv);
    const FixedPoint_t fpType = fixedPointFromBitdepth(depth);

    for (uint32_t planeIndex = 0; planeIndex < planeCount && planeIndex < RCMaxPlanes; ++planeIndex) {
        /* Plane 0 NV12 from external image is not really interleaved. */
        const Interleaving_t plane_ilv = ((ilv == ILNV12) && (planeIndex == 0)) ? ILNone : ilv;

        surfaceIdle(&surfaces[planeIndex]);

        /* Set-up, even if there is no destination surface (that will be handled later) */
        uint32_t width;
        uint32_t height;

        deserialiseCalculateSurfaceProperties(&ctx->deserialised, loq, planeIndex, &width, &height);
        surfaceInitialiseExt(&surfaces[planeIndex], image->plane[planeIndex], fpType, width, height,
                             image->stride[planeIndex], plane_ilv);
    }
}

/*-----------------------------------------------------------------------------*/

static inline bool copyDeserialisedToGlobalConfig(Logger_t log, perseus_global_config* config,
                                                  DeserialisedData_t* data)
{
    if (!config) {
        VN_ERROR(log, "perseus_global_config data pointer NULL\n");
        return false;
    }
    if (!data) {
        VN_ERROR(log, "deserialised_data data pointer NULL\n");
        return false;
    }

    config->nal_idr_set = (data->type == NTIDR) ? 1 : 0;
    config->width = data->width;
    config->height = data->height;
    config->num_planes = data->numPlanes;
    config->num_layers = data->numLayers;
    config->use_predicted_average = data->usePredictedAverage;
    config->temporal_use_reduced_signalling = data->temporalUseReducedSignalling;
    config->temporal_enabled = data->temporalEnabled;
    config->use_deblocking = data->deblock.enabled;

    config->scaling_modes[PSS_LOQ_0] = scalingModeToAPI(data->scalingModes[LOQ0]);
    config->scaling_modes[PSS_LOQ_1] = scalingModeToAPI(data->scalingModes[LOQ1]);

    config->temporal_step_width_modifier = data->temporalStepWidthModifier;
    config->chroma_stepwidth_multiplier = data->chromaStepWidthMultiplier;
    config->colourspace = chromaToAPI(data->chroma);
    config->upsample = upscaleTypeToAPI(data->upscale);

    config->bitdepths[LOQ0] = bitdepthToAPI(data->enhaDepth);
    config->bitdepths[LOQ2] = bitdepthToAPI(data->baseDepth);
    config->bitdepths[LOQ1] = config->bitdepths[LOQ2];

    return true;
}

static inline bool bitdepthMatchesExpected(Logger_t log, const BitDepth_t expectedDepths[LOQMaxCount],
                                           const perseus_image* image, const char* imageString,
                                           LOQIndex_t loq)
{
    BitDepth_t bitdepth = bitdepthFromAPI(image->depth);
    if (bitdepth != expectedDepths[loq]) {
        const char* bitdepthString = bitdepthToString(bitdepth);
        const char* expectedString = bitdepthToString(expectedDepths[loq]);
        const char* loqString = loqIndexToString(loq);
        VN_ERROR(log, "Depth is %s, but expected %s for %s [%s]\n", bitdepthString, expectedString,
                 loqString, imageString);
        return false;
    }
    return true;
}
/*-----------------------------------------------------------------------------*/

VN_DEC_CORE_API const char* perseus_get_version(void) { return CoreVersionFull(); }

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_config_init(perseus_decoder_config* cfg)
{
    if (!cfg) {
        return -1;
    }

    memset(cfg, 0, sizeof(perseus_decoder_config));
    cfg->num_worker_threads = -1;
    cfg->pipeline_mode = PPM_SPEED;
    cfg->use_external_buffers = false;
    cfg->simd_type = PSS_SIMD_AUTO;
    cfg->debug_config_path = NULL;
    cfg->s_strength = -1.0f;
    cfg->dither_seed = 0;
    cfg->dither_override_strength = -1;
    cfg->generate_cmdbuffers = 0;
    cfg->apply_cmdbuffers_internal = false;
    cfg->apply_cmdbuffers_threads = 1;
#if VN_CORE_FEATURE(OVERLAY_IMAGE)
    cfg->logo_overlay_position_x = LOGO_OVERLAY_POSITION_X_DEFAULT;
    cfg->logo_overlay_position_y = LOGO_OVERLAY_POSITION_Y_DEFAULT;
    cfg->logo_overlay_delay = LOGO_OVERLAY_DELAY_DEFAULT;
#endif
    return 0;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_open(perseus_decoder* pp,
                                                     const perseus_decoder_config* const cfg)
{
    int32_t res;

    if (!cfg) {
        return -1;
    }

    if (!pp) {
        return -1;
    }

    /* @todo(bob): Error catching for all this initialisation logic can be a lot better, this is
     *             hideously leaky under error. */

    MemorySettings_t memoryParams = {0};

    Memory_t memory = NULL;
    if (!memoryInitialise(&memory, &memoryParams)) {
        return -1;
    }

    perseus_decoder_impl* instance = VN_CALLOC_T(memory, perseus_decoder_impl);
    if (!instance) {
        memoryRelease(memory);
        return -1;
    }

    Context_t* ctx = VN_CALLOC_T(memory, Context_t);
    if (!ctx) {
        VN_FREE(memory, instance);
        memoryRelease(memory);
        return -1;
    }

    Logger_t log = NULL;
    LoggerSettings_t logConfig = {0};
    logConfig.callback = cfg->log_callback;
    logConfig.userData = cfg->log_userdata;

    if (!logInitialize(memory, &log, &logConfig)) {
        VN_FREE(memory, ctx);
        VN_FREE(memory, instance);
        memoryRelease(memory);
        return -1;
    }

    if (cfg->s_strength != -1.0f && (cfg->s_strength < 0.0f || cfg->s_strength > 1.0f)) {
        VN_ERROR(log, "invalid configuration: s_strength out of valid range: [0.0, 1.0]\n");
        VN_FREE(memory, instance);
        VN_FREE(memory, ctx);
        memoryRelease(memory);
        return -1;
    }

    ctx->pipelineMode = cfg->pipeline_mode;
    ctx->useExternalSurfaces = cfg->use_external_buffers;
    ctx->generateSurfaces = false;
    ctx->convertS8 = false;
    ctx->disableTemporalApply = false;
    VN_CHECK(strcpyDeep(memory, cfg->debug_config_path, &ctx->debugConfigPath));
    ctx->useApproximatePA = cfg->use_approximate_pa;
    ctx->useOldCodeLengths = cfg->use_old_code_lengths;

#if VN_CORE_FEATURE(OVERLAY_IMAGE)
    if (cfg->logo_overlay_delay > VN_OVERLAY_MAX_DELAY()) {
        VN_ERROR(log, "invalid configuration: logo_overlay_delay out of valid range: [0, %u]\n",
                 VN_OVERLAY_MAX_DELAY());
        VN_FREE(memory, instance);
        VN_FREE(memory, ctx);
        memoryRelease(memory);
        return -1;
    }

#if VN_CORE_FEATURE(FORCE_OVERLAY)
    if (!cfg->logo_overlay_enable) {
        VN_INFO(log, "Disabling the overlay is not supported on this version", VN_OVERLAY_MAX_DELAY());
    }
    ctx->useLogoOverlay = true;
#else
    ctx->useLogoOverlay = cfg->logo_overlay_enable;
#endif

    ctx->logoOverlayPositionX = cfg->logo_overlay_position_x;
    ctx->logoOverlayPositionY = cfg->logo_overlay_position_y;
    ctx->logoOverlayDelay = cfg->logo_overlay_delay;
    ctx->logoOverlayCount = 0;
#endif

    VN_CHECK(strcpyDeep(memory, cfg->dump_path, &ctx->dumpPath));
    ctx->dumpSurfaces = cfg->dump_surfaces;

    for (int32_t i = 0; i < LOQEnhancedCount; ++i) {
        ctx->highlightState[i].enabled = false;
        ctx->highlightState[i].valSigned = 0;
        ctx->highlightState[i].valUnsigned = 0;
    }

    contextPlaneSurfacesInitialise(ctx);

    if (cfg->simd_type == PSS_SIMD_AUTO) {
        ctx->cpuFeatures = detectSupportedSIMDFeatures();
    } else {
        ctx->cpuFeatures = CAFNone;
    }

    profilerInitialise(&ctx->profiler, memory, log);

    uint32_t threadCount =
        (cfg->num_worker_threads == -1) ? threadingGetNumCores() : cfg->num_worker_threads;

    VN_CHECK(threadingInitialise(memory, log, ctx->profiler, &ctx->threadManager, threadCount));
    if (ctx->dumpSurfaces) {
        VN_CHECK(surfaceDumpCacheInitialise(memory, log, &ctx->surfaceDumpCache));
    }

    deserialiseInitialise(memory, &ctx->deserialised);
    ctx->deserialised.globalConfigSet = false;

    ctx->generateCmdBuffers = cfg->generate_cmdbuffers;
    ctx->applyCmdBuffers = ctx->generateCmdBuffers && cfg->apply_cmdbuffers_internal;
    if (ctx->generateCmdBuffers) {
        ctx->applyCmdBufferThreads =
            (cfg->apply_cmdbuffers_threads < 0)
                ? (uint16_t)clampS32(threadingGetNumCores(), 1, MaxCmdBufferEntryPoints)
                : (uint16_t)cfg->apply_cmdbuffers_threads;
        if (ctx->applyCmdBufferThreads > MaxCmdBufferEntryPoints) {
            VN_ERROR(log, "invalid configuration: requested cmdBufferThreads %d is too high, max 16\n",
                     ctx->applyCmdBufferThreads);
            return -1;
        }
    } else {
        ctx->applyCmdBufferThreads = 1;
    }

    /* @todo: Proper clean-up. */

    if (!ditherInitialize(memory, &ctx->dither, cfg->dither_seed, !cfg->disable_dithering,
                          cfg->dither_override_strength)) {
        return -1;
    }

    if (!sharpenInitialize(&ctx->threadManager, memory, log, &ctx->sharpen, cfg->s_strength)) {
        return -1;
    }

    if (!timeInitialize(memory, &ctx->time)) {
        return -1;
    }

    ctx->useParallelDecode = (cfg->use_parallel_decode == 0) ? false : true;

    if (!ctx->useParallelDecode && !decodeSerialInitialize(memory, ctx->decodeSerial)) {
        return -1;
    }

    if (ctx->useParallelDecode && !decodeParallelInitialize(memory, ctx->decodeParallel)) {
        return -1;
    }

#if VN_CORE_FEATURE(STATS)
    const StatsConfig_t statsConfig = {.enabled = (cfg->debug_internal_stats_path == NULL) ? false : true,
                                       .outputPath = cfg->debug_internal_stats_path,
                                       .time = ctx->time};
    if (!statsInitialize(memory, &ctx->stats, &statsConfig)) {
        return -1;
    }
#endif

    /* VUI non-zero defaults. */
    ctx->vuiInfo.video_format = PSS_VUI_VF_UNSPECIFIED;
    ctx->vuiInfo.colour_primaries = 2;
    ctx->vuiInfo.transfer_characteristics = 2;
    ctx->vuiInfo.matrix_coefficients = 2;

    ctx->memory = memory;
    ctx->log = log;
    instance->m_context = ctx;

    *pp = instance;

    return 0;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_close(perseus_decoder p)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    decodeSerialRelease(ctx->decodeSerial[LOQ0]);
    decodeSerialRelease(ctx->decodeSerial[LOQ1]);
    decodeParallelRelease(ctx->decodeParallel[LOQ0]);
    decodeParallelRelease(ctx->decodeParallel[LOQ1]);
    timeRelease(ctx->time);
    ditherRelease(ctx->dither);
    sharpenRelease(ctx->sharpen);

    if (ctx->started) {
        deserialiseRelease(&ctx->deserialised);
    }

    surfaceDumpCacheRelease(ctx->surfaceDumpCache);

    contextPlaneSurfacesRelease(ctx, ctx->memory);
    threadingRelease(&ctx->threadManager);
    profilerRelease(&ctx->profiler, ctx->memory);
#if VN_CORE_FEATURE(STATS)
    statsRelease(ctx->stats);
#endif

    Memory_t memory = ctx->memory;
    Logger_t log = ctx->log;

    VN_FREE(memory, ctx->debugConfigPath);
    VN_FREE(memory, ctx->dumpPath);
    VN_FREE(memory, ctx);
    VN_FREE(memory, p);

    memoryReport(memory, log);
    logRelease(log);
    memoryRelease(memory);

    return 0;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_config_deserialise(perseus_decoder const p,
                                                                   const uint8_t* perseus, size_t perseusLen,
                                                                   perseus_global_config* config)
{
    Context_t* const ctx = p ? p->m_context : NULL;
    DeserialisedData_t deserialised = {0};
    int32_t res;

    if (!ctx) {
        return -1;
    }

    if (!perseus) {
        VN_ERROR(ctx->log, "Perseus data pointer NULL\n");
        return -1;
    }

    if (!config) {
        VN_ERROR(ctx->log, "perseus_global_config data pointer NULL\n");
        return -1;
    }

    deserialiseInitialise(ctx->memory, &deserialised);

    memset(config, 0, sizeof(perseus_global_config));

    VN_CHECK(deserialise(ctx->memory, ctx->log, perseus, (uint32_t)perseusLen, &deserialised, ctx,
                         Parse_GlobalConfig));

    /* Copy data from DeserialisedData to perseus_global_config for output if config block was present this frame */
    config->global_config_set = deserialised.currentGlobalConfigSet;

    if (deserialised.globalConfigSet) {
        copyDeserialisedToGlobalConfig(ctx->log, config, &deserialised);
    }

    deserialiseRelease(&deserialised);

    return 0;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_parse(perseus_decoder const p, const uint8_t* perseus,
                                                      size_t perseusLen, perseus_decoder_stream* stm)
{
    Context_t* const ctx = p ? p->m_context : NULL;
    int32_t res = 0;

    if (!ctx) {
        return -1;
    }

    VN_PROFILE_FUNCTION();

    if (stm != NULL) {
        memset(stm, 0, sizeof(perseus_decoder_stream));
    }

    if (!perseus) {
        VN_ERROR(ctx->log, "Perseus data pointer NULL\n");
        return -1;
    }

    DeserialisedData_t* data = &ctx->deserialised;
    VN_CHECK(deserialise(ctx->memory, ctx->log, perseus, (uint32_t)perseusLen, data, ctx, Parse_Full));

    if (ctx->debugConfigPath != NULL) {
        deserialiseDump(ctx->log, ctx->debugConfigPath, data);
        VN_FREE(ctx->memory, ctx->debugConfigPath);
        ctx->debugConfigPath = NULL;
    }

    DequantArgs_t dequantArgs = {0};
    initialiseDequantArgs(data, &dequantArgs);

    VN_CHECK(dequantCalculate(&ctx->dequant, &dequantArgs));

    /* Correctly configure bit-depths for each LOQ - this sets up the appropriate
     * fixed point-types for each LOQ based upon pipeline mode. */
    contextSetDepths(ctx);

    /* Setup pipeline configuration during parse. */
    VN_CHECK(contextTemporalConvertSurfacesPrepare(ctx, ctx->memory, ctx->log));

    if (ctx->generateSurfaces) {
        contextExternalSurfacesPrepare(ctx);
    }

    // Ideally this value and the stm->loq_reset[LOQ0] should be 1 at the same time, however I suspect they are not :(
    bool clearTemporal = ((data->temporalRefresh && data->temporalEnabled) ||
                          (ctx->generateSurfaces && !data->temporalEnabled));

    if (stm != NULL) {
        stm->loq_reset[LOQ1] = 1;
        stm->loq_reset[LOQ0] = data->temporalRefresh || !data->temporalEnabled;
    }

    /* Clear surfaces */
    for (uint32_t planeIndex = 0; planeIndex < ctx->deserialised.numPlanes; ++planeIndex) {
        PlaneSurfaces_t* plane = &ctx->planes[planeIndex];

        if (ctx->generateSurfaces) {
            VN_PROFILE_START("Reset base-pixels");

            if (!ctx->generateCmdBuffers || ctx->applyCmdBuffers) {
                surfaceZero(ctx->memory, &plane->basePixels);
                surfaceZero(ctx->memory, &plane->basePixelsU8);
            }

            VN_PROFILE_STOP();
        }

        /* Clear temporal buffer (currently only LOQ0) if required. */
        if (clearTemporal) {
            VN_PROFILE_START("Reset temporal buffer");

            if (!ctx->generateCmdBuffers || ctx->applyCmdBuffers) {
                for (int32_t i = 0; i < 2; ++i) {
                    surfaceZero(ctx->memory, &plane->temporalBuffer[i]);
                }
                surfaceZero(ctx->memory, &plane->temporalBufferU8);
            }

            VN_PROFILE_STOP();
        }
    }

    ctx->started = 1;

    if (stm != NULL) {
        perseus_decoder_get_stream(p, stm);
    }

    if (!ditherRegenerate(ctx->dither, data->ditherStrength, data->ditherType)) {
        VN_PROFILE_STOP();
        return -1;
    }

    if (!sharpenSet(ctx->sharpen, data->sharpenType, data->sharpenStrength)) {
        VN_PROFILE_STOP();
        return -1;
    }

    VN_PROFILE_STOP();

    return 0;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_get_stream(perseus_decoder p, perseus_decoder_stream* stm)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    if (ctx->started != 1) {
        VN_ERROR(ctx->log, "Call perseus_decoder_parse() first\n");
        return -1;
    }

    if (!stm) {
        VN_ERROR(ctx->log, "stm pointer is null\n");
        return -1;
    }

    DeserialisedData_t* data = &ctx->deserialised;

    stm->global_config.global_config_set = data->currentGlobalConfigSet;
    copyDeserialisedToGlobalConfig(ctx->log, &stm->global_config, data);

    stm->pic_type = pictureTypeToAPI(data->picType);
    stm->dither_info.dither_type = ditherTypeToAPI(data->ditherType);
    stm->dither_info.dither_strength = data->ditherStrength;
    stm->s_info.mode = sharpenTypeToAPI(data->sharpenType);
    stm->s_info.strength = sharpenGetStrength(ctx->sharpen);
    stm->base_hash = 0;
    stm->loq_enabled[PSS_LOQ_0] = data->entropyEnabled[LOQ0] ? 1 : 0;
    stm->loq_enabled[PSS_LOQ_1] = data->entropyEnabled[LOQ1] ? 1 : 0;

    stm->pipeline_mode = data->pipelineMode;

    memcpy(&stm->hdr_info, &ctx->hdrInfo, sizeof(lcevc_hdr_info));
    memcpy(&stm->vui_info, &ctx->vuiInfo, sizeof(lcevc_vui_info));
    memcpy(&stm->deinterlacing_info, &ctx->deinterlacingInfo, sizeof(lcevc_deinterlacing_info));
    memcpy(&stm->conformance_window, &data->conformanceWindow, sizeof(lcevc_conformance_window));

    return 0;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_decode_base(perseus_decoder const p,
                                                            const perseus_image* base)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    if (!base) {
        VN_ERROR(ctx->log, "perseus_image* base is NULL\n");
        return -1;
    }

    if (!ctx->started) {
        VN_ERROR(ctx->log, "Call perseus_decoder_parse() first\n");
        return -1;
    }

    DeserialisedData_t* data = &ctx->deserialised;

    if (!data->globalConfigSet) {
        VN_ERROR(ctx->log, "Have not yet received a global config block\n");
        return -1;
    }

    if (ctx->useExternalSurfaces) {
        for (uint32_t planeIndex = 0; planeIndex < data->numPlanes; ++planeIndex) {
            const PlaneSurfaces_t* plane = &ctx->planes[planeIndex];
            if (surfaceIsIdle(&plane->externalSurfaces[LOQ1])) {
                VN_ERROR(ctx->log, "calling error: external surfaces being used but not set\n");
                return -1;
            }
        }
    }

    if (!bitdepthMatchesExpected(ctx->log, ctx->inputDepth, base, "base", LOQ1)) {
        return -1;
    }

    VN_PROFILE_FUNCTION();

    /* Convert from external API to internal. */
    Surface_t baseSurfaces[3] = {{0}};
    surfacesFromImage(ctx, LOQ1, base, baseSurfaces, 3);

    const bool internalSurfaces = contextLOQUsingInternalSurfaces(ctx, ctx->memory, ctx->log, LOQ1);
    if (internalSurfaces && (ctx->deserialised.scalingModes[LOQ1] == Scale0D) &&
        (contextInternalSurfacesImageCopy(ctx, ctx->log, baseSurfaces, LOQ1, true) != 0)) {
        VN_ERROR(ctx->log, "Failed to load internal surface for base input\n");
        return -1;
    }

    Surface_t* decodeDstPlanes[RCMaxPlanes] = {0};
    for (int32_t planeIndex = 0; planeIndex < RCMaxPlanes; ++planeIndex) {
        decodeDstPlanes[planeIndex] = internalSurfaces ? &ctx->planes[planeIndex].internalSurfaces[LOQ1]
                                                       : &baseSurfaces[planeIndex];
    }

    for (uint32_t planeIndex = 0; planeIndex < data->numPlanes && planeIndex < RCMaxPlanes; ++planeIndex) {
        surfaceDump(ctx->memory, ctx->log, ctx, decodeDstPlanes[planeIndex], "dpi_base_predi_P%d",
                    planeIndex);
    }

    FrameStats_t frameStats = statsNewFrame(ctx->stats);

    if (ctx->useParallelDecode) {
        DecodeParallelArgs_t params = {
            .deserialised = &ctx->deserialised,
            .log = ctx->log,
            .threadManager = &(ctx->threadManager),
            .dst = {decodeDstPlanes[0], decodeDstPlanes[1], decodeDstPlanes[2]},
            .loq = LOQ1,
            .scalingMode = Scale2D,
            .dequant = contextGetDequant(ctx, 0, LOQ1),
            .stats = frameStats,
            .deblock = &data->deblock,
            .highlight = &ctx->highlightState[LOQ1],
            .useOldCodeLengths = ctx->useOldCodeLengths,
            .applyTemporal = false, /* Never apply temporal at LOQ1 */
        };

        if (decodeParallel(ctx, ctx->decodeParallel[LOQ1], &params) != 0) {
            VN_ERROR(ctx->log, "Failed during parallel decode loop LOQ1\n");
            return -1;
        }
    } else {
        DecodeSerialArgs_t params = {
            .dst = {decodeDstPlanes[0], decodeDstPlanes[1], decodeDstPlanes[2]},
            .loq = LOQ1,
            .memory = ctx->memory,
            .log = ctx->log,
            .stats = frameStats,
            .tuCoordsAreInSurfaceRasterOrder =
                (!ctx->deserialised.temporalEnabled && ctx->deserialised.tileDimensions == TDTNone),
            .applyTemporal = false, /* Never apply temporal at LOQ1 */
        };

        if (decodeSerial(ctx, &params) != 0) {
            VN_ERROR(ctx->log, "Failed during decode serial\n");
            return -1;
        }
    }

    for (uint32_t planeIndex = 0; planeIndex < data->numPlanes && planeIndex < RCMaxPlanes; ++planeIndex) {
        surfaceDump(ctx->memory, ctx->log, ctx, decodeDstPlanes[planeIndex], "dpi_base_recon_P%d",
                    planeIndex);
    }

    VN_PROFILE_STOP();

    return 0;
}

VN_DEC_CORE_API int perseus_decoder_apply_ext_residuals(perseus_decoder const p, const perseus_image* input,
                                                        perseus_image* residuals, int planeIndex,
                                                        perseus_loq_index loqIndex)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    if (!input || !residuals) {
        VN_ERROR(ctx->log, "perseus_image* input or residuals is NULL\n");
        return -1;
    }

    if (!ctx->started) {
        VN_ERROR(ctx->log, "calling error: call perseus_decoder_parse() first\n");
        return -1;
    }

    const LOQIndex_t loq = loqIndexFromAPI(loqIndex);

    uint32_t width;
    uint32_t height;
    deserialiseCalculateSurfaceProperties(&ctx->deserialised, loq, planeIndex, &width, &height);

    perseus_buffer_info resPSSInfo = {{0}};
    perseus_decoder_get_surface_info(p, planeIndex, &resPSSInfo);

    FixedPoint_t srcType = fixedPointFromBitdepth(bitdepthFromAPI(residuals->depth));
    FixedPoint_t dstType = fixedPointFromBitdepth(bitdepthFromAPI(input->depth));

    /* If the residual surface is S16, then fixedpoint_type should be high precision */
    if (resPSSInfo.format == PSS_SURFACE_S16) {
        srcType = fixedPointHighPrecision(srcType);
    }

    Surface_t src;
    Surface_t dst;

    surfaceIdle(&src);
    surfaceIdle(&dst);
    surfaceInitialiseExt(&src, residuals->plane[planeIndex], srcType, width, height,
                         residuals->stride[planeIndex], ILNone);
    surfaceInitialiseExt(&dst, input->plane[planeIndex], dstType, width, height,
                         input->stride[planeIndex], ILNone);

    return surfaceBlit(ctx->log, &(ctx->threadManager), ctx->cpuFeatures, &src, &dst, BMAdd) ? 0 : -1;
}

VN_DEC_CORE_API int32_t VN_CALLCONV perseus_decoder_upscale(perseus_decoder const p,
                                                            const perseus_image* full,
                                                            const perseus_image* base,
                                                            perseus_loq_index baseLOQ)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    DeserialisedData_t* data = &ctx->deserialised;

    if (!full || !base) {
        VN_ERROR(ctx->log, "perseus_image* full or perseus_image* base is NULL\n");
        return -1;
    }

    if (!ctx->started) {
        VN_ERROR(ctx->log, "calling error: call perseus_decoder_decode_base() first\n");
        return -1;
    }

    if (!data->globalConfigSet) {
        VN_ERROR(ctx->log, "stream corrupt: global config not received\n");
        return -1;
    }

    const LOQIndex_t baseIndex = loqIndexFromAPI(baseLOQ);
    const LOQIndex_t targetIndex = loqIndexFromAPI(baseIndex - 1);

    if (baseIndex != LOQ2 && baseIndex != LOQ1) {
        VN_ERROR(ctx->log, "calling error: base_loq must either be PSS_LOQ_1 or PSS_LOQ_2, received: %s\n",
                 loqIndexToString(baseIndex));
        return -1;
    }

    if (targetIndex != (baseIndex - 1)) {
        VN_ERROR(ctx->log, "calling error: target_loq must be one LOQ from base_loq. base: %s, target: %s\n",
                 loqIndexToString(baseIndex), loqIndexToString(targetIndex));
        return -1;
    }

    if (base->ilv != full->ilv) {
        VN_ERROR(ctx->log, "calling error: base ilv (%d) must be the same as full ilv (%d)\n",
                 base->ilv, full->ilv);
        return -1;
    }

    if (!bitdepthMatchesExpected(ctx->log, ctx->inputDepth, base, "base", baseIndex)) {
        return -1;
    }

    // Note that at this stage, the input bitdepth should ALSO match the upscaled image here.
    if (!bitdepthMatchesExpected(ctx->log, ctx->inputDepth, full, "full", targetIndex)) {
        return -1;
    }

    if (full->ilv != PSS_ILV_NONE && (ctx->pipelineMode == PPM_PRECISION)) {
        VN_ERROR(ctx->log, "calling error: Precision mode only supports planar interleaving\n");
        return -1;
    }

    if ((data->scalingModes[targetIndex] == Scale0D) && (targetIndex == LOQ1)) {
        VN_ERROR(ctx->log, "calling error: Upscale should only be called when upscaling is required at this LOQ, %s -> %s has a 0D scale\n",
                 loqIndexToString(baseIndex), loqIndexToString(targetIndex));
        return -1;
    }

    VN_PROFILE_FUNCTION();

    /* Convert from external API to internal. */
    Surface_t baseSurfaces[3] = {{0}};
    Surface_t fullSurfaces[3] = {{0}};
    surfacesFromImage(ctx, baseIndex, base, baseSurfaces, 3);
    surfacesFromImage(ctx, targetIndex, full, fullSurfaces, 3);

    uint32_t planeCount = 0;

    if (data->chroma == CTMonochrome) {
        planeCount = 1;
    } else {
        switch (interleavingFromAPI(full->ilv)) {
            case ILNone: planeCount = 3; break;
            case ILNV12: planeCount = 2; break;
            case ILYUYV:
            case ILUYVY:
            case ILRGB:
            case ILRGBA: planeCount = 1; break;
            default: return -1;
        }
    }

    const bool paEnabled = upscalePAIsEnabled(ctx->log, ctx);
    const bool ditherEnabled = shouldUpscaleApplyDither(ctx);
    const bool baseIsInternal = contextLOQUsingInternalSurfaces(ctx, ctx->memory, ctx->log, baseIndex);
    const bool targetIsInternal = contextLOQUsingInternalSurfaces(ctx, ctx->memory, ctx->log, targetIndex);

    /* For internal bases at LOQ2, copy (for now). However, really we don't need to unless we're
     * precision or there's a depth change, which shouldn't be the case for this LOQ. */
    if (baseIsInternal && (baseIndex == LOQ2) &&
        (contextInternalSurfacesImageCopy(ctx, ctx->log, baseSurfaces, LOQ2, true) != 0)) {
        VN_ERROR(ctx->log, "Failed to load internal surface for base input\n");
        return -1;
    }

    if (data->scalingModes[targetIndex] == Scale0D) {
        /* Perform a copy from base to high here. This is quite suboptimal, and is
         * a clear indicator we must improve the API.
         *
         * Doing this however prevents complex logic in the user space as the
         * user does not know if we are using internal surfaces, so they may attempt
         * to bypass this upscale call and copy themselves, or just reference the
         * base surface in subsequent API calls, but if there is a depth shift, even
         * in `speed` mode, internal surfaces may be used. This will all be improved
         * once we sort out the API - noting currently only LOQ-1 -> LOQ-0 suffers
         * this issue it's totally safe to skip the upscale call for LOQ-2 -> LOQ-1.
         *
         * Noting it is possible to work around this issue with even more spaghetti
         * in the API - preference is to avoid that and keep this comment as a warning,
         * also important to note that LOQ-1 -> LOQ-0 0D scaling is essentially not
         * a feature we're concerned with and merely a conformance configuration. */
        for (uint32_t planeIndex = 0; planeIndex < planeCount; ++planeIndex) {
            const PlaneSurfaces_t* internalPlane = &ctx->planes[planeIndex];

            const Surface_t* src = baseIsInternal ? &internalPlane->internalSurfaces[baseIndex]
                                                  : &baseSurfaces[planeIndex];

            const Surface_t* dst = targetIsInternal ? &internalPlane->internalSurfaces[targetIndex]
                                                    : &fullSurfaces[planeIndex];

            if (!surfaceBlit(ctx->log, &(ctx->threadManager), ctx->cpuFeatures, src, dst, BMCopy)) {
                return -1;
            }

            surfaceDump(ctx->memory, ctx->log, ctx, src, "dpi_upscale_L%u_P%u_src",
                        (uint32_t)baseIndex, planeIndex);
            surfaceDump(ctx->memory, ctx->log, ctx, dst, "dpi_upscale_L%u_P%u_dst",
                        (uint32_t)baseIndex, planeIndex);
        }
    } else {
        for (uint32_t planeIndex = 0; planeIndex < planeCount; ++planeIndex) {
            UpscaleArgs_t params = {0};
            const PlaneSurfaces_t* internalPlane = &ctx->planes[planeIndex];

            params.src = baseIsInternal ? &internalPlane->internalSurfaces[baseIndex]
                                        : &baseSurfaces[planeIndex];

            params.dst = targetIsInternal ? &internalPlane->internalSurfaces[targetIndex]
                                          : &fullSurfaces[planeIndex];

            params.applyPA = paEnabled;
            params.applyDither = ditherEnabled && (planeIndex == 0) && (baseIndex == LOQ1);
            params.type = data->upscale;
            params.mode = data->scalingModes[targetIndex];
            params.preferredAccel = ctx->cpuFeatures;

            if (!upscale(ctx->memory, ctx->log, ctx, &params)) {
                return -1;
            }

            surfaceDump(ctx->memory, ctx->log, ctx, params.src, "dpi_upscale_L%u_P%u_src",
                        (uint32_t)baseIndex, planeIndex);
            surfaceDump(ctx->memory, ctx->log, ctx, params.dst, "dpi_upscale_L%u_P%u_dst",
                        (uint32_t)baseIndex, planeIndex);
        }
    }

    VN_PROFILE_STOP();

    return 0;
}

static bool apply_temporal_buffer(Logger_t log, Context_t* ctx, Surface_t* dst[RCMaxPlanes])
{
    const DeserialisedData_t* data = &ctx->deserialised;

    if (!data->temporalEnabled || ctx->disableTemporalApply ||
        (ctx->generateCmdBuffers && !ctx->applyCmdBuffers)) {
        return true;
    }

    const int32_t planeCount = data->numPlanes;

    for (int32_t i = 0; i < planeCount && i < RCMaxPlanes; i++) {
        const PlaneSurfaces_t* plane = &ctx->planes[i];
        const Surface_t* src = &plane->temporalBuffer[data->fieldType];

        /* only apply if we have somewhere to apply to */
        if (dst[i] && dst[i]->data &&
            !surfaceBlit(log, &(ctx->threadManager), ctx->cpuFeatures, src, dst[i], BMAdd)) {
            return false;
        }
    }

    return true;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_decode_high(perseus_decoder const p,
                                                            const perseus_image* full)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    if (!full) {
        VN_ERROR(ctx->log, "perseus_image* full is NULL\n");
        return -1;
    }

    if (!ctx->started) {
        VN_ERROR(ctx->log, "calling error: call perseus_decoder_parse() first\n");
        return -1;
    }

    DeserialisedData_t* data = &ctx->deserialised;

    if (!data->globalConfigSet) {
        VN_ERROR(ctx->log, "stream corrupt: global config not received\n");
        return -1;
    }

    /* Verify external surfaces are valid. */
    if (ctx->useExternalSurfaces) {
        for (int32_t planeIndex = 0; planeIndex < data->numPlanes; ++planeIndex) {
            PlaneSurfaces_t* plane = &ctx->planes[planeIndex];

            if (surfaceIsIdle(&plane->externalSurfaces[LOQ0])) {
                VN_ERROR(ctx->log, "calling error: external surfaces being used but not set\n");
                return -1;
            }
        }
    }

    if (!bitdepthMatchesExpected(ctx->log, ctx->outputDepth, full, "full", LOQ0)) {
        return -1;
    }

    VN_PROFILE_FUNCTION();

    Surface_t fullSurfaces[RCMaxPlanes] = {{0}};
    surfacesFromImage(ctx, LOQ0, full, fullSurfaces, 3);

    /* Determine target surfaces to apply residuals to. */
    Surface_t* decodeDstPlanes[RCMaxPlanes] = {0};
    const bool bInternalSurfaces = contextLOQUsingInternalSurfaces(ctx, ctx->memory, ctx->log, LOQ0);
    for (int32_t planeIndex = 0; planeIndex < RCMaxPlanes; ++planeIndex) {
        decodeDstPlanes[planeIndex] = bInternalSurfaces ? &ctx->planes[planeIndex].internalSurfaces[LOQ0]
                                                        : &fullSurfaces[planeIndex];
    }

    FrameStats_t frameStats = statsGetFrame(ctx->stats);

    if (ctx->useParallelDecode) {
        DecodeParallelArgs_t params = {
            .deserialised = &ctx->deserialised,
            .log = ctx->log,
            .threadManager = &(ctx->threadManager),
            .dst = {decodeDstPlanes[0], decodeDstPlanes[1], decodeDstPlanes[2]},
            .loq = LOQ0,
            .scalingMode = data->scalingModes[LOQ0],
            .dequant = contextGetDequant(ctx, 0, LOQ0),
            .preferredAccel = ctx->cpuFeatures,
            .stats = frameStats,
            .deblock = NULL,
            .highlight = &ctx->highlightState[LOQ0],
            .useOldCodeLengths = ctx->useOldCodeLengths,
            .applyTemporal = ctx->deserialised.temporalEnabled,
        };

        if (decodeParallel(ctx, ctx->decodeParallel[LOQ0], &params) != 0) {
            VN_ERROR(ctx->log, "Failed during parallel decode loop LOQ0\n");
            return -1;
        }
    } else {
        DecodeSerialArgs_t params = {
            .dst = {decodeDstPlanes[0], decodeDstPlanes[1], decodeDstPlanes[2]},
            .loq = LOQ0,
            .memory = ctx->memory,
            .log = ctx->log,
            .stats = frameStats,
            .tuCoordsAreInSurfaceRasterOrder =
                (!ctx->deserialised.temporalEnabled && ctx->deserialised.tileDimensions == TDTNone),
            .applyTemporal = ctx->deserialised.temporalEnabled,
        };

        if (decodeSerial(ctx, &params) != 0) {
            VN_ERROR(ctx->log, "Can't apply full image data\n");
            return -1;
        }
    }

    VN_FRAMESTATS_RECORD_START(frameStats, STApplyTemporalBufferStart);
    bool temporalRes = apply_temporal_buffer(ctx->log, ctx, decodeDstPlanes);
    VN_FRAMESTATS_RECORD_STOP(frameStats, STApplyTemporalBufferStop);

    int32_t res = 0;

    if (!temporalRes) {
        VN_ERROR(ctx->log, "Failed to apply temporal buffer to destination surface\n");
        res = -1;
    }

    statsEndFrame(frameStats);

    for (int32_t planeIndex = 0; planeIndex < data->numPlanes && planeIndex < RCMaxPlanes; ++planeIndex) {
        PlaneSurfaces_t* internalPlane = &ctx->planes[planeIndex];
        Surface_t* temporalSurface = &internalPlane->temporalBuffer[0];
        surfaceDump(ctx->memory, ctx->log, ctx, decodeDstPlanes[planeIndex], "dpi_high_P%d", planeIndex);
        surfaceDump(ctx->memory, ctx->log, ctx, temporalSurface, "dpi_temporal_P%d", planeIndex);
    }

    /* Copy internal surfaces back out, ensuring to copy from the appropriate
     * surface that has been worked on. */
    if (bInternalSurfaces &&
        (contextInternalSurfacesImageCopy(ctx, ctx->log, fullSurfaces, LOQ0, false) != 0)) {
        VN_ERROR(ctx->log, "Failed to store internal surface for high output\n");
        res = -1;
    }

#if VN_CORE_FEATURE(OVERLAY_IMAGE)
    if (perseus_decoder_apply_overlay(p, full) != 0) {
        VN_ERROR(ctx->log, "Failed to apply overlay to destination surface\n");
        res = -1;
    }
#endif
    VN_PROFILE_STOP();

    return res;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_apply_s(perseus_decoder const p, const perseus_image* image)
{
    int32_t res = 0;
    Context_t* ctx = p ? p->m_context : NULL;

    if (ctx == NULL) {
        return -1;
    }

    if (image == NULL) {
        VN_ERROR(ctx->log, "invalid param: image\n");
        return -1;
    }

    if (!bitdepthMatchesExpected(ctx->log, ctx->outputDepth, image, "image", LOQ0)) {
        return -1;
    }

    if (!sharpenIsEnabled(ctx->sharpen)) {
        return res;
    }

    VN_PROFILE_FUNCTION();

    Surface_t imageSurface;
    surfacesFromImage(ctx, LOQ0, image, &imageSurface, 1);

    if (!surfaceSharpen(ctx->sharpen, &imageSurface, ctx->dither, ctx->cpuFeatures)) {
        res = -1;
    }

    VN_PROFILE_STOP();

    return res;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_apply_overlay(perseus_decoder const p,
                                                              const perseus_image* image)
{
#if VN_CORE_FEATURE(OVERLAY_IMAGE)
    int32_t res = 0;
    Context_t* ctx = p ? p->m_context : NULL;
    Surface_t image_surface;

    if (ctx == NULL) {
        return -1;
    }

    if (image == NULL) {
        VN_ERROR(ctx->log, "invalid param: image\n");
        return -1;
    }

    if (!bitdepthMatchesExpected(ctx->log, ctx->outputDepth, image, "image", LOQ0)) {
        return -1;
    }

    if (!overlayIsEnabled(ctx)) {
        return res;
    }

    VN_PROFILE_FUNCTION();

    surfacesFromImage(ctx, LOQ0, image, &image_surface, 1);

    const OverlayArgs_t params = {
        &image_surface,
    };

    if (ctx->logoOverlayCount++ >= ctx->logoOverlayDelay) {
        res = overlayApply(ctx->log, ctx, &params);
    }
    VN_PROFILE_STOP();

    return res;
#else
    return -1;
#endif
}

#if VN_OS(BROWSER)

static void emccTraceEnter(const char* name)
{
#if VN_CORE_FEATURE(EMSCRIPTEN_TRACING)
    emscripten_trace_enter_context(name);
#endif
}

static void emccTraceExit(void)
{
#if VN_CORE_FEATURE(EMSCRIPTEN_TRACING)
    emscripten_trace_report_memory_layout();
    emscripten_trace_report_off_heap_data();
    emscripten_trace_exit_context();
#endif
}

static void emccTraceFrameStart(void)
{
#if VN_CORE_FEATURE(EMSCRIPTEN_TRACING)
    emscripten_trace_record_frame_start();
#endif
}

static void emccTraceFrameEnd(void)
{
#if VN_CORE_FEATURE(EMSCRIPTEN_TRACING)
    emscripten_trace_record_frame_end();
#endif
}

VN_DEC_CORE_API uint8_t VN_CALLCONV perseus_decoder_get_dither_strength(perseus_decoder const p)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    return ctx->deserialised.ditherStrength;
}

VN_DEC_CORE_API uint8_t VN_CALLCONV perseus_decoder_get_dither_type(perseus_decoder const p)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    return ctx->deserialised.ditherType;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_clear_temporal(perseus_decoder const p, const int planeIndex)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    if (planeIndex < 0 || planeIndex >= ctx->deserialised.numPlanes) {
        VN_ERROR(ctx->log, "invalid param: plane_idx=%d invalid\n", planeIndex);
        return -1;
    }

    emccTraceEnter("Clear temporal");

    PlaneSurfaces_t* plane = &ctx->planes[planeIndex];

    surfaceZero(ctx->memory, &plane->temporalBuffer[0]);
    surfaceZero(ctx->memory, &plane->temporalBufferU8);

    emccTraceExit();

    return 0;
}

VN_DEC_CORE_API perseus_decoder VN_CALLCONV perseus_decoder_open_wrapper(int generateSurfaces,
                                                                         int use_parallel_decode)
{
    emccTraceEnter("Open");

    perseus_decoder_config cfg;

    if (perseus_decoder_config_init(&cfg) != 0) {
        emccTraceExit();
        return NULL;
    }

    cfg.use_parallel_decode = use_parallel_decode;
    cfg.num_worker_threads = -1;

    perseus_decoder res = NULL;
    if (perseus_decoder_open(&res, &cfg) != 0) {
        emccTraceExit();
        return NULL;
    }

    Context_t* ctx = res->m_context;
    ctx->generateSurfaces = generateSurfaces;
    ctx->disableTemporalApply = generateSurfaces; /* Don't want to apply if we're generating. */
    ctx->convertS8 = generateSurfaces;

    emccTraceExit();

    return res;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_decode_wrapper(perseus_decoder const p, void* pbaseImage,
                                                               void* pfullImage, uint32_t dst_width,
                                                               uint32_t lumaStride, uint32_t interleaved)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    if (ctx->started != 1) {
        VN_ERROR(ctx->log, "calling error: Call _perseus_decoder_parse() first\n");
        return -1;
    }

    emccTraceEnter("Decode");

    uint32_t baseHeight;

    if (ctx->deserialised.scalingModes[LOQ0] == Scale1D) {
        baseHeight = ctx->deserialised.height;
    } else {
        baseHeight = ctx->deserialised.height / 2;
    }

    uint32_t baseWidth = dst_width / 2;

    perseus_image baseImage;
    memset(&baseImage, 0, sizeof(baseImage));
    baseImage.plane[0] = pbaseImage;
    baseImage.plane[1] = (uint8_t*)baseImage.plane[0] + (baseWidth * baseHeight);
    baseImage.plane[2] = (uint8_t*)baseImage.plane[1] + (baseWidth * baseHeight / 4);

    baseImage.stride[0] = lumaStride;
    baseImage.stride[1] = lumaStride / 2;
    baseImage.stride[2] = lumaStride / 2;
    baseImage.ilv = (perseus_interleaving)interleaved;

    perseus_image fullImage;
    memset(&fullImage, 0, sizeof(fullImage));
    fullImage.plane[0] = pfullImage;
    fullImage.plane[1] = (uint8_t*)fullImage.plane[0] + (dst_width * ctx->deserialised.height);
    fullImage.plane[2] = (uint8_t*)fullImage.plane[1] + (dst_width * ctx->deserialised.height / 4);

    fullImage.stride[0] = lumaStride;
    fullImage.stride[1] = lumaStride / 2;
    fullImage.stride[2] = lumaStride / 2;
    fullImage.ilv = (perseus_interleaving)interleaved;

    const int32_t res = perseus_decoder_decode(p, &fullImage, &baseImage);

    emccTraceExit();

    return res;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_decode_base_wrapper(perseus_decoder const p,
                                                                    void* image, uint32_t imageStride)
{
    if (!p) {
        return -1;
    }

    if (!image) {
        VN_ERROR(p->m_context->log, "invalid param: image=%d invalid\n", image);
        return -1;
    }

    perseus_image img;
    memset(&img, 0, sizeof(img));

    emccTraceEnter("Base");

    img.plane[0] = image;
    img.stride[0] = imageStride;
    img.ilv = PSS_ILV_NONE;

    const int32_t res = perseus_decoder_decode_base(p, &img);

    emccTraceExit();

    return res;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_decode_high_wrapper(perseus_decoder const p,
                                                                    void* image, uint32_t imageStride)
{
    if (!p) {
        return -1;
    }

    if (!image) {
        VN_ERROR(p->m_context->log, "invalid param: image=%d invalid\n", image);
        return -1;
    }

    perseus_image img;
    memset(&img, 0, sizeof(img));

    emccTraceEnter("High");

    img.plane[0] = image;
    img.stride[0] = imageStride;
    img.ilv = PSS_ILV_NONE;

    const int32_t res = perseus_decoder_decode_high(p, &img);

    emccTraceFrameEnd();
    emccTraceExit();

    return res;
}

VN_DEC_CORE_API int perseus_decoder_upscale_wrapper(perseus_decoder const p, void* baseImage,
                                                    uint32_t baseWidth, uint32_t baseHeight,
                                                    void* fullImage, uint32_t fullWidth, uint32_t fullHeight)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    emccTraceEnter("Upscale");

    uint32_t srcSize = baseWidth * baseHeight;
    perseus_image srcImage;
    srcImage.depth = PSS_DEPTH_8;
    srcImage.ilv = PSS_ILV_NONE;
    srcImage.plane[0] = baseImage;
    srcImage.plane[1] = (uint8_t*)baseImage + srcSize;
    srcImage.plane[2] = (uint8_t*)baseImage + (srcSize * 5 / 4);
    srcImage.stride[0] = baseWidth;
    srcImage.stride[1] = srcImage.stride[0] / 2;
    srcImage.stride[2] = srcImage.stride[1];

    uint32_t dstSize = fullWidth * fullHeight;
    perseus_image dstImage;
    dstImage.depth = PSS_DEPTH_8;
    dstImage.ilv = PSS_ILV_NONE;
    dstImage.plane[0] = fullImage;
    dstImage.plane[1] = (uint8_t*)fullImage + dstSize;
    dstImage.plane[2] = (uint8_t*)fullImage + (dstSize * 5 / 4);
    dstImage.stride[0] = fullWidth;
    dstImage.stride[1] = dstImage.stride[0] / 2;
    dstImage.stride[2] = dstImage.stride[1];

    /* @todo: Support specifying the LOQ index. */
    const int32_t res = perseus_decoder_upscale(p, &dstImage, &srcImage, PSS_LOQ_1);

    emccTraceExit();

    return res;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_parse_wrapper(perseus_decoder const p, const uint8_t* perseus,
                                                              size_t perseusLen, int infoPtr[5])
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    if (!infoPtr) {
        VN_ERROR(ctx->log, "invalid param: info_ptr=%d invalid\n", infoPtr);
        return -1;
    }

    emccTraceEnter("Parse");
    emccTraceFrameStart();

    perseus_decoder_stream outStm;
    if (perseus_decoder_parse(p, perseus, perseusLen, &outStm) < 0) {
        VN_ERROR(ctx->log, "calling error: Couldn't parse the data\n");
        return -1;
    }

    infoPtr[0] = outStm.global_config.width;
    infoPtr[1] = outStm.global_config.height;
    infoPtr[2] = (ctx->deserialised.scalingModes[LOQ0] == Scale1D) ? 1 : 0;
    infoPtr[3] = outStm.loq_enabled[PSS_LOQ_1];
    infoPtr[4] = outStm.loq_enabled[PSS_LOQ_0];

    emccTraceExit();

    return 0;
}

VN_DEC_CORE_API uint32_t VN_CALLCONV perseus_decoder_get_surface_size(perseus_decoder const p, int high)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    if (high) {
        return (ctx->deserialised.width * ctx->deserialised.height);
    } else {
        const uint32_t height =
            ctx->deserialised.height >> (ctx->deserialised.scalingModes[LOQ0] == Scale1D ? 0 : 1);
        return ((ctx->deserialised.width >> 1) * height);
    }
}

VN_DEC_CORE_API uint32_t VN_CALLCONV perseus_decoder_get_base_hash(perseus_decoder const p, void* out)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    if (ctx->started != 1) {
        VN_ERROR(ctx->log, "Call _perseus_decoder_parse() first\n");
        return -1;
    }

    const uint64_t hash = 0;

    memcpy(out, &hash, sizeof(uint64_t));

    return 0;
}

VN_DEC_CORE_API void VN_CALLCONV perseus_start_tracing(void)
{
#if VN_CORE_FEATURE(EMSCRIPTEN_TRACING)
    emscripten_trace_configure("http://127.0.0.1:5000/", "V-Nova LCEVC");
    emscripten_trace_set_session_username("liblcevc_dpi");
#endif
}

VN_DEC_CORE_API void VN_CALLCONV perseus_end_tracing(void)
{
#if VN_CORE_FEATURE(EMSCRIPTEN_TRACING)
    emscripten_trace_close();
#endif
}

VN_DEC_CORE_API uint32_t VN_CALLCONV perseus_decoder_get_last_error_wrapper(perseus_decoder const p)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return PSS_PERSEUS_API_CALL_ERROR;
    }

    return PSS_PERSEUS_UNKNOWN_ERROR;
}
#endif /* EMSCRIPTEN BUILD */

VN_DEC_CORE_API void perseus_decoder_get_surface_info(perseus_decoder const p, int planeIndex,
                                                      perseus_buffer_info* bufferInfo)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return;
    }

    if (planeIndex < 0 || planeIndex >= ctx->deserialised.numPlanes) {
        VN_ERROR(ctx->log, "invalid param: plane_idx=%d invalid\n", planeIndex);
        return;
    }

    bufferInfo->format = ctx->convertS8 ? PSS_SURFACE_U8 : PSS_SURFACE_S16;
    bufferInfo->using_external_buffers = ctx->useExternalSurfaces;

    for (uint32_t loq = 0; loq < LOQMaxCount; ++loq) {
        uint32_t width;
        uint32_t height;
        deserialiseCalculateSurfaceProperties(&ctx->deserialised, (LOQIndex_t)loq,
                                              (uint32_t)planeIndex, &width, &height);
        bufferInfo->size[loq] = width * height;
    }
}

VN_DEC_CORE_API void perseus_decoder_set_generate_surfaces(perseus_decoder const p, uint8_t enable,
                                                           perseus_surface_format format,
                                                           uint8_t useExternalBuffer)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return;
    }

    const bool oldConvertS8 = ctx->convertS8;

    ctx->generateSurfaces = enable;
    ctx->disableTemporalApply = enable;
    ctx->convertS8 = (format == PSS_SURFACE_U8) ? true : false;
    ctx->useExternalSurfaces = useExternalBuffer;

    if (ctx->generateSurfaces && oldConvertS8 != ctx->convertS8) {
        contextExternalSurfacesPrepare(ctx);
    }
}

VN_DEC_CORE_API void perseus_decoder_set_surface(perseus_decoder const p, int plane_idx,
                                                 perseus_loq_index loqIndex, void* buffer)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return;
    }

    if (plane_idx < 0 || plane_idx >= ctx->deserialised.numPlanes) {
        VN_ERROR(ctx->log, "invalid param: plane_idx=%d invalid\n", plane_idx);
        return;
    }

    const LOQIndex_t loq = loqIndexFromAPI(loqIndex);

    if (loqIndex != PSS_LOQ_0 && loqIndex != PSS_LOQ_1) {
        VN_ERROR(ctx->log, "invalid param: loq_idx=%d invalid - muster either be PSS_LOQ_0 or PSS_LOQ_1\n",
                 loqIndex);
        return;
    }

    if (!ctx->useExternalSurfaces) {
        VN_INFO(ctx->log, "The use of external surfaces has not been set\n");
    }

    PlaneSurfaces_t* plane = &ctx->planes[plane_idx];
    plane->externalSurfaces[loq].data = (uint8_t*)buffer;
}

VN_DEC_CORE_API void* VN_CALLCONV perseus_decoder_get_surface(perseus_decoder const p, int plane_idx,
                                                              perseus_loq_index loqIndex)
{
    Context_t* ctx = p ? p->m_context : NULL;

    if (!ctx) {
        return NULL;
    }

    if (plane_idx < 0 || plane_idx >= ctx->deserialised.numPlanes) {
        VN_ERROR(ctx->log, "invalid param: plane_idx=%d invalid\n", plane_idx);
        return NULL;
    }

    if (loqIndex != PSS_LOQ_0 && loqIndex != PSS_LOQ_1) {
        VN_ERROR(ctx->log, "invalid param loq_idx=%d invalid - must either be PSS_LOQ_0 or PSS_LOQ_1\n",
                 loqIndex);
        return NULL;
    }

    if (!ctx->generateSurfaces) {
        return NULL;
    }

    const LOQIndex_t loq = loqIndexFromAPI(loqIndex);
    PlaneSurfaces_t* plane = &ctx->planes[plane_idx];

    if (ctx->useExternalSurfaces) {
        return plane->externalSurfaces[loq].data;
    }

    if (ctx->convertS8) {
        return (loq == LOQ0) ? plane->temporalBufferU8.data : plane->basePixelsU8.data;
    }

    return (loq == LOQ0) ? plane->temporalBuffer[0].data : plane->basePixels.data;
}

VN_DEC_CORE_API int VN_CALLCONV perseus_decoder_set_live_config(perseus_decoder const decoder,
                                                                perseus_decoder_live_config cfg)
{
    Context_t* ctx = decoder ? decoder->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    const bool oldConvertS8 = ctx->convertS8;

    switch (cfg.format) {
        case PSS_SURFACE_U8: ctx->convertS8 = true; break;
        case PSS_SURFACE_S16:
        default: ctx->convertS8 = false; break;
    }

    ctx->generateSurfaces = cfg.generate_surfaces;
    ctx->disableTemporalApply = cfg.generate_surfaces;
    ctx->useExternalSurfaces = cfg.use_external_buffers;

    if (ctx->generateSurfaces && oldConvertS8 != ctx->convertS8) {
        contextExternalSurfacesPrepare(ctx);
    }

    return 0;
}

VN_DEC_CORE_API int32_t VN_CALLCONV perseus_decoder_decode(perseus_decoder const decoder,
                                                           const perseus_image* fullImage,
                                                           const perseus_image* baseImage)
{
    int32_t ret = 0;
    Context_t* ctx = decoder ? decoder->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    profilerTickStart(ctx->profiler);
    VN_PROFILE_FUNCTION();

    perseus_image loq2Target;
    const perseus_image* loq1BaseImage = NULL;

    if (ctx->deserialised.scalingModes[LOQ1] != Scale0D) {
        int32_t plane_idx;

        /* Ensure that the intermediate surfaces have been prepared for
           upscaling into.
           @todo: Support more exotic formats (requires larger refactor of
                  API and internal mechanisms.
         */
        ret = contextLOQ2TargetSurfacePrepare(ctx, ctx->memory, ctx->log);
        if (ret != 0) {
            return ret;
        }

        memset(&loq2Target, 0, sizeof(perseus_image));
        loq2Target.ilv = PSS_ILV_NONE;
        loq2Target.depth = baseImage->depth;

        for (plane_idx = 0; plane_idx < 3; ++plane_idx) {
            loq2Target.plane[plane_idx] = ctx->planes[plane_idx].loq2UpsampleTarget.data;
            loq2Target.stride[plane_idx] = ctx->planes[plane_idx].loq2UpsampleTarget.stride;
        }

        ret = perseus_decoder_upscale(decoder, &loq2Target, baseImage, PSS_LOQ_2);
        if (ret != 0) {
            return ret;
        }

        loq1BaseImage = &loq2Target;
    } else {
        loq1BaseImage = baseImage;
    }

    ret = perseus_decoder_decode_base(decoder, loq1BaseImage);
    if (ret != 0) {
        return ret;
    }

    ret = perseus_decoder_upscale(decoder, fullImage, loq1BaseImage, PSS_LOQ_1);
    if (ret != 0) {
        return ret;
    }

    ret = perseus_decoder_decode_high(decoder, fullImage);
    if (ret != 0) {
        return ret;
    }

    if ((ret = perseus_decoder_apply_s(decoder, fullImage)) != 0) {
        return ret;
    }

    ctx->started = 0;

    VN_PROFILE_STOP();
    profilerTickStop(ctx->profiler);

    if (ret != 0) {
        return -1;
    }

    return 0;
}

VN_DEC_CORE_API int perseus_decoder_get_upsample_kernel(perseus_decoder const decoder,
                                                        perseus_kernel* kernelOut,
                                                        perseus_upsample upsampleMethod)
{
    Context_t* ctx = decoder ? decoder->m_context : NULL;

    if (!ctx || !kernelOut) {
        return -1;
    }

    const UpscaleType_t type = upscaleTypeFromAPI(upsampleMethod);

    Kernel_t kernelInternal;
    if (!upscaleGetKernel(ctx->log, ctx, type, &kernelInternal)) {
        return -1;
    }

    memcpy(kernelOut->k, kernelInternal.coeffs, sizeof(kernelInternal.coeffs));
    kernelOut->len = kernelInternal.length;
    kernelOut->is_pre_baked_pa = kernelInternal.isPreBakedPA;

    return 0;
}

VN_DEC_CORE_API void VN_CALLCONV perseus_decoder_get_last_error(perseus_decoder const decoder,
                                                                perseus_error_codes* code,
                                                                const char** const message)
{
    if (code) {
        *code = PSS_PERSEUS_UNKNOWN_ERROR;
    }
    if (message) {
        *message = "Error functionality is deprecated, API will be removed";
    }
}

VN_DEC_CORE_API void perseus_decoder_debug(perseus_decoder const decoder, perseus_debug_mode mode)
{
    Context_t* ctx = decoder ? decoder->m_context : NULL;

    if (!ctx) {
        return;
    }

    const bool enable = (mode == HIGHLIGHT_RESIDUALS) ? true : false;
    for (int32_t i = 0; i < LOQEnhancedCount; ++i) {
        ctx->highlightState[i].enabled = enable;
    }
}

VN_DEC_CORE_API int perseus_decoder_get_num_residual_planes(perseus_decoder const decoder)
{
    Context_t* ctx = decoder ? decoder->m_context : NULL;
    if (!ctx) {
        return -1;
    }
    return ctx->deserialised.numPlanes;
}

VN_DEC_CORE_API int perseus_decoder_get_num_tiles(perseus_decoder const decoder, int plane_idx,
                                                  perseus_loq_index loq_idx)
{
    const Context_t* ctx = decoder ? decoder->m_context : NULL;
    if (!ctx) {
        return -1;
    }
    const LOQIndex_t loq = loqIndexFromAPI(loq_idx);
    return ctx->deserialised.tileCount[plane_idx][loq];
}

VN_DEC_CORE_API int perseus_decoder_get_apply_cmd_buffer_threads(perseus_decoder decoder)
{
    const Context_t* ctx = decoder ? decoder->m_context : NULL;
    if (!ctx) {
        return -1;
    }
    return ctx->applyCmdBufferThreads;
}

VN_DEC_CORE_API int perseus_decoder_get_cmd_buffer(perseus_decoder decoder, perseus_loq_index loq,
                                                   int planeIdx, int tileIdx, perseus_cmdbuffer* buffer,
                                                   perseus_cmdbuffer_entrypoint* entrypoints,
                                                   int numEntrypoints)
{
    Context_t* ctx = decoder ? decoder->m_context : NULL;

    if (!ctx) {
        return -1;
    }

    if (loq != PSS_LOQ_0 && loq != PSS_LOQ_1) {
        return -1;
    }

    if (!buffer) {
        VN_ERROR(ctx->log, "Calling error: buffer must be a valid pointer\n");
        return -1;
    }

    const CmdBuffer_t* src = NULL;

    if (ctx->useParallelDecode) {
        src = decodeParallelGetCmdBuffer(ctx->decodeParallel[loq], planeIdx, (uint8_t)tileIdx);
    } else {
        src = decodeSerialGetCmdBuffer(ctx->decodeSerial[loq], (uint8_t)planeIdx, (uint8_t)tileIdx);
    }

    if (src == NULL) {
        VN_ERROR(ctx->log, "Failed to determine correct source command buffer\n");
        return -1;
    }

    buffer->type = (ctx->deserialised.transform == TransformDDS) ? PSS_CBT_4x4 : PSS_CBT_2x2;
    buffer->commands = (const uint8_t*)src->data.start;
    buffer->data = src->data.currentData;
    buffer->count = src->count;
    buffer->command_size = (uint32_t)cmdBufferGetCommandsSize(src);
    buffer->data_size = (uint32_t)cmdBufferGetDataSize(src);

    if (ctx->applyCmdBufferThreads > 1) {
        if (!entrypoints) {
            VN_ERROR(ctx->log, "Calling error: entrypoints must be a valid pointer\n");
            return -1;
        }
        if (numEntrypoints < ctx->applyCmdBufferThreads) {
            VN_ERROR(ctx->log, "Calling error: an array of %u entrypoints are required\n",
                     ctx->applyCmdBufferThreads);
            return -1;
        }
        uint16_t entryPointIndex = 0;
        for (; entryPointIndex < ctx->applyCmdBufferThreads; entryPointIndex++) {
            const CmdBufferEntryPoint_t* internalEntryPoint = NULL;
            if (ctx->useParallelDecode) {
                internalEntryPoint = decodeParallelGetCmdBufferEntryPoint(
                    ctx->decodeParallel[loq], (uint8_t)planeIdx, (uint8_t)tileIdx, entryPointIndex);
            } else {
                internalEntryPoint = decodeSerialGetCmdBufferEntryPoint(
                    ctx->decodeSerial[loq], (uint8_t)planeIdx, (uint8_t)tileIdx, entryPointIndex);
            }
            entrypoints[entryPointIndex].count = (int32_t)internalEntryPoint->count;
            entrypoints[entryPointIndex].initial_jump = internalEntryPoint->initialJump;
            entrypoints[entryPointIndex].command_offset = internalEntryPoint->commandOffset;
            entrypoints[entryPointIndex].data_offset = internalEntryPoint->dataOffset;
        }
        // In case more entrypoints are given than the configured threads, a invalid count is set.
        for (; entryPointIndex < numEntrypoints; entryPointIndex++) {
            entrypoints[entryPointIndex].count = -1;
        }
    } else if (entrypoints != NULL && numEntrypoints >= ctx->applyCmdBufferThreads) {
        entrypoints[0].count = buffer->count;
        entrypoints[0].initial_jump = 0;
        entrypoints[0].command_offset = 0;
        entrypoints[0].data_offset = 0;
        for (int32_t entryPointIndex = 1; entryPointIndex < numEntrypoints; entryPointIndex++) {
            entrypoints[entryPointIndex].count = -1;
        }
    } else {
        entrypoints = NULL;
    }

    return 0;
}

VN_DEC_CORE_API uint8_t perseus_get_bitdepth(perseus_bitdepth depth)
{
    switch (depth) {
        case PSS_DEPTH_8: return 8;
        case PSS_DEPTH_10: return 10;
        case PSS_DEPTH_12: return 12;
        case PSS_DEPTH_14: return 14;
    }
    return 0;
}

VN_DEC_CORE_API uint8_t perseus_get_bytedepth(perseus_bitdepth depth)
{
    return (perseus_get_bitdepth(depth) + 7) / 8;
}

VN_DEC_CORE_API uint8_t perseus_is_rgb(perseus_interleaving ilv)
{
    switch (ilv) {
        case PSS_ILV_RGB:
        case PSS_ILV_RGBA: return 1;
        case PSS_ILV_NONE:
        case PSS_ILV_NV12:
        case PSS_ILV_UYVY:
        case PSS_ILV_YUYV: break;
    }
    return 0;
}
