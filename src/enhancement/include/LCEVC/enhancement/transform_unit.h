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

#ifndef VN_LCEVC_ENHANCEMENT_TRANSFORM_UNIT_H
#define VN_LCEVC_ENHANCEMENT_TRANSFORM_UNIT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! \file
 *
 * This file contains an implementation for dealing with traversing the transform
 * layouts in 2D space.
 *
 * A nominal LCEVC decoding loop will take advantage of the fact that residuals are
 * relatively sparsely laid out, and that the entropy encoded data is minimally
 * compressed with a run-length encoder where only the zero runs are tracked, as such
 * any decoding loop can trivially keep track and jump over large runs of zero value
 * transforms.
 *
 * The LCEVC standard provides 2 strategies for navigating the coefficient surfaces
 * during entropy coding.
 *
 * In this library they are referred to as `Surface Raster` and `Block Raster`, this
 * file aims to abstract the problem away from a decoding loop such that it only needs
 * to keep track of which transform "index" they are on and use this functionality to
 * determine the destination 2D coordinates of where this transforms residuals should
 * be applied.
 *
 * # Surface Raster
 *
 * Surface raster is the simplest mechanism, the destination coordinates are determined
 * through a linear memory order access pattern in the exact same way that the
 * destination surface memory is ordered, with the only caveat being that the step
 * size is a function of the transform size.
 *
 * # Block Raster
 *
 * Block raster is a little more complicated, the surface is divided into 32x32 blocks
 * where the transforms are navigated within the block in raster order, once the
 * last transform is processed within a block the navigation jumps top-left transform
 * of the next block along in raster order.
 *
 * For example the follow access pattern is observed:
 *
 *     This example has the follow properties:
 *     Surface size    64x64 pixels
 *     Block size:     32x32 pixels
 *     Transform size: 4x4 pixels
 *
 *     ---------------------------------------------------------------------
 *     |   0   1   2   3   4   5   6   7 |  64  65  66  67  68  69  70  71 |
 *     |   8   9  10  11  12  13  14  15 |  72  73  74  75  76  77  78  79 |
 *     |  16  17  18  19  20  21  22  23 |  80  81  82  83  84  85  86  87 |
 *     |  24  25  26  27  28  29  30  31 |  88  89  90  91  92  93  94  95 |
 *     |  32  33  34  35  36  37  38  39 |  96  97  98  99 100 101 102 103 |
 *     |  40  41  42  43  44  45  46  47 | 104 105 106 107 108 109 110 111 |
 *     |  48  49  50  51  52  53  54  55 | 112 113 114 115 116 117 118 119 |
 *     |  56  57  58  59  60  61  62  63 | 120 121 122 123 124 125 126 127 |
 *     ---------------------------------------------------------------------
 *     | 128 129 130 131 132 133 134 135 | 192 193 194 195 196 197 198 199 |
 *     | 136 137 138 139 140 141 142 143 | 200 201 202 203 204 205 206 207 |
 *     | 144 145 146 147 148 149 150 151 | 208 209 210 211 212 213 214 215 |
 *     | 152 153 154 155 156 157 158 159 | 216 217 218 219 220 221 222 223 |
 *     | 160 161 162 163 164 165 166 167 | 224 225 226 227 228 229 230 231 |
 *     | 168 169 170 171 172 173 174 175 | 232 233 234 235 236 237 238 239 |
 *     | 176 177 178 179 190 181 182 183 | 240 241 242 243 244 245 246 247 |
 *     | 184 185 186 187 188 189 190 191 | 248 249 250 251 252 253 254 255 |
 *     ---------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------*/

typedef struct Chunk Chunk;
typedef struct CmdBufferCpu CmdBufferCpu;

/*------------------------------------------------------------------------------*/

/* TUState stores transform unit information. A transform unit is either 4 (DD) or 16 (DDS)
 * coefficients, which are then transformed into residuals. */
typedef struct TUState
{
    uint32_t tuTotal;   /**< The total number of TUs in the whole surface*/
    uint32_t numAcross; /**< The width of the surface, in TUs. */
    uint32_t xOffset;
    uint32_t yOffset;
    uint8_t tuWidthShift; /**< Width of the TU, log2. E.g. DDS is 4x4, so width is 4, shift is 2 */

    struct BlockArgs
    {
        uint32_t tuPerBlockBottomEdge;    /**< Number of TUs in the bottom edge block. */
        uint32_t tuPerBlockRowRightEdge;  /**< Number of TUs in a right edge blocks row. */
        uint32_t tuPerBlockColBottomEdge; /**< Number of TUs in a bottom edge blocks column. */
        uint32_t tuPerRow; /**< Number of TUs in a whole row of blocks (including row edge block). */
        uint32_t wholeBlocksPerRow; /**< Number of full blocks in a row */
        uint32_t wholeBlocksPerCol; /**< Number of full blocks in a column */
        uint32_t blocksPerRow;      /**< Number of blocks in a row */
        uint32_t blocksPerCol;      /**< Number of blocks in a column */
        uint16_t tuPerBlock;        /**< Number of TUs in whole block. 64 for DDS, 256 for DD */
        uint8_t tuPerBlockDims; /**< Number of TUs across or down in whole blocks. 8 for DDS, 16 for DD*/
        uint8_t tuPerBlockDimsShift; /**< log2(tuPerBlockDims). shift by this instead of multiplying/dividing by tuPerBlockDims.*/
        uint8_t tuPerBlockShift; /**< log2(tuPerBlock). shift by this instead of multiplying/dividing by tuPerBlock.*/
        uint32_t maxWholeBlockTu; /**< TU Index above which tuPerBlockBottomEdge applies */
    } block;
    struct BlockAlignedArgs
    {
        uint32_t tuPerRow;       /**< Number of TUs in a whole aligned row */
        uint32_t maxWholeBlockY; /**< Y position of the lowest whole block */
    } blockAligned;
} TUState;

