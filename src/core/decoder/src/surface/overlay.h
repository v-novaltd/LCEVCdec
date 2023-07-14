/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_OVERLAY_H_
#define VN_DEC_CORE_OVERLAY_H_

#include "common/types.h"
#include "context.h"

#define VN_OVERLAY_MAX_DELAY() 750

typedef struct OverlayArgs
{
    const Surface_t* dst;
} OverlayArgs_t;

/*! \brief apply overlay to an image */
int32_t overlayApply(Context_t* ctx, const OverlayArgs_t* params);

/*! \brief determines whether overlay should be applied. */
bool overlayIsEnabled(const Context_t* ctx);

#endif /* VN_DEC_CORE_OVERLAY_H_ */
