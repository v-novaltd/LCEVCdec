/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
#ifndef VN_DEC_CORE_TIME_H_
#define VN_DEC_CORE_TIME_H_

#include "common/platform.h"

/*------------------------------------------------------------------------------*/

typedef struct Memory* Memory_t;
typedef struct Time* Time_t;

/*------------------------------------------------------------------------------*/

bool timeInitialize(Memory_t memory, Time_t* time);
void timeRelease(Time_t time);
uint64_t timeNowNano(Time_t time);

/*------------------------------------------------------------------------------*/

#endif /* VN_DEC_CORE_TIME_H_ */
