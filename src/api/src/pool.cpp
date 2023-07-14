/* Copyright (c) V-Nova International Limited 2023. All rights reserved. */

#include "pool.h"

#include "accel_context.h"
#include "decoder.h"
#include "picture.h"
#include "picture_lock.h"

// - Pool -----------------------------------------------------------------------------------------

namespace lcevc_dec::decoder {

// Declare the pool types. Having this here (rather than in the header) allows us to avoid
// including every Pool'd type whenever we want to use one Pool.
template class Pool<AccelContext>;
template class Pool<Decoder>; // Only used as a base class for DecoderPool
template class Pool<Picture>;
template class Pool<PictureLock>;

} // namespace lcevc_dec::decoder
