/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

typedef struct StaticImageDesc_t
{
    struct StaticImageHeader_t
    {
        uint32_t w;
        uint32_t h;
    } header;
    uint32_t dataSize;
    const uint8_t* const data;
} StaticImageDesc_t;

#ifdef __cplusplus
}
#endif
