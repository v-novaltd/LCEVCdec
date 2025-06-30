# 4.0.0 Migration Guide

LCEVCdec version 4.0.0 incudes a small API change that must be implemented in any downstream integrations before upgrading. This guide gives recommendations on how to implement this API change, changes to shared library structure and how to better call LCEVCdec 4.0.0 APIs for the optimal performance.

## Send data API changes

The function definition for `LCEVC_SendDecoderEnhancementData` has changed from

```c++
LCEVC_ReturnCode LCEVC_SendDecoderEnhancementData( LCEVC_DecoderHandle decHandle, int64_t timestamp, bool discontinuity,
                                                   const uint8_t* data, uint32_t byteSize );
```

to

```c++
LCEVC_ReturnCode LCEVC_SendDecoderEnhancementData( LCEVC_DecoderHandle decHandle, uint64_t timestamp,
                                                   const uint8_t* data, uint32_t byteSize );
```

And function definition for `LCEVC_SendDecoderBase` has changed from

```c++
LCEVC_ReturnCode LCEVC_SendDecoderBase( LCEVC_DecoderHandle decHandle, int64_t timestamp, bool discontinuity,
                                        LCEVC_PictureHandle base, uint32_t timeoutUs, void* userData );
```

to

```c++
LCEVC_ReturnCode LCEVC_SendDecoderBase( LCEVC_DecoderHandle decHandle, uint64_t timestamp, LCEVC_PictureHandle base,
                                        uint32_t timeoutUs, void* userData );
```

Removing the discontinuity flag and changing the timestamp variable to an **unsigned** long long. This aligns the timestamp type with many downstream integrations and moves the handling of discontinuous streams into the callers responsibility. This also has the improved logging and debugging effect that the LCEVCdec library will never change a frame's timestamp even internally, meaning that any logged timestamp from the decoder aligns exactly with what was input previously.

To easily migrate to the new function, first add a counter which is incremented whenever the old `discontinuity` flag is true, then use the helper utility `getUniqueTimestamp` from `src/utility/include/LCEVC/utility/timestamp.h` to generate a new-style timestamp from the old values.

## Pipelined Architecture

Version 4.0.0 of LCEVCdec adds two new pipelines - `cpu` and `vulkan` in addition to the `core` which has been renamed to `legacy`. The new default pipeline is `cpu` which is currently the only recommended pipeline for production use. Importantly, after updating the API call as above, the decoder will be functional as before with comparable performance to 3.x.x, however the new pipelined architecture of the `cpu` and `vulkan` pipelines can significantly improve hardware utilisation and performance when given multiple frames at once. In practice integrations can simplistically call `SendDecoderEnhancementData` and `SendDecoderBase` then wait for `ReceiveDecoderPicture` on a given frame then move onto the next frame. From version 4.0.0 onwards `SendDecoderEnhancementData` and `SendDecoderBase` should be called as soon as the data and base are ready, sending as many frames as possible to the decoder then only calling `ReceiveDecoderPicture` when the frame is actually required to be played. The new internal task structure can then resolve the inter-frame dependency tree and dispatch tasks across many CPU cores.

Previously, although recommended, it was possible to play a whole stream without calling `LCEVC_SynchronizeDecoder` with `dropPending=false` after sending the last frame. This is now a hard requirement to ensure all decoding tasks gracefully finish.

The sample code has been updated with these changes for reference.

## Library Structure Changes

When building LCEVCdec 3.x.x with `BUILD_SHARED_LIBS=ON` the following libraries were required for downstream integrations (linux libraries shown):

- `liblcevc_dec_api.so`
- `liblcevc_dec_core.so`

Now the following libraries will be built:

- `liblcevc_dec_api.so` - unchanged
- `liblcevc_dec_pipeline_cpu.so` - new default and recommended pipeline, toggled with `VN_SDK_PIPELINE_CPU`
- `liblcevc_dec_pipeline_vulkan.so` - not built by default and still experimental, not recommended for production use, toggled with `VN_SDK_PIPELINE_VULKAN`
- `liblcevc_dec_pipeline_legacy.so` and `liblcevc_dec_legacy.so` - the renamed `core` from previous versions, deprecated and will be removed in a future release, toggled with `VN_SDK_PIPELINE_LEGACY`

As well as toggling pipelines at build time via CMake flags, they can be changed at runtime with the API `LCEVC_ConfigureDecoderString("pipeline", "<pipeline>")` (or via JSON) where the argument can be either one of `cpu`, `vulkan` or `legacy` with more planned for future releases. Other than issues documented under 'Known Issues' of the release notes, if you have any issues in the new CPU pipeline please raise them as a Github issue and also try with the legacy pipeline. The Vulkan pipeline is not complete and issues are expected there, please don't raise Vulkan pipeline issues on Github yet.
