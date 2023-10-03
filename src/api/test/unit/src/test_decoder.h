/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_TEST_UNIT_TEST_DECODER_H_
#define VN_API_TEST_UNIT_TEST_DECODER_H_

// Define this so that we can reach both the internal headers (which are NOT in a dll) AND some
// handy functions from the actual interface of the API (which normally WOULD be in a dll).
#define VNDisablePublicAPI

#include <LCEVC/lcevc_dec.h>

#include <cstddef>
#include <vector>

static const int32_t kResultsQueueCap = 24;
static const int32_t kUnprocessedCap = 100;

#endif // VN_API_TEST_UNIT_TEST_DECODER_H_
