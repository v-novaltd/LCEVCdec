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

Most functions take a `decHandle` parameter for a decoder object, functions will return
LCEVC_InvalidParam if the handle is invalid or LCEVC_Uninitialised if the decoder isn't
initialised before running function-specific methods.

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
