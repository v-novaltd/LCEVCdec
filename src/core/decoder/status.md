# TODO

Below is a list of changes still to be performed on the core decoder library, it is roughly prioritised:

* ~~Change macros to VN_BLAH style~~
* ~~Replace feature enable macros (i.e. `#ifdef VNEnableSSE` to `#if VN_FEATURE(SSE)`~~
* Replace any remaining "size" related variables with size_t.
* ~~Check and remove base hash.~~
* ~~New memory management~~
* ~~Make it compile with /W4.~~
* Replace assert with build flag wrapper.
* Sonarcube clean
* New error handling
    - Remove VNCheck, or make more powerful.
* Improve fixed width integer type usage
    - There's a lot of casts and conversions, that may be superfluous
    - Possibly implemented checked casts to ensure we're not losing any precision during casts.
* New logging
* Better logging for memory tracing report.
* Remove Context everywhere (or succinctly, remove passing context as a function arg everywhere, structs should just store a ref).
* Profiler rework
* Dither buffer rework
* Forward decl all the things as much as possible.
* New S-filter code
* Investigate value in changing upscale calling code for it to be more "optimal"
* New optimised decoder loop
* New API
    - Ensure user can pass in callbacks for allocation routines.
* Unit tests
* Tests for emscripten build
* First class support for interleaved formats (upscaler supports it, other code blocks less so).
* Fix numeric type craziness - lots and lots of casting between integral types that can probably generalised.

<br><br>

# File Status

| File                     | Includes | General Formatting | Doxygen | Clang Tidy | Notes |
| ------------------------ | -------- | ------------------ | ------- | ---------- | ----- |
| context.c                | Done     | Done               | Done    | Done       |       |
| context.h                | Done     | Done               | Done    | 1st pass   | Needs refactoring |
| api.c                    | Done     | Done               |         |            | About as clean as it will become, will be replaced with a new API. |
| common/bitstream.c       | Done     | Done               | Done    | Done       | Very "safe", probably at the cost of performance, to be evaluated. |
| common/bitstream.h       | Done     | Done               | Done    | Done       | Change API so that BitStream in opaque, user doesn't need to know the internal state. |
| common/bytestream.c      | Done     | Done               | Done    | Done       | Same as bitstream |
| common/bytestream.h      | Done     | Done               | Done    | Done       | Same as bitstream |
| common/cmdbuffer.c       | Done     | Done               | Done    | Done       |       |
| common/cmdbuffer.h       | Done     | Done               | Done    | Done       | Data representation may change, currently representation is unclear |
| common/dithering.c       | Done     | Done               | Done    | Done       | Dither buffer size is arbitrary. |
| common/dithering.h       | Done     | Done               | Done    | Done       | Dither buffer representation is stored on Context_t, this is wrong. |
| common/error.c           | Done     | Done               | Done    | Done       | Calls log directly, this is wrong |
| common/error.h           | Done     | Done               | Done    | Done       | Needs refactoring |
| common/log.c             | Done     | Done               |         | 1st pass   |       |
| common/log.h             | Done     | Done               |         | Done       | Needs refactoring |
| common/memory.c          | Done     | Done               |         | Done       |       |
| common/memory.h          | Done     | Done               | Done    | Done       |       |
| common/neon.h            | Done     | Done               |         |            | Some of this functionality needs to be evaluated and possibly expanded upon |
| common/platform.h        | Done     | Done               | Done    | 1st pass   | Memory allocation functionality needs to be removed, check macros need removing |
| common/profiler.c        | Done     | Done               |         | Done       | Lots of interesting things in here that could be improved |
| common/profiler.h        | Done     | Done               |         | Done       | Needs refactoring - although the primary interface is probably ok |
| common/random.c          | Done     | Done               |         | 1st pass   | Some todos - and weirdly has aliasing global state |
| common/random.h          | Done     | Done               |         | Done       |       |
| common/simd.c            | Done     | Done               |         |            | Needs to be driven from better feature macros (hopefully Sam has adopted this) |
| common/simd.h            | Done     | Done               |         |            |       |
| common/sse.h             | Done     | Done               |         |            | Same as neon.h |
| common/threading.c       | Done     | Done               |         | Done       | Abstractions need improving      |
| common/threading.h       | Done     | Done               |         | Done       | Needs refactoring |
| common/types.c           | Done     | Done               |         |            |       |
| common/types.h           | Done     | Done               |         |            | This needs splitting up, fixedpoint can be hoisted out |
| decode/apply_residual.c  | Done     | Done               |         |            |       |
| decode/apply_residual.h  | Done     | Done               |         |            | New decoding loop will be implemented |
| decode/dequant.c         | Done     | Done               |         | Done       | Probably some implementation improvements here. |
| decode/dequant.h         | Done     | Done               | Done    | Done       |       |
| decode/deserialiser.c    | Done     | Done               | N/A     |            |       |
| decode/deserialiser.h    | Done     | Done               | Done    |            | New API will remove this implementation |
| decode/entropy.c         | Done     | Done               | Done    | Done       |       |
| decode/entropy.h         | Done     | Done               | Done    | Done       | Use opaque handles. |
| decode/huffman.c         | Done     | Done               | Done    | 1st pass   |       |
| decode/huffman.h         | Done     | Done               | Done    | Done       | Use opaque handles. |
| decode/transform.c       | Done     | Done               |         | Done       |       |
| decode/transform.h       | Done     | Done               | Done    | Done       |       |
| decode/transform_unit.c  | Done     | Done               |         | Done       | This functionality is generally quite slow for a hot path (divs/mods) look into optimising |
| decode/transform_unit.h  | Done     | Done               | Done    | Done       | Consider opaque handle |
| surface/blit.c           | Done     | Done               |         | Done       |       |
| surface/blit.h           | Done     | Done               | Done    | Done       |       |
| surface/blit_common.h    | Done     | Done               | Done    | Done       |       |
| surface/blit_neon.c      | Done     | Done               | Done    |            |       |
| surface/blit_scalar.c    | Done     | Done               | Done    | Done       |       |
| surface/blit_sse.c       | Done     | Done               | Done    |            |       |
| surface/overlay.c        | Done     | Done               |         | Done       | If we decide to keep then this should be folded into blit as another blit mode |
| surface/overlay.h        | Done     | Done               |         | Done       | Check if we want to retain this functionality. If retaining remove the macro |
| surface/sfilter.c        | Done     | Done               |         | Done       | This needs some refactoring to behave similarly to blit & upscale. |
| surface/sfilter.h        | Done     | Done               | Done    | Done       | Some of S-Filter needs to be re-worked. |
| surface/sfilter_scalar.c | Done     | Done               |         | Done       | Needs refactoring |
| surface/sfilter_scalar.h | Done     | Done               |         | Done       | This can probably be removed as fwd decl should be sufficient. |
| surface/sfilter_simd.c   | Done     | Done               |         |            | Needs to be split to SSE and NEON |
| surface/sfilter_simd.h   | Done     | Done               | Done    |            | This can probably be removed too. |
| surface/surface.c        | Done     | Done               |         | 1st pass   | Still needs some tlc, file handling a candidate for removal, possibly user supplied functions required. |
| surface/surface.h        | Done     | Done               |         | Done       | Surface cache could possibly be split out from here. Surface dimensions should be size_t. |
| surface/surface_common.h | Done     | Done               | Done    |            |       |
| surface/upscale.c        | Done     | Done               | Done    | Done       | Look into refactoring the general processing model of the upscaler. |
| surface/upscale.h        | Done     | Done               | Done    | Done       |       |
| surface/upscale_common.c | Done     | Done               | N/A     | Done       |       |
| surface/upscale_common.h | Done     | Done               | Done    | Done       |       |
| surface/upscale_neon.c   | Done     | Done               | Done    | 1st pass   |       |
| surface/upscale_neon.h   | Done     | Done               | Done    |            | Can be removed as functions can be fwd decl |
| surface/upscale_scalar.c | Done     | Done               |         | Done       |       |
| surface/upscale_scalar.h | Done     | Done               |         | Done       |       |
| surface/upscale_sse.c    | Done     | Done               |         | Done       | This can probably be "improved" |
| surface/upscale_sse.h    | Done     | Done               |         | Done       | Can be removed as functions can be fwd decl |