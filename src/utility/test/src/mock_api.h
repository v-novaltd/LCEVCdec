// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Link time replacement for LCEVC_DEC API - captures calls and atguments for some entry points
//
#ifndef LCEVC_TEST_MOCK_API_H
#define LCEVC_TEST_MOCK_API_H

#include <iostream>

// Supply ostream that API logging will be written to
//
void mockApiSetStream(std::ostream* os);

#endif