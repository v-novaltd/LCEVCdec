# DIL configuration JSON
The DIL is configured via the `json_settings` string in the `DIL_Create` function. For most cases this value can be an empty string as the DIL will set suitable defaults.

This document lists the DIL configuration values grouped by expected use, some configuration is provided for internal use only and others are for debug use.

# Standard configuration values
These values are expected to be used by DIL users.

| Tag | Type | Default value | Description |
| :---- | :---- | :----- | :----- |
| can_use_restricted_system_calls | Boolean | true | Allows the user of the DIL to indicate that certain system calls can't be made. |
| dil_is_renderer | Boolean | false | Lets the DIL know that it will be rendering the decoded image. This allows for more performance optimisations by the DIL. |
| events | IntArray | {} | Only in the new `lcevc_dec.h` API. Allows the user to choose which events to receive and ignore: only event types which are listed in this array will be sent. |
| gl_decode | Boolean | false | Select GPU or CPU pipeline for upscale and apply residuals. |
| enable_gpu | Boolean | Set based on OpenGL version | Controls if the GPU will be available for decoding and/or rendering. By default this is initialised to `true` if the OpenGL version is >= 3.0 otherwise it is set to `false`. This means that on `OpenGL ES 2.0` systems the GPU will be disabled. It is still possible to enable the GPU by setting this value to `true` in the JSON config. However this may cause the `DIL_Create` to fail and even if it succeeds it is not guaranteed that the decode or render will work correctly. Setting this to `false` will disable the GPU code. This may result in decode and/or renders failing due to the GPU code being unavailable. |

# Internal Testing
NOTE: setting the `gl_major`, `gl_minor` and/or `gl_es` is only a hint to the OpenGL driver.
Whilst it will attempt to operate at a lower level it is still a higher level driver and may not check that all the values are valid for the version requested.
The driver is unlikely to be able to simulate any version bugs and odd behaviour.

