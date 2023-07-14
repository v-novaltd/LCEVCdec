/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */
#ifndef VN_API_ACCEL_CONTEXT_H_
#define VN_API_ACCEL_CONTEXT_H_

namespace lcevc_dec::decoder {

class AccelBuffer
{};

class AccelContext
{};

} // namespace lcevc_dec::decoder

// This is an opaque API type. It has to go here so that it's accessible to both api.cpp and
// interface.cpp (which needs to cast up and down the inheritance hierarchy).
struct LCEVC_AccelBuffer : public lcevc_dec::decoder::AccelBuffer
{};

#endif // VN_API_ACCEL_CONTEXT_H_
