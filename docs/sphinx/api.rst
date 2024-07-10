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

Most functions take a ``decHandle`` parameter for a decoder object, functions will return LCEVC_InvalidParam if the handle is invalid or LCEVC_Uninitialised if the decoder isn't initialised before running decoder-specific methods.

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

For a list of options settable with this function, see :ref:`Configurable Options`

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

Configurable Options
--------------------

Most options are set to their optimal value automatically from detection of the stream, below are some parameters you may consider setting manually. Options are set with ``LCEVC_ConfigureDecoder`` calls.

``log_level``
.............

Type: ``int``

Sets the amount of logging output by the decoder based on severity levels:

- 0 - Disabled: No logs are emitted
- 1 - Fatal: Very bad and extremely rare (at most, once per run). Problems which will cause crashes, or otherwise cause the decoder to shut down unexpectedly.
- 2 - Error: Bad and very rare. Problems which either (1) entirely prevent the decoder from performing a task (e.g. a blank screen), or (2) are not *serious* per se, but are nonetheless clearly wrong (e.g. taking a valid Picture and giving it a negative width)
- 3 - Warning: Bad and somewhat rare. Problems which will have adverse affects on either performance or visual quality. The decoder will work, but badly.
- 4 - Info: Not bad, but rare and useful. Important information which any user would want to know, such as version data and configuration.
- 5 - Debug: Not bad, somewhat rare, and useful. Things like resolution changes, dropped frames, and queues reaching their capacity. Should be *less frequent* than once per-frame.
- 6 - Trace: Not bad, nor rare, but useful. This is the only level which is allowed to emit information once per frame. Signposts significant steps in the code path. Typically, externally users will not appreciate these logs, but LCEVCdec developers will.

Note that, even at the Trace level, some types of logs should *never* be seen. For example, the Trace level can show logs that *all* developers understand, but not logs that are understood by only 1 developer (or worse, none). Raw pointer addresses should be exceedingly rare. There should never be per-pixel or per-row logs.

At the Debug and Trace level, timestamps are provided in nanoseconds. At all other levels, timestamps are provided in microseconds.

Logging is enabled as soon as the ``log_level`` option is set. The ``LCEVC_CreateDecoder`` function cannot produce any logging messages - it will ony allocate memory, and use its the return code to show success. The real work of creating the decoder will happen during ``LCEVC_InitializeDecoder``, and will produce the requested diagnaostics and status through the configured logging.

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
