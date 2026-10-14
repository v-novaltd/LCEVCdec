/* Copyright (c) V-Nova International Limited 2026. All rights reserved.
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

const uint kDequantTransformDDLayerCount = 4;
const uint kDequantTransformDDSLayerCount = 16;
const uint kDequantTransformDdsGroupCount = 4;
const uint kDequantTransformScale1D = 1; // LdeScalingMode::Scale1D.

ivec4 dequantizeCoefficients(ivec4 coefficients, ivec4 stepWidths, ivec4 offsets)
{
    ivec4 results = coefficients * stepWidths;
    results += sign(coefficients) * offsets;
    return clamp(results, ivec4(-32768), ivec4(32767));
}

ivec4 dequantInverseTransformDD(ivec4 coefficients, ivec4 stepWidths, ivec4 offsets,
                                uint scalingMode)
{
    const ivec4 c = dequantizeCoefficients(coefficients, stepWidths, offsets);
    ivec4 residuals;

    if(scalingMode == kDequantTransformScale1D) {
        residuals.x = saturateS16(c.x + c.y + c.z);
        residuals.y = saturateS16(c.x - c.y - c.z);
        residuals.z = saturateS16(c.w + c.y - c.z);
        residuals.w = saturateS16(c.w - c.y + c.z);
    } else { // Scale0D, Scale2D
        residuals.x = saturateS16(c.x + c.y + c.z + c.w);
        residuals.y = saturateS16(c.x - c.y + c.z - c.w);
        residuals.z = saturateS16(c.x + c.y - c.z - c.w);
        residuals.w = saturateS16(c.x - c.y - c.z + c.w);
    }
    return residuals;
}

ivec4 dequantInverseTransformDDSFirstPass(ivec4 coefficients, ivec4 stepWidths, ivec4 offsets)
{
    const ivec4 c = dequantizeCoefficients(coefficients, stepWidths, offsets);
    return ivec4(c.x + c.y + c.z + c.w, c.x - c.y + c.z - c.w,
                 c.x + c.y - c.z - c.w, c.x - c.y - c.z + c.w);
}

ivec4 inverseTransformDDSSecondPass(ivec4 firstPass, uint scalingMode)
{
    ivec4 residuals;
    if(scalingMode == kDequantTransformScale1D) {
        residuals.x = saturateS16(firstPass.x + firstPass.y + firstPass.w);
        residuals.y = saturateS16(firstPass.x - firstPass.y - firstPass.w);
        residuals.z = saturateS16(firstPass.y + firstPass.z - firstPass.w);
        residuals.w = saturateS16(firstPass.z - firstPass.y + firstPass.w);
    } else { // Scale0D, Scale2D
        residuals.x = saturateS16(firstPass.x + firstPass.y + firstPass.z + firstPass.w);
        residuals.y = saturateS16(firstPass.x - firstPass.y + firstPass.z - firstPass.w);
        residuals.z = saturateS16(firstPass.x + firstPass.y - firstPass.z - firstPass.w);
        residuals.w = saturateS16(firstPass.x - firstPass.y - firstPass.z + firstPass.w);
    }
    return residuals;
}

int deblockResidual(int residual, uint coefficient)
{
    // Reproduce the CPU's unsigned multiplication and logical shift, then truncate to signed 16-bit.
    const uint scaled = (coefficient * uint(residual)) >> 4u;
    const int truncated = int(scaled & 0xffffu);
    return truncated >= 0x8000 ? truncated - 0x10000 : truncated;
}

void deblockDDS(inout ivec4 output0, inout ivec4 output1,
                inout ivec4 output2, inout ivec4 output3,
                uint corner, uint side)
{
    output0.x = deblockResidual(output0.x, corner);
    output0.y = deblockResidual(output0.y, side);
    output0.z = deblockResidual(output0.z, side);
    output1.x = deblockResidual(output1.x, side);
    output1.y = deblockResidual(output1.y, corner);
    output1.w = deblockResidual(output1.w, side);
    output2.x = deblockResidual(output2.x, side);
    output2.z = deblockResidual(output2.z, corner);
    output2.w = deblockResidual(output2.w, side);
    output3.y = deblockResidual(output3.y, side);
    output3.z = deblockResidual(output3.z, side);
    output3.w = deblockResidual(output3.w, corner);
}
