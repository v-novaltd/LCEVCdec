/* Copyright (c) V-Nova International Limited 2022-2025. All rights reserved.
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

#ifndef VN_LCEVC_PIXEL_PROCESSING_DITHER_H
#define VN_LCEVC_PIXEL_PROCESSING_DITHER_H

#include "stdbool.h"
#include "stdint.h"
#include "string.h"

#include <LCEVC/common/memory.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/random.h>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/pipeline/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! \file
 * \brief This file provides a simple dithering module.
 *
 * Dithering is a method of introducing noise into a decoded image to provide an
 * apparent image sharpening.
 *
 * For LCEVC dithering controls are signaled within the bit-stream: - these are
 * strength and type and can change on a per-frame basis.
 *
 * # Dither Strength
 * For strength the value is the maximum amount (+/-) that a pixel may be perturbed by
 * the random noise.
 *
 * # Dither Type
 * Currently there is:
 *     - None (i.e. disabled)
 *     - Uniform (i.e. uniformly random).
 *
 * In our implementation we opt to use a xor-shift-rotate algorithm for performance
 * reasons, we have not performed analysis to determine if it is uniform, but it
 * does appear to provide subjectively sound noise.
 *
 * # Usage
 * As mutiple frames may be decoded in paralel each with their own required strength
 * a global buffer of unscaled entropy is shared between threads this entropy
 * is scaled down to the frames's strength at application of dithering.
 *
 * At the start of each slice we generate a random offset into the buffer to avoid
 * repeating patterns
 *
 * ldppDitherGlobalInitialize is used to initalise the global entropy buffer with a
 * particular seed if desired.
 *
 * This global buffer can then be passed to ldppDitherFrameInitialise which
 * also stores the dithering strength for a particular frame along with a unique
 * seed for that frame.
 *
 * As it is possible to apply dithering to multiple slices of the same frame
 * in paralell, a unique seed for each slice is created from the slice's vertical
 * offset and plane index via ldppDitherSliceInitialise. This seed initalises the
 * RNG used to create random offsets into the entropy buffer. This stops the dither
 * pattern repeating at slice boundaries.
 *
 * The offset into the entropy buffer for a particular slice is retrevied via
 * ldppDitherGetBuffer. The entropy values can be scaled down and applied to
 * incoming pixel values using the inline helpers: ldppDitherApply(SSE/NEON)
 */
/*------------------------------------------------------------------------------*/

/*! Structure to hold global entropy buffer
 */
typedef struct LdppDitherGlobal
{
    LdcMemoryAllocator* allocator;
    LdcMemoryAllocation allocationBuffer;
    uint16_t* buffer;
} LdppDitherGlobal;

/*! Structure to hold per-frame dithering information.
 */
typedef struct LdppDitherFrame
{
    LdppDitherGlobal* global;
    uint64_t frameSeed;
    uint8_t strength;
} LdppDitherFrame;

/*! Structure to hold per-slice dithering information and RNG for buffer offsets
 */
typedef struct LdppDitherSlice
{
    LdppDitherGlobal* global;
    LdcRandom random;
    uint8_t strength;
} LdppDitherSlice;

/*------------------------------------------------------------------------------*/

/*! Initializes the global dither module
 *
 * \param allocator        The memory allocator.
 * \param dither           The dither module to initialize.
 * \param seed             The seed to initialize the random number generator
 *                         with, if 0 it will use time()
 *
 * \return false if memory allocation failed, otherwise true.
 */
bool ldppDitherGlobalInitialize(LdcMemoryAllocator* allocator, LdppDitherGlobal* dither, uint64_t seed);

/*! Releases the dither module and any associated memory. */
void ldppDitherGlobalRelease(LdppDitherGlobal* dither);

/*! Initializes the per-frame dither module
 *
 * \param dither           The frame dither module to initialize.
 * \param global           The global dither module to retrieve entropy from.
 * \param seed             The seed for this particular frame, this should be unique
 *                         for each frame to avoid repitition, the frames timestamp is
 *                         a ideal seed. if 0 it will use time()
 * \param global           The dithering strength for this particular frame
 *
 * \return false if dithering strength is greater than 31 otherwise true.
 */
bool ldppDitherFrameInitialise(LdppDitherFrame* frame, LdppDitherGlobal* global, uint64_t seed,
                               uint8_t strength);

/*! Initializes the per-slice dither module
 *
 * \param slice            The slice dither module to initialize.
 * \param frame            The frame dither module to retrieve entropy and strength from.
 * \param offset           The offset into the frame for this particular slice *
 * \param planeIndex       The index of the plane of which this slice belongs to
 *                         this is combined with the frame seed along the offset above
 *                         to create a unique seed per slice for buffer offsets
 */
void ldppDitherSliceInitialise(LdppDitherSlice* slice, LdppDitherFrame* frame, uint32_t offset,
                               uint32_t planeIndex);

/*! Query the local dithering module for a pointer to an array of
 *  values that contain unscaled random noise that is at least `length`
 *  elements long.
 *
 * \param dither  The slice dither module.
 * \param length  The number of elements to query.
 *
 * \return A valid pointer on success, otherwise NULL, noting if `length`
 *         is greater than 16384 the function will return NULL.
 */
const uint16_t* ldppDitherGetBuffer(LdppDitherSlice* dither, size_t length);

/*! Query the bitshift required for a signed, fixed point pixel
 *
 *  If the pixel is a unsigned type the function will return zero
 *
 * \param bitDepth  The bitdepth of the pixel
 *
 * \return The number of bits to shift the dither value by
 *         depending on the bitdepth of the pixel.
 */
int8_t ldppDitherGetShiftS16(LdpFixedPoint bitDepth);

/*------------------------------------------------------------------------------*/

#include "detail/apply_dither_neon.h"
#include "detail/apply_dither_scalar.h"
#include "detail/apply_dither_sse.h"

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_PIXEL_PROCESSING_DITHER_H
