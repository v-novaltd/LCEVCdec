/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#include "surface/overlay.h"

#include <images.h>
#include <lcevcLogo.h>

/* @todo(bob): Overlay is basically a blit and so should be replaced with it. */

typedef void (*ApplyOverlay_t)(const uint8_t* src, uint8_t* dst);

/* Target percentage of width that overlay should occupy */
const int32_t kOverlayWidthPercentage = 6;

static void applyOverlayU8(const uint8_t* src, uint8_t* dst)
{
    int32_t temp = *dst + *src;
    *dst = saturateU8(temp);
}

static void applyOverlayU10(const uint8_t* src, uint8_t* dst)
{
    uint16_t* dst16 = (uint16_t*)dst;
    int32_t temp = *dst16 + (*src << 2);
    *dst16 = saturateUN(temp, (1 << 10) - 1);
}

static void applyOverlayU12(const uint8_t* src, uint8_t* dst)
{
    uint16_t* dst16 = (uint16_t*)dst;
    int32_t temp = *dst16 + (*src << 4);
    *dst16 = saturateUN(temp, (1 << 12) - 1);
}

static void applyOverlayU14(const uint8_t* src, uint8_t* dst)
{
    uint16_t* dst16 = (uint16_t*)dst;
    int32_t temp = *dst16 + (*src << 2);
    *dst16 = saturateUN(temp, (1 << 14) - 1);
}

static void applyOverlayS16(const uint8_t* src, uint8_t* dst)
{
    int16_t* dst16 = (int16_t*)dst;
    int32_t temp = *dst16 + fpU8ToS8(*src);
    *dst16 = saturateS16(temp);
}

static const ApplyOverlay_t kTable[FPCount] = {
    applyOverlayU8,  applyOverlayU10, applyOverlayU12, applyOverlayU14,
    applyOverlayS16, applyOverlayS16, applyOverlayS16, applyOverlayS16,
};

static const StaticImageDesc_t* getBestSizeImage(const StaticImageDesc_t* images[],
                                                 uint8_t imagesCount, int32_t targetWidth)
{
    uint16_t bestDifference = USHRT_MAX;
    const StaticImageDesc_t* bestImage = NULL;

    for (int32_t i = 0; i < imagesCount; i++) {
        const int32_t width = (int32_t)images[i]->header.w;
        const uint16_t difference = (uint16_t)abs(targetWidth - width);

        if (difference < bestDifference) {
            bestDifference = difference;
            bestImage = images[i];
        }
    }

    return bestImage;
}

static const StaticImageDesc_t* getOverlaySource(int32_t targetWidth)
{
    static const StaticImageDesc_t* images[] = {
        &lcevcLogo_230x77, &lcevcLogo_201x67, &lcevcLogo_172x57, &lcevcLogo_143x48,
        &lcevcLogo_115x38, &lcevcLogo_100x33, &lcevcLogo_86x28,  &lcevcLogo_71x24,
        &lcevcLogo_57x19,  &lcevcLogo_50x16,  &lcevcLogo_43x14,  &lcevcLogo_35x12,
        &lcevcLogo_28x9,   &lcevcLogo_25x8,   &lcevcLogo_21x7,   &lcevcLogo_17x6,
    };

    return getBestSizeImage(images, sizeof(images) / sizeof(images[0]), targetWidth);
}

int32_t overlayApply(Context_t* ctx, const OverlayArgs_t* params)
{
    const Surface_t* surf = params->dst;
    FixedPoint_t fp = surf->type;
    const uint8_t numBytes = (uint8_t)fixedPointByteSize(fp);
    ApplyOverlay_t applyPixel = kTable[fp];
    const int32_t overlaySrcTargetWidth = (int32_t)(surf->width * kOverlayWidthPercentage) / 100;
    const StaticImageDesc_t* overlaySrc = getOverlaySource(overlaySrcTargetWidth);
    const uint32_t yMax = surf->height;
    const uint32_t xMax = surf->width;
    const size_t srcHeight = (size_t)overlaySrc->header.h;
    const size_t srcWidth = (size_t)overlaySrc->header.w;
    const size_t dstOffset =
        ctx->logoOverlayPositionX + srcWidth > xMax ? xMax - srcWidth : ctx->logoOverlayPositionX;

    size_t dstY =
        ctx->logoOverlayPositionY + srcHeight > yMax ? yMax - srcHeight : ctx->logoOverlayPositionY;

    if (applyPixel == NULL) {
        VN_ERROR(ctx->log, "Could not find function to apply overlay to pixel type %s\n",
                 fixedPointToString(fp));
        return -1;
    }
    if (srcHeight > yMax) {
        VN_ERROR(ctx->log, "Overlay is too tall (%u pixels) to fit in frame (%u pixels)\n", srcHeight, yMax);
        return -1;
    }
    if (srcWidth > xMax) {
        VN_ERROR(ctx->log, "Overlay is too wide (%u pixels) to fit in frame (%u pixels)\n", srcWidth, xMax);
        return -1;
    }

    for (size_t srcY = 0; srcY < srcHeight; srcY++, dstY++) {
        const uint8_t* src = &overlaySrc->data[srcY * srcWidth];
        uint8_t* dst = surfaceGetLine(surf, (uint32_t)dstY) + dstOffset * numBytes;

        for (size_t srcX = 0; srcX < srcWidth; srcX++, src++, dst += numBytes) {
            applyPixel(src, dst);
        }
    }

    return 0;
}

bool overlayIsEnabled(const Context_t* ctx) { return ctx->useLogoOverlay; }