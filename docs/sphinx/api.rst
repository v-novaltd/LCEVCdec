API
===


Data Structures
---------------

.. doxygenstruct:: LCEVC_DecoderHandle
   :members:

.. doxygenstruct:: LCEVC_PictureHandle
   :members:

.. doxygenstruct:: LCEVC_AccelContextHandle
   :members:

.. doxygenstruct:: LCEVC_AccelBufferHandle
   :members:

.. doxygenstruct:: LCEVC_PictureLockHandle
   :members:

.. doxygenstruct:: LCEVC_HDRStaticInfo
   :members:

.. doxygenstruct:: LCEVC_DecodeInformation
   :members:

.. doxygenstruct:: LCEVC_PictureDesc
   :members:

.. doxygenstruct:: LCEVC_PictureBufferDesc
   :members:

.. doxygenstruct:: LCEVC_PicturePlaneDesc
   :members:


Enums
-----

.. doxygenenum:: LCEVC_LogLevel

.. doxygenenum:: LCEVC_LogPrecision

.. doxygenenum:: LCEVC_ReturnCode

.. doxygenenum:: LCEVC_ColorRange

.. doxygenenum:: LCEVC_ColorPrimaries

.. doxygenenum:: LCEVC_TransferCharacteristics

.. doxygenenum:: LCEVC_MatrixCoefficients

.. doxygenenum:: LCEVC_PictureFlag

.. doxygenenum:: LCEVC_ColorFormat

.. doxygenenum:: LCEVC_Access

.. doxygenenum:: LCEVC_Event


Functions
---------

Most functions take a ``decHandle`` parameter for a decoder object, functions will return LCEVC_InvalidParam if the handle is invalid or LCEVC_Uninitialized if the decoder isn't initialized before running decoder-specific methods.

.. doxygenfunction:: LCEVC_DefaultPictureDesc

.. doxygenfunction:: LCEVC_AllocPicture

.. doxygenfunction:: LCEVC_AllocPictureExternal

.. doxygenfunction:: LCEVC_FreePicture

.. doxygenfunction:: LCEVC_SetPictureFlag

.. doxygenfunction:: LCEVC_GetPictureFlag

.. doxygenfunction:: LCEVC_GetPictureDesc

.. doxygenfunction:: LCEVC_SetPictureDesc

.. doxygenfunction:: LCEVC_GetPictureBuffer

.. doxygenfunction:: LCEVC_GetPicturePlaneCount

.. doxygenfunction:: LCEVC_SetPictureUserData

.. doxygenfunction:: LCEVC_GetPictureUserData

.. doxygenfunction:: LCEVC_LockPicture

.. doxygenfunction:: LCEVC_GetPictureLockBufferDesc

.. doxygenfunction:: LCEVC_GetPictureLockPlaneDesc

.. doxygenfunction:: LCEVC_UnlockPicture

.. doxygengroup:: LCEVC_ConfigureDecoder

For a list of options settable with this function, see :ref:`configurable options<api:Configuration Options>`

.. doxygenfunction:: LCEVC_CreateDecoder

.. doxygenfunction:: LCEVC_InitializeDecoder

.. doxygenfunction:: LCEVC_DestroyDecoder

.. doxygenfunction:: LCEVC_SendDecoderEnhancementData

.. doxygenfunction:: LCEVC_SendDecoderBase

.. doxygenfunction:: LCEVC_ReceiveDecoderBase

.. doxygenfunction:: LCEVC_SendDecoderPicture

.. doxygenfunction:: LCEVC_ReceiveDecoderPicture

.. doxygenfunction:: LCEVC_PeekDecoder

.. doxygenfunction:: LCEVC_SkipDecoder

.. doxygenfunction:: LCEVC_SynchronizeDecoder

.. doxygenfunction:: LCEVC_SetDecoderEventCallback

Typedefs
--------

.. doxygentypedef:: LCEVC_EventCallback

Configuration Options
---------------------

The decoder is designed to automatically pick the optimal configuration for the build, runtime and stream however parameters can be manually overridden for testing and debugging. Some parameters are common to all pipelines and others are only recognized when running in a specific pipeline. Some parameters have additional context specified in :ref:`Configuration Options Details<api:Configuration Options Details>`.

Common Options
..............

The following options are not specific to any given pipeline and may need to be set for a given integration.

=========================== ========== ================ ===============================================================
Option                      Type       Default          Description
=========================== ========== ================ ===============================================================
``pipeline``                string     cpu              The decode pipeline to use, options are ‘cpu’, ‘vulkan’
                                                        or ‘legacy’.
``events``                  intArray   \-               Array of :cpp:enum:`LCEVC_Event`. The events that will be
                                                        generated via the event callback.
``threads``                 int        physical threads The number of threads to spawn for parallel tasks.
``log_level``               int        6                Set the amount of logging printed where 0 is no logs and 6 is
                                                        verbose (maximum)
``log_stdout``              boolean    true             If true, logs go to stdout. If false, logs go to a
                                                        platform-specific log handler: `__android_log_write` (Android),
                                                        `os_log_with_type` (Apple), or else `stderr` only for severe
                                                        logs (Windows, Linux). Windows also always prints logs to the
                                                        Debugger (e.g. the Visual Studio console).
``passthrough_mode``        int        0                Determines if the Decoder runs in passthrough mode never (-1),
                                                        always (1), or only when LCEVC Enhancement data is missing (0).
                                                        Passthrough mode means that no LCEVC is applied whatsoever: the
                                                        base is simply copied to the output picture.
=========================== ========== ================ ===============================================================

CPU Pipeline Options
....................

=========================== ========== ================ ===============================================================
Option                      Type       Default          Description
=========================== ========== ================ ===============================================================
``default_max_reorder``     int        16               The number of frames to buffer in the re-ordering queue. Can be
                                                        set lower for latency-critical applications where b-frames are
                                                        not used.
``enhancement_delay``       int        0                Number of frames after the base that the enhancement may arrive
``max_latency``             int        32               The maximum number of frames that the decoder is expected to
                                                        buffer, a greater value will increase memory usage but reduce
                                                        potential stuttering and can improve energy use on mobile
                                                        devices.
``min_latency``             int        0                The number of frames that the decoder may buffer before
                                                        `LCEVC_ReceiveDecoderPicture` will block waiting for a picture
                                                        to complete.
``temporal_buffers``        int        1                A temporal buffer requires a full size 16-bit plane for each
                                                        enhanced plane. Increasing this value to 2 allows the next GOP
                                                        to start processing before the last has finished to reduce
                                                        stuttering at the cost of additional memory.
``log_tasks``               boolean    false            Debug parameter for logging the task pool during decoding.
                                                        This causes blocking in the pipeline and requires log_level=debug
=========================== ========== ================ ===============================================================

Legacy Pipeline Options
.......................

============================= ========== ================ ===============================================================
Option                        Type       Default          Description
============================= ========== ================ ===============================================================
``enable_logo_overlay``       boolean    false            Adds a V-Nova logo over the decoded output.
``generate_cmdbuffers``       boolean    true             Generate CPU cmdbuffers and apply to the plane as a separate
                                                          step rather than directly applying during decode loop.
``high_precision``            boolean    false            Use a 16-bit temporal plane for 8-bit decoding as required by
                                                          the LCEVC standard. Otherwise uses a 8-bit temporal plane which
                                                          can cause rounding errors but is slightly faster/lower memory.
``predicted_average_method``  int        1                Whether to apply predicted average directly (1) or
                                                          approximately, using the upsampling kernel (2). Predicted
                                                          average is a step of upsampling, in which the upsampled 2x2
                                                          pixel is normalized to the same magnitude as the original source
                                                          pixel.
``logo_overlay_delay_frames`` int        0                Wait a given number of frames before applying logo overlay.
``logo_overlay_position_x``   int        auto             Change the logo overlay X pixel position.
``logo_overlay_position_y``   int        auto             Change the logo overlay Y pixel position.
``loq_unprocessed_cap``       int        100              The number of frames of raw LCEVC Enhancement data to store
                                                          before rejecting it. Use -1 to wrap around to infinity for
                                                          "no cap".
``parallel_decode``           boolean    false            Enable multi-threaded huffman layer decoding.
``predicted_average_method``  int        1                Three options: 0 - PA off, 1 - standard conformant PA,
                                                          2 - approximate PA where the upscale coefficients are adjusted
                                                          to improve speed - not conformant to LCEVC standard.
``pss_surface_fp_setting``    int        1                This determines whether residuals are stored in S16 (0) or U8
                                                          (1) surfaces. The default (-1) is to use U8 surfaces if and only
                                                          if all LOQs are 8bit.
``results_queue_cap``         int        24               The number of decoding results (either decoded pictures or
                                                          decode failures) to store. This queue is cleared by calling
                                                          :cpp:func:`LCEVC_ReceiveDecoderPicture`. Use -1 to wrap around
                                                          to infinity for "no cap".
============================= ========== ================ ===============================================================

Vulkan Pipeline Options
.......................

TODO - Vulkan is still in development and not all options are defined yet.

Debug Configuration Options
...........................

The following options use the same API for configuration but are primarily designed to force specific operation modes
for use in the functional tests or debugging.

=========================== ========== ============= =========== ==================================================================
Option                      Type       Default       Pipelines   Description
=========================== ========== ============= =========== ==================================================================
``allow_dithering``         boolean    true          all         If false, dithering will not occur, even when it's enabled in the
                                                                 stream.
``dither_seed``             int        -1 (disabled) all         The seed to use for dithering.
``dither_strength``         int        -1 (disabled) all         If provided, this overrides the stream's Dither strength. Dither is
                                                                 random noise applied during rendering. It subjectively improves
                                                                 video quality, at low cost.
``force_bitstream_version`` int        -1 (auto)     all         Override bitstream version information. Version information is
                                                                 typically present in streams encoded by V-Nova, but absent in
                                                                 LTM-encoded streams.
``force_scalar``            boolean    false         cpu, legacy If true, no SIMD (SSE, NEON, etc.) will be used.
``highlight_residuals``     boolean    false         all         If true, residuals will appear as saturated squares.
``s_filter_strength``       float      -1 (disabled) all         If provided, this overrides the stream's S-Filter strength.
                                                                 S-Filter is a sharpening modification to the upsampling step.
``trace_file``              string     \-            cpu, vulkan Path to dump a `perfetto <https://ui.perfetto.dev/>`_ compatible
                                                                 JSON file - ``VN_SDK_TRACING`` CMake flag required.
=========================== ========== ============= =========== ==================================================================


Configuration Options Details
-----------------------------

See below for additional information on commonly used options.

``log_level``
.............

Type: ``int``

Sets the amount of logging output by the decoder based on severity levels:

- 0 - Disabled: No logs are emitted
- 1 - Fatal: Logs the cause of an error that requires the decoder to immediately crash
- 2 - Error: Logs issues which entirely prevent the decoder from performing a vital task for example a blank screen or defaulting to passthrough
- 3 - Warning: Logs problems which will have adverse affects on either performance or visual quality. The decoder will work, but badly.
- 4 - Info: Important information which any user would want to know, such as version data and configuration.
- 5 - Debug: Logs things like resolution changes, dropped frames, and queues reaching their capacity. Should be *less frequent* than once per-frame.
- 6 - Verbose: This is the only level which is allowed to emit information once per frame. Signposts significant steps in the code path. Typically, external users will not appreciate these logs, but LCEVCdec developers will.

Note that, even at the Verbose level, some types of logs should *never* be seen. For example, the Verbose level can show logs that *all* developers understand, but not logs that are understood by only 1 developer (or worse, none). Raw pointer addresses should be exceedingly rare. There should never be per-pixel or per-row logs.

At the Debug and Verbose level, timestamps are provided in nanoseconds. At all other levels, timestamps are provided in microseconds.

Logging is enabled as soon as the ``log_level`` option is set. The ``LCEVC_CreateDecoder`` function cannot produce any logging messages - it will ony allocate memory, and use its the return code to show success. The real work of creating the decoder will happen during ``LCEVC_InitializeDecoder``, and will produce the requested diagnostics and status through the configured logging.

``events``
..........

Type: ``intArray``

Sets the events that will trigger callbacks set by ``LCEVC_SetDecoderEventCallback``. For example, to enable all callback events to be triggered, use the snippet:

.. code-block:: C++

   static const std::vector<int32_t> kAllEvents = {
       LCEVC_Log,
       LCEVC_Exit,
       LCEVC_CanSendBase,
       LCEVC_CanSendEnhancement,
       LCEVC_CanSendPicture,
       LCEVC_CanReceive,
       LCEVC_BasePictureDone,
       LCEVC_OutputPictureDone,
   };
   LCEVC_ConfigureDecoderIntArray(hdl, "events", static_cast<uint32_t>(kAllEvents.size()), kAllEvents.data());

Note that your vector of events can be in any order.