| Tag | Type | Default value | Description |
| :---- | :---- | :----- | :----- |
| force_bt709_to_bt2020_conversion | Integer | -1 | Force the BT709 to BT2020 conversion on (1) or off (0), the default (-1) disables this feature and allows the code to decide what to do. |
| force_lcevc_bitdepth_loq0 | Integer | 0 | Force a valid bit depth, 8, 10, 12, 14 or 16, for the LOQ1 processing. |
| force_lcevc_bitdepth_loq1 | Integer | 0 | Force a valid bit depth, 8, 10, 12, 14 or 16, for the LOQ1 processing. |
| forced_base_colorinfo | ColorInfo | default | Force a specific value(s) on the base (input) image. See the ColorInfo description below for more details and the default values. |
| forced_enhanced_colorinfo | ColorInfo | default | Force a specific value(s) on the enhanced (output) image. See the ColorInfo description below for more details and the default values. |
| gl_es | Boolean | false | Force specific OpenGLES version to be used by the DIL. When true the version of OpenGLES specified in gl_major and gl_minor will be used. |
| gl_finish_mid_rgb_convert | Boolean | false | Perform a glFinish in the middle of the RGB-to-pipeline input conversion stage. Needed for the MediaTek Palmer in RGBTexture mode. |
| gl_major | Integer |from device | Force specific OpenGLES major version number to be used by the DIL. Must set a value for GL minor version too. |
| gl_minor | Integer | from device | Force specific OpenGLES minor version number to be used by the DIL. Must set a value for GL major version too. |
| max_texture_upload_size | Integer | INT_MAX |Some devices have a limit for the maximum buffer size that can be used to upload a texture. This limits the upload to a maximum byte size and upload in block of that size. |
| post_pipeline_tonemapper | String | "" | Selects the tonemapping Look Up Table (LUT) to be used when a LUT is required. This overrides the default decided by the DIL based upon the input and output image color information. Possible values are `HlgToPq`, `PqToHlg`, `HDR2020ToSDR2020`, `SDR2020ToHDR2020`, `HDR2020ToSDR2020Hable`, `SDR2020ToHLG2020Hable`, `CustomLUT1` or `CustomLUT2`. NOTE: the two `CustomLUT`'s need to be defined using `tonemap_custom_1` and/or `tonemap_custom_2` before use, by default they are empty. This applies the LUT after the main decode pipeline. |
! post_pipeline_tonemapper_forced | Boolean | false | When `true` the value of `post_pipeline_tonemapper` will be used for all decode processing. |
| pre_pipeline_tonemapper | String | "" | Selects the tonemapping Look Up Table (LUT) when a LUT is required. This overrides the default decided by the DIL based upon the input and output image color information. Possible values are `HlgToPq`, `PqToHlg`, `HDR2020ToSDR2020`, `SDR2020ToHDR2020`, `HDR2020ToSDR2020Hable`, `SDR2020ToHLG2020Hable`, `CustomLUT1` or `CustomLUT2`. NOTE: the two `CustomLUT`'s need to be defined using `tonemap_custom_1` and/or `tonemap_custom_2` before use, by default they are empty. This applies the LUT before the main decode pipeline. |
! pre_pipeline_tonemapper_forced | Boolean | false | When `true` the value of `pre_pipeline_tonemapper` will be used for all decode processing. |
| required_gl_major | Integer | 3 | Minimum required GL major version to enable GPU support, see `enable_gpu` for more details. |
| required_gl_minor | Integer | 0 | Minimum required GL minor version to enable GPU support, see `enable_gpu` for more details. |
| supports_secure_mode | Integer | -1 | Possible values are -1 = auto, 0 = not supported, 1 = supported.<br>The DIL detects if the device is able to support secure mode for DRM playback. The DIL requires some EGL and GL extensions to use secure textures and surfaces. Also some hardware and/or driver versions have bugs that cause problems. In auto mode the result of extension and driver/hardware checks is used, else the config can overwrite the detected value. |
| tonemap_custom_1 | Number array or String | [], "" | When set as a JSON array it defines an array of floats to be used in tonemapping. When set as a string it defines a 1D cube map file, the full path, to be loaded as the array of floats to use. Selected using the tag `CustomLUT1`. NOTE: when using the JSON array mode, due to the way that JSON converts and stores numbers the value used may not be exactly what was specified, e.g `99.33433` will become `99.334328` which is close but not the same. |
| tonemap_custom_2 | Number array or string | [], "" | When set as a JSON array it defines an array of floats to be used in tonemapping. When set as a string it defines a 1D cube map file, the full path, to be loaded as the array of floats to use. Selected using the tag `CustomLUT2`. NOTE: when using the JSON array mode, due to the way that JSON converts and stores numbers the value used may not be exactly what was specified, e.g `99.33433` will become `99.334328` which is close but not the same. |
| sync_after_decode | Boolean | false | Controls if a GL sync is done at the end of each decode.|
| use_wide_bitdepth_backbuffer | Boolean | false | Used for testing non 8bit streams. This is currently the only way to test non 8bit streams. This needs to be removed and the DIL needs to be able to create the output image based on the input and LCEVC block, and process things as required. This will require an API change to allow images to be created as `automatic` types, thus allowing the DIL to do what it needs to do.

# ColorInfo structure
All of these values match values of similar names in ITU-T Rec. H.273 v2 (07/2021) and ISO/IEC TR23091-4:2021 (twinned doc).
| Tag | Type | Default value | Description |
| :---- | :---- | :----- | :----- |
| matrix_coeffs | Integer | 2 | This is actually an `Image::MatrixCoefficients::Enum` Here are some of the main values that could be used: `UNSPECIFIED` = 2, `BT709` = 1, `BT601_NTSC` = 6, `BT2020_NCL` = 9, `BT2020_CL` = 10. |
| primaries | Integer | 2 | This is actually an `Image::ColorTransfer::ColorPrimaries` Here are some of the main values that could be used: `UNSPECIFIED` = 2, `BT709` = 1, `BT601_NTSC` = 6, `BT2020` = 9. |
| range | Integer | 0 | This is actually an `Image::ColorRange::Enum` which has the following possible values `Unknown` = 0, `Full` = 1, `Limited` = 2. |
| transfer | Integer | 2 | This is actually an `Image::ColorTransfer::Enum` Here are some of the main values that could be used: `UNSPECIFIED` = 2, `BT709` = 1, `BT601` = 6, `LINEAR` = 8, `PQ` = 16 or `HLG` = 18. |

