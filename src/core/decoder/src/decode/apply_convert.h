/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_APPLY_CONVERT_H_
#define VN_DEC_CORE_APPLY_CONVERT_H_

#include "common/platform.h"
#include "common/types.h"

/*------------------------------------------------------------------------------*/

typedef struct Surface Surface_t;
typedef struct CmdBuffer CmdBuffer_t;
typedef struct Highlight Highlight_t;

/*------------------------------------------------------------------------------*/

bool applyConvert(Context_t* context, const CmdBuffer_t* buffer, const Surface_t* src,
                  Surface_t* dst, CPUAccelerationFeatures_t accel);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_APPLY_CONVERT_H_ */