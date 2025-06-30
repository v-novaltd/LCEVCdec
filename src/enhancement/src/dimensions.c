/* Copyright (c) V-Nova International Limited 2025. All rights reserved.
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

#include <LCEVC/common/limit.h>
#include <LCEVC/enhancement/bitstream_types.h>
#include <LCEVC/enhancement/config_parser.h>
#include <LCEVC/enhancement/dimensions.h>

void ldeTileDimensionsFromConfig(const LdeGlobalConfig* globalConfig, LdeLOQIndex loq,
                                 uint16_t planeIdx, uint16_t tileIdx, uint16_t* width, uint16_t* height)
{
    uint16_t planeWidth = 0;
    uint16_t planeHeight = 0;
    ldePlaneDimensionsFromConfig(globalConfig, loq, planeIdx, &planeWidth, &planeHeight);

    const uint16_t tileWidth = globalConfig->tileWidth[planeIdx];
    const uint16_t tileHeight = globalConfig->tileHeight[planeIdx];

    uint16_t tilesAcross = divideCeilU16(planeWidth, globalConfig->tileWidth[planeIdx]);
    const uint16_t tileIndexX = tileIdx % tilesAcross;
    const uint16_t tileIndexY = tileIdx / tilesAcross;

    *width = minU16(tileWidth, planeWidth - (tileIndexX * tileWidth));
    *height = minU16(tileHeight, planeHeight - (tileIndexY * tileHeight));
}

void ldeTileStartFromConfig(const LdeGlobalConfig* globalConfig, LdeLOQIndex loq, uint16_t planeIdx,
                            uint16_t tileIdx, uint16_t* startX, uint16_t* startY)
{
    if (tileIdx == 0) {
        *startX = 0;
        *startY = 0;
    } else {
        uint16_t planeWidth = 0;
        uint16_t planeHeight = 0;
        ldePlaneDimensionsFromConfig(globalConfig, loq, planeIdx, &planeWidth, &planeHeight);
        uint16_t tileWidth = 0;
        uint16_t tileHeight = 0;
        ldeTileDimensionsFromConfig(globalConfig, loq, planeIdx, 0, &tileWidth, &tileHeight);
        uint16_t tilesAcross = divideCeilU16(planeWidth, globalConfig->tileWidth[planeIdx]);
        *startX = (tileIdx % tilesAcross) * tileWidth;
        *startY = (tileIdx / tilesAcross) * tileHeight;
    }
}

void ldePlaneDimensionsFromConfig(const LdeGlobalConfig* globalConfig, const LdeLOQIndex loq,
                                  uint16_t const planeIdx, uint16_t* width, uint16_t* height)
{
    uint16_t calcWidth = globalConfig->width;
    uint16_t calcHeight = globalConfig->height;

    /* Scale to the correct LOQ. */
    for (uint8_t loqIdx = 0; loqIdx < loq; ++loqIdx) {
        const LdeScalingMode loqScalingMode = globalConfig->scalingModes[loqIdx];

        if (loqScalingMode != Scale0D) {
            calcWidth = (calcWidth + 1) >> 1;

            if (loqScalingMode == Scale2D) {
                calcHeight = (calcHeight + 1) >> 1;
            }
        }
    }

    /* Scale to correct plane */
    if (planeIdx > 0) {
        const LdeChroma chroma = globalConfig->chroma;

        if (chroma == CT420 || chroma == CT422) {
            calcWidth = (calcWidth + 1) >> 1;

            if (chroma == CT420) {
                calcHeight = (calcHeight + 1) >> 1;
            }
        }
    }

    *width = calcWidth;
    *height = calcHeight;
}

uint32_t ldeTotalNumTiles(const LdeGlobalConfig* globalConfig)
{
    uint32_t totalTiles = 0;
    for (uint8_t planeIdx = 0; planeIdx < globalConfig->numPlanes; ++planeIdx) {
        for (int8_t loq = LOQ1; loq >= LOQ0; --loq) {
            totalTiles += globalConfig->numTiles[planeIdx][loq];
        }
    }

    return totalTiles;
}