# Functionality
These values affect the features that are offered to the client, either related to LCEVC decoding or to the
support of for ex. further processing

| Tag | Type | Default value | Description |
| :---- | :---- | :----- | :----- |
| deinterlace_is_simple_copy | Boolean | false | **NON-COMPLIANT**<br>deinterlace function is a simple copy of the input image. |
| dithering | Boolean |	true | **NON-COMPLIANT**<br>Enable or disable dithering. |
| dither_strength | Integer | -1 | **NON-COMPLIANT**<br>Possible values, -1 = Use what is is in the stream, 0 or above = override the stream.<br>Control the source and level of dithering strength. Applies in both original DIL and new `lcevc_dec.h` API. |
| enable_logo_overlay | Boolean | false | Enable an LCEVC logo watermark during playback to show that the stream is being enhanced by LCEVC. |
| external_decode | String | "" | Load a shared library for testing custom decoder code. |
| logo_overlay_delay_frames | Integer | -1 | Specify number of frames before displaying overlay. Default of -1 will use internal default. |
| logo_overlay_position_x | Integer | -1 | Specify displacement in pixels of left edge of overlay watermark from left edge of video. Default of -1 will use internal default. |
| logo_overlay_position_y | Integer | -1 | Specify displacement in pixels of top edge of overlay watermark from top edge of video. Default of -1 will use internal default. |
| maintain_last_lcevc | Boolean | true | When `true` frames with missing lcevc data will use the last received LCEVC state, assuming the input_cc's match. Else frames with no lcevc data will drop to passthrough. |
| pipeline_bitdepth | Integer | 3 | (GPU mode only) The bitdepth of the pipeline, i.e. what texture type are the images stored in as they're processed. 0 is 8bit float, 1 is 16bit float, 2 is 16bit integer, and 3 is auto-configure. Auto-configure follows this logic: if pipeline_is_integer, and you request a wide bitdepth backbuffer, and the device is not ES2, then use 16bit integer; else, if the base is greater-than-8bits, or we request a wide bitdepth backbuffer and successfully create it on a non-ES2 device, then use 16bit float; else, use 8bit float. |
| predicted_average_method | Integer | 1 | Possible values 0 = disable predicted average (GPU mode only, **non-compliant**), 1 = apply PA directly, 2 = apply PA approximately, by pre-baking it into the upscaling kernels. Option 1 generally has the worst performance, while option 2 has better performance. Option 0 mainly exists just for debugging, because PA can conceal decoding errors. Note that PA is also signalled by the stream, so if the stream has PA turned off, these options won't be able to turn it on. |
| pss_surface_fp_setting | Integer | -1 | **NON-COMPLIANT**<br>Possible values -1 = suggest U8 surfaces, 0 = use S87 surfaces, 1 = use U8 surfaces.<br>Choose the surface for residuals, as used by the DPI. 0 uses S87, -1 suggests U8 surfaces but switches if input is 10+ bits or DPI mode is 'precision', and 1 forces u8 surfaces. |
| s_filter_mode | Integer | -1 | **NON-COMPLIANT**<br>Possible values, -1 = use LCEVC Stream value, 0 = Turn off S-Filter, 1 = Turn on S-Filter using value given by s_strength.<br>Control turning on and off S-Filter and whether to use the Stream value or specified version.|
| s_strength | Float | -1 | **NON-COMPLIANT**<br>Override S-Filter strength specified in the LCEVC stream, anything 0 or above will over ride what is in the stream. |
| synchronise_decode | Boolean | false | Call a GL synchronise at the end of the decode operation. |
| temporal_active | Boolean | true | **NON-COMPLIANT**<br>Override the LCEVC stream temporal active flag, setting to false will disable temporal processing. |
| use_backbuffer_timestamp | Boolean | false | (Currently only supported on Android) On rendering to the back buffer, whether to tag or not the frame timestamp. If tagged, a consumer on the surface would be able to retrieve the timestamp associated with the frame. Note that this is useful only if the DIL is configured to render to an off screen surface and there is an external post processor that draws to the screen, in which case the post processor would be able to get the timestamp associated with each frame.<br>NOTE: this feature might be unstable if used on certain Android GL ES implementations with ES version lower than 3.1.
| use_chroma_residuals | Boolean | true | **NON-COMPLIANT**<br>For debugging performance issues, removes processing of chroma residuals. |
| use_loq0 | Boolean | true | **NON-COMPLIANT**<br>For debugging performance issues, removes processing of LOQ0 residuals.<br>Only works in GPU mode. |
| use_loq1 | Boolean | true | **NON-COMPLIANT**<br>For debugging performance issues, removes processing of LOQ1 residuals.<br>Only works in GPU mode. |

