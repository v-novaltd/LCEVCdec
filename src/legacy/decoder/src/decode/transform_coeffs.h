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

#ifndef VN_LCEVC_LEGACY_TRANSFORM_COEFFS_H
#define VN_LCEVC_LEGACY_TRANSFORM_COEFFS_H

#include "common/platform.h"
#include "common/threading.h"

/*! \file
 *
 * This file contains an implementation for decoding compressed transform coefficients
 * and their interspersed run-lengths into an intermediate representation that is
 * easier to process and decouples the expensive entropy decoding step from the simpler
 * residual calculation step.
 *
 * The general premise is that there are up to 17 independent chunks of compressed
 * data depending on the transform type, and whether temporal is enabled. This module
 * performs entropy decoding on each of these chunks in parallel, storing the decoded
 * resulting data in an intermediate buffer.
 *
 * The intent of this implementation is to try and improve throughput at the cost
 * of increased memory utilization & bandwidth of relatively small data sets.
 *
 * For each compressed coefficient the intermediate buffer stores a coefficient and
 * zero-value run-length pair where the run-length is the number of zeros that immediately
 * follow the coefficient. This run-length was either signaled in the compressed data
 * immediately after the coefficient, or if it wasn't (because the stream contains
 * multiple signals in neighboring TUs) a zero-value run-length of 0 is inserted.
 * This helps with simplifying the residual calculations during cmdbuffer generation.
 *
 * The pairs are not stored contiguously with each other. But rather in 2 separate
 * contiguous arrays, this is because they have different sized data types. A user
 * must access both buffers with the same index to reconstruct the pair.
 *
 * The first coefficient within the intermediate buffer is always assumed to start in
 * the top-left coordinate of the transform layer for the tile being processed, knowing
 * this with the symbols and run-lengths allows a user to keep track of the transform
 * index for each layer and hence the overall coordinates on the destination surface.
 *
 * The intermediate representation is backed by a dynamically growing buffer, it is
 * recommended to cache instances of `TransformCoeff_t` to prevent unnecessary
 * allocations - although a user should beware that the buffers do not contract.
 */

/*------------------------------------------------------------------------------*/

typedef struct Chunk Chunk_t;
typedef struct CmdBuffer CmdBuffer_t;
typedef struct Context Context_t;
typedef struct Logger* Logger_t;
typedef struct Memory* Memory_t;
typedef struct ThreadManager ThreadManager_t;
typedef struct TUState TUState_t;

/*------------------------------------------------------------------------------*/

/*! \brief Opaque handle used for decoding and storing coefficient
 *         data in a dynamically expanding memory buffer. */
typedef struct TransformCoeffs* TransformCoeffs_t;

/*! \brief Enum for temporal data signal.
 *
 *  This signal is specialized to differentiate between an intra
 *  transform, and an intra 32x32 block - the command buffer
 *  generation code takes advantage of the knowledge about
 *  the intra block clears to help skip more TUs. */
typedef enum TemporalCoeff
{
    TCInter = 0,
    TCIntra = 1,
    TCIntraBlock = 2,
} TemporalCoeff_t;

typedef struct BlockClearJumps
{
    Memory_t memory;
    uint32_t* jumps;
    uint32_t count;
    uint32_t capacity;
    bool error;
} * BlockClearJumps_t;

/* clang-format off */

/*! \brief Helper struct containing arguments necessary for
 *         decoding the transform coeffs */
typedef struct TransformCoeffsDecodeArgs
{
    Logger_t           log;
    ThreadManager_t    threadManager;
    Chunk_t*           chunks;                       /**< Array of chunks for each transform layer to decode from. */
    Chunk_t*           temporalChunk;                /**< Single chunk for the temporal signal to decode from. */
    TransformCoeffs_t* coeffs;                       /**< Array of transform coeffs for each transform layer to decode into. */
    TransformCoeffs_t  temporalCoeffs;               /**< Single transform coeff for decoding temporal data into. */
    int32_t            chunkCount;                   /**< Number of `chunks` to decode. */
    TUState_t*         tuState;                      /**< Transform unit state used for calculated co-ordinates. */
    bool               temporalUseReducedSignalling; /**< Indicates that the temporal data is compressed with reduced signaling. */
    BlockClearJumps_t* blockClears;                  /**< Temporary dynamic array to store block clear locations when building cmdbuffers */
    uint8_t            bitstreamVersion;             /**< Version of the bitstream (streams without versions are always on "current version"). */
} TransformCoeffsDecodeArgs_t;

/*! \brief Contains 2 lists of coefficients and runs respectively.
 *
 * A single index into both lists forms a pair containing a single
 * coefficient and a zero run length. The first entry in coeffs is
 * assumed to start at the top-left of the tile being decoded.
 */
typedef struct TransformCoeffsData
{
    const int16_t*  coeffs; /**< Contiguous array of coefficients containing `count` entries. */
    const uint32_t* runs;   /**< Contiguous array of runs containing `count` entries. */
    uint32_t        count;  /**< Number of entries in both arrays. */
} TransformCoeffsData_t;

/* clang-format on */

/*------------------------------------------------------------------------------*/

bool blockClearJumpsInitialize(Memory_t memory, BlockClearJumps_t* blockClear);

void blockClearJumpsRelease(BlockClearJumps_t blockClear);

/*! Creates a new `TransformCoeffs_t` instance ready for decoding into.
 *
 *  This only needs to be performed once for each instance. This function
 *  preallocates some memory to write coefficients and run length data into.
 *
 *  If this function returns true, the user must call `transformCoeffsRelease`,
 *  otherwise memory will leak.
 *
 * \return false upon allocation an error, otherwise true.
 */
bool transformCoeffsInitialize(Memory_t memory, TransformCoeffs_t* coeffs);

/*! Releases any memory associated with this instance of `TransformCoeffs_t`
 *  including the instance itself. */
void transformCoeffsRelease(TransformCoeffs_t coeffs);

/*! Retrieves buffers for the coefficients and runs for an instance of `TransformCoeffs_t`
 *  that has been decoded into.
 *
 *  It is only valid to call this after calling `transformCoeffsDecode` with
 *  the same instance.
 *
 *  The pointers returned by this function are only valid until the next
 *  call to `transformCoeffsDecode` or `transformCoeffsRelease`. */
TransformCoeffsData_t transformCoeffsGetData(TransformCoeffs_t coeffs);

/*! Entropy decodes coefficients and temporal signals from compressed chunks
 *  into an intermediate format stored on `TransformCoeffs_t`.
 *
 * \return True on success, False if there was an error decoding any of
 *         the compressed data. */
bool transformCoeffsDecode(TransformCoeffsDecodeArgs_t* args);

/*------------------------------------------------------------------------------*/

#endif // VN_LCEVC_LEGACY_TRANSFORM_COEFFS_H