typedef enum TuStateReturn
{
    TUMore,
    TUComplete,
    TUError,
} TuStateReturn;

/*! \brief Setup a TUState based upon a region and tuSize.
 *
 * \param state        The state to initialize
 * \param width        Plane width in pixels
 * \param height       Plane height in pixels
 * \param xOffset      X-axis offset from left in pixels, for tiles
 * \param yOffset      Y-axis offset from top in pixels, for tiles
 * \param tuWidthShift The size of the transform, 1 for DD, 2 for DDS
 *
 * \return True on success, otherwise false.
 */
bool ldeTuStateInitialize(TUState* state, uint32_t width, uint32_t height, uint32_t xOffset,
                          uint32_t yOffset, uint8_t tuWidthShift);

/*! \brief Given a transform index calculate the absolute destination surface coordinates
 *         using the surface raster access pattern.
 *
 * \param state    The state to use
 * \param tuIndex  The transform unit index
 * \param x        Location to write the absolute x coordinate
 * \param y        Location to write the absolute y coordinate
 *
 * \return TUMore if it's not the last TU in the state, TUComplete for the last TU and TUError
 *         if the requested tuIndex is past the end of the plane bounded by state
 */
TuStateReturn ldeTuCoordsSurfaceRaster(const TUState* state, uint32_t tuIndex, uint32_t* x, uint32_t* y);

/*! \brief Given an x, y coordinate within a surface raster ordered plane, returns the TU index
 *
 * \param state    The state to use
 * \param x        The x coordinate
 * \param y        The y coordinate
 *
 * \return TU index of the coordinate
 */
uint32_t ldeTuCoordsSurfaceIndex(const TUState* state, uint32_t x, uint32_t y);

/*! \brief Given a transform index calculate the absolute destination surface coordinates
 *         using the surface raster access pattern where the dimensions of the surface are rounded
 *         up to the nearest 32px.
 *
 * \param state    The state to use
 * \param tuIndex  The transform unit index
 * \param x        Location to write the absolute x coordinate
 * \param y        Location to write the absolute y coordinate
 */
void ldeTuCoordsBlockAlignedRaster(const TUState* state, uint32_t tuIndex, uint32_t* x, uint32_t* y);

/*! \brief Given a transform index calculate the absolute destination surface coordinates
 *         using the surface raster access pattern.
 *
 * \param state    The state to use
 * \param tuIndex  The transform unit index
 * \param x        Location to write the absolute x coordinate
 * \param y        Location to write the absolute y coordinate
 *
 * \return TUMore if it's not the last TU in the state, TUComplete for the last TU and TUError
 *         if the requested tuIndex is past the end of the plane bounded by state
 */
TuStateReturn ldeTuCoordsBlockRaster(const TUState* state, uint32_t tuIndex, uint32_t* x, uint32_t* y);

/*! \brief Given an x, y coordinate within a block ordered plane, returns the TU index where the
 *         dimensions of the surface are rounded up to the nearest 32x32 pixels (32 is BSTemporal)
 *
 * \param state    The state to use
 * \param x        The x coordinate
 * \param y        The y coordinate
 *
 * \return TU index of the coordinate */
uint32_t ldeTuCoordsBlockAlignedIndex(const TUState* state, uint32_t x, uint32_t y);

/*! \brief Given tuIndex within a block ordered plane, returns the TU index where the
 *         dimensions of the surface are rounded up to the nearest 32x32 pixels (32 is BSTemporal)
 *
 * \param state    The state to use
 * \param tuIndex      The tuIndex
 *
 * \return Block-aligned TU index */
uint32_t ldeTuIndexBlockAlignedIndex(const TUState* state, uint32_t tuIndex);

/*! \brief Obtains block details for the given pixel coordinate
 *
 * \param state        The state to use
 * \param tuIndex      The tuIndex within a block
 *
 * \return Number of transform units within the block. */
uint32_t ldeTuCoordsBlockTuCount(const TUState* state, uint32_t tuIndex);

/*! \brief Determines if the TU Index is at the start of a block
 *
 * \param state        The state to use
 * \param tuIndex      The tuIndex to check
 *
 * \return True if the TU is the first (top left) of a block. */
bool ldeTuIsBlockStart(const TUState* state, uint32_t tuIndex);

/*------------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif // VN_LCEVC_ENHANCEMENT_TRANSFORM_UNIT_H