# Performance
These values control performance sensitive parts of the DIL. For now they can be controlled via the config file but it is expected that the DIL
will be able to change the values dynamically at some point in the future. This will allow the DIL to adapt to devices as it decodes.

| Tag | Type | Default value | Description |
| :---- | :---- | :----- | :----- |
| can_precalculate | Boolean | false | Only impacts CPU mode. When set true the DIL will generate residuals asynchronously from the apply/upscale etc. Trades memory bandwidth vs processing cost. Currently does not fully work. This is required to be able to toggle between GPU and CPU modes.|
| core_parallel_decode | Boolean | false | Enable a parallel version of the core decoding code, default is the standard serial version. Also used in conjunction with `dpi_threads`.|
| cpu_toggle_count | Integer | 0 | Number of CPU decodes to do before switching to GPU decodes. Must have `can_precalculate` active & input and output types must be supported by both CPU & GPU mode. If zero do everything as per gl_decode flag. Currently does not work correctly with ABR ladder switching. |
| dpi_pipeline_mode | Integer | 0 | Set the pipeline mode for the DPI (or "Core Decoder"). 0 is "speed" and 1 is "precision". |
| dpi_threads | Integer | -1 | Set the number of DPI threads used for CPU mode LCEVC processing. The default of `-1` will automatically set the value to the number of available cores - 1. |
| passthrough_mode | Integer | 0 | **NON-COMPLIANT**<br>Selects the DIL mode for pass through, which is the base frame passing through the DIL pipeline without lcevc enhancement being applied. Possible values -1 = Disable: base will not pass through, i.e. it must always be lcevc enhanced, if lcevc data are not available or not valid decode will fail; 0 = Allow, base will pass through if lcevc is not found or not applied; 1 = Force, base will pass through, regardless of lcevc data being present or valid. |
| geometry_residuals | Boolean | false | Allows the residual textures to be created using GPU drawing commands, rather than a buffer upload. This may be more efficent for some cases. This is still being tested at the moment. |
| gpu_toggle_count | Integer | 0 | Number of GPU decodes to do before switching to CPU decodes. Must have `can_precalculate` active & input and output types must be supported by both CPU & GPU mode. If zero do everything as per gl_decode flag. Currently does not work correctly with ABR ladder switching. |
| loq_residual_image_type | Integer | 0 | Possible values are 0 = buffers, 1 = textures, 2 = hardware buffers.<br>The format that residuals are stored in. Some restrictions exist: Textures can not be used with CPU, only GPU, buffers/HW buffers typically for both, only certain Android and iOS devices support hardware buffers. Ideally the DIL should be allowed to select this value based on the current configuration and device capabilities. |
| loq_processed_cap | Integer | 24 | Capping limit on the processed LCEVC buffers in the LOQ store. Current sizing is defined to be able to iron out frames with huge numbers of residuals (charging). For devices with smaller memory size this can be useful but too small and performance suffers.<br>Ideally the DIL should be allowed to select this value based on the current configuration and device capabilities. This was `loq_store_size` which is, currently, still supported but is deprecated. |
| loq_unprocessed_cap | Integer | 100 | Capping limit on the unprocessed LCEVC buffers in the LOQ store. This limits the number of stored LCEVC buffers in the DIL. Setting to 0 will remove the cap and the DIL will store all the buffers given to it. If the limit is reached the DIL will return `DIL_RC_TryLater` to indicate that the buffer has not been stored. In the `lcevc_dil.h` API, this also sets the limit on base pictures and output-container pictures. |
| number_quads | Integer | 0 | Some devices need a "more complicated GPU draw operation" to work correctly (here's looking at you Quest2). This value allows for triangles to be configured as part of the GPU drawing operation. We found that 100 was enough to get the Quest2 to work correctly. There is a performance hit when using this value. |
| pipeline_is_integer | Boolean | false | (GPU mode only) Whether or not to use the integer pipeline. Note that our only integer pipeline is 16bit, so if the device can't handle that, it'll be 8bit float |
| render_late_time_ms | Integer | 10 | If render has a desired time this value allows the request to be late by # ms before failing with `DIL_RC_Timeout` |
| results_queue_cap | Integer | 24 | **ONLY FOR THE NEW lcevc_dil.h API** <br> Capping limit on the results in the results queue. This is a queue of the results that get reported by `LCEVC_ReceiveDecoderPicture`. It can be thought of as a "decoded picture queue" (though it will also include failed decodes on the queue). |
| simple_render_mode | Boolean | false | Controls if the render process is performed in 2 stages (false), with the swap being performed at the desired render time. Or in a single stage (true) where the render draw is started at the desired render time. |
| stable_frame_count | Integer | 16 | Number of frame decodes before the DIL starts looking at decode average time used by the message queue to hold off decodes (based on how long they take) when rendering. The first few frames have a skewed decode time because they will have some buffer and texture allocation time included. This allows certain number of frames to be ignored at the start of processing. This allows the DIL to hit the render times more accurately. It is unlikely that this needs changing unless there is a system running LCEVC that takes more than 16 frames to settle down. |
| timed_run_adjust_scale | Float | 0.3 | Used to multiply by the current average decode time to product a back off time (30% of a decode time). Currently the DIL can either be decoding or rendering a frame. If a frame is being decoded any frame that requires rendering will have to wait for the decode to complete, this can cause renders to occur late. To help alleviate this problem the DIL keeps an average of the last 100 decode times (AV_DEC) and can hold off a pending decode if there is a render required within `AV_DEC * timed_run_adjust_scale` ms, and will wait for the render to be complete before starting the decode. This allows the DIL to hit the render times more accurately. It is unlikely that this needs changing. |
| use_dither_texture | Boolean | false | **NON-COMPLIANT**<br>When dithering is enabled, use a precomputed noise texture to perform the dithering. Has no effect if dithering is disabled. |

# Debug and Stats
These values control various parts of the DIL to aid in debugging and tracking problems.

| Tag | Type | Default value | Description |
| :---- | :---- | :----- | :----- |
| debug_config_path | String | ./dpi_dump.json | DPI debug configuration JSON file output location, setting this enables dump of DPI configuration. |
| dump_deinterlace_input | Boolean | false | Dump the input image when in deinterlace mode. Note when this is active this will be the only input dump. |
| dump_filename | String | "" | File name to dump YUV of output frame, empty string ("") is no dump. See also `dump_pipeline_image`. If you supply two "%d"s in the file name, they'll be replaced by the width and height respectively. If you supply two "%d"s and a "%s", the "%s" will be replaced with either "rgb" or "yuv" depending on the file type. Finally, if you supply two "%d"s and two "%s"s, the first "%s" will be replaced the interleaving type ("NV12", "None", etc), while the second will be replaced with "rgb" or "yuv". Please do not use any % symbols beyond this.|
| dump_frame_count | Integer | 0 | Number of frames to dump. Default is to dump all frames. |
| dump_frame_start | Integer | 0 | Frame number to start dumping from, in the range 0 - #-1. |
| dump_input_filename | String | "" | File name to dump YUV of input frame, empty string ("") is no dump. See also `dump_pipeline_image`. This filename uses the same formatting rules as `dump_filename`.|
| dump_lcevc_filename | String | "" | File name to dump LCEVC bin of input frame, empty string ("") is no dump. NOT affected by `dump_pipeline_image`. This filename does NOT use any format string substitutions. Dumped .bin file won't include DTS values, so it can only be used by the DIL (the DPI requires DTS values to reorder lcevc frames).|
| dump_pipeline_image | Boolean | false | Controls if the input/output dump is, 1: the pipeline image (true), 2: the initial input/final output image (false). Should be set if using `dump_input_filename`. |
| dump_rate | Integer | 0 | Number of frames to skip before dumping a frame. Setting to 1 will dump every other frame	0 disables this and dumps all frames. |
| hashes_filename | String | "" | File to output Frame hashes. Hashes are used for conformance testing to see if frames match expected output. Empty string is no dump. |
| hasher_type | String | "" | Hasher object type to use when producing the hash. Possible values XXH3 (default), XX64, RAW. |
| highlight_residuals | Boolean | false | **NON-COMPLIANT**<br>Setting true saturates LOQ 1 and 0 residuals makes residuals highly visible, useful as a quick check that residuals are present and LCEVC is being decoded and applied.|
| invalidate_shader_cache | Boolean | false | Forces the shader cache to be invalidated when the DIL is initialised. |
| load_local_shaders | Boolean | false | Uses local shaders on the disk. |
| log_level | Integer | varies |  Possible values 0 - 5 (off - verbose).<br>Used to override the client's log level settings for the DIL. For the original DIL, the default is 4 (Debug). For the `lcevc_dec.h` API, the default is 1 (Error). |
| log_stdout | Boolean | true | Output logging over standard output or not, can be used to force logging to use stdout. |
| max_stats_store_size | Integer | 100 | The stats system maintains a stack of `stats` information on a per frame basis (keyed by `timehandle`). This allows stats for a frame to be modified over time until it's finally written to file. This allows the decode and render stats to be logged for the same frame in one line. The stats data is written to disc once the size of the stored stack exceeds this value.|
| shader_cache_location | String | /sdcard or . | Specifies the location the shader cache should be read from /written to. Android defaults to `/sdcard/` and all other platforms default to `.` (current workign directory). When `load_local_shaders` is true this can also be used to specify where the shader code should be loaded from. |
| stats | Boolean | false | Enable DIL timing statistics for different elements of LCEVC decoding and application to a statistics file for analysis	May impact performance on some device types, creates statistics file on host device. |
| stats_file | String | dil_stats.json | File name and path to dump statistics file out to. Empty string will gather stats, but not write them out. Not defined will use default, which will be in /sdcard for android, Documents for iOS, Library/Caches for tvOS, ${HOME} for IPhone or Simulator and ./ for everything else. |
| stats_file_writer_sleep_ns | Long | 5000000 | Wait time in nano seconds that the stats file writer will sleep whilst polling the queue. |
| stats_max_collectors | Integer | 10 | Maximum number of stats generators (threads). |
| stats_queue_read_size | Integer | 256 | Size of the bulk queue reader buffer. |
| stats_queue_size | Integer | 1024 | Size of the queue. |

# Legacy, deprecated  or unused values
| Tag | Type | Default value | Description |
| :---- | :---- | :----- | :----- |
| disable_gl_deinterlacer | Boolean | false | Fully disable the use of the deinterlacer shaders, which can crash on some devices. |
| enable_predicted_average | Boolean | true | **NON-COMPLIANT**<br>Enable and disable predicted average (or "PA"). This is useful to debug the image, because PA can cover up issues in the LCEVC decode. This only works in GPU mode; in CPU mode, PA is always set from the stream. This has been deprecated in favour of `predicted_average_method`|
| gl_swap_interval | Integer | 1 | Open GLES's swap interval configuration. |
| fullscreen | Boolean | false | Sets the Open GL window inside the context to operate in full screen mode. |
| hints | JSON |  NONE | It is intender that the DIL user can provide some maximum values to the DIL to allow it to configure and allocate resources and memory during the configuration phase rather than suffering the hit during he first few frames of processing. |
| input_type | Integer | NONE | Hints what the input type will be. |
| loq_raw_cap | Integer | - | Please use `loq_unprocessed_cap` in preference. |
| u8_surface | Boolean | false | **NON-COMPLIANT**<br>Uses an unsigned bit surface instead of signed 16	Improves performance due to reduced data size. This has been deprecated in favour of `pss_surface_fp_setting`. |
| use_old_code_lengths | Boolean | false | Allow streams created with an old encoder to be played. This will be removed at some point. |
