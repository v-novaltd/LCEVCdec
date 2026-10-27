Android Integration
===================

This page describes how an Android player gets decoded LCEVC video from LCEVCdec onto the screen. It covers allocating the output pictures that LCEVCdec decodes into, either by the decoder (`Internal Buffer Allocation`_) or from the player's own memory or ``AHardwareBuffer`` objects (`External Buffer Allocation`_), and then displaying them, either with LCEVCdec's render API (`Rendering with the LCEVCdec API`_) or with the player's own renderer (`Rendering outside LCEVCdec`_). The rendering options available depend on how the output pictures were allocated.

Output Buffer Allocation
------------------------

Output pictures passed to :cpp:func:`LCEVC_SendDecoderPicture` can be allocated in three ways. The format of the picture can be found before allocating it with :cpp:func:`LCEVC_PeekDecoderPictureDesc`.

Internal Buffer Allocation
..........................

:cpp:func:`LCEVC_AllocPicture` allocates a picture whose memory is owned by the decoder. This is supported by both pipelines and is the simplest option. The picture's format is set by the decoder to the stream's natural output format (see `Output Formats`_).

External Buffer Allocation
..........................

:cpp:func:`LCEVC_AllocPictureExternal` allocates a picture backed by memory owned by the integration. The memory is described by an :cpp:struct:`LCEVC_PictureBufferDesc`, and can be either plain memory or an ``AHardwareBuffer``.

**External memory** - set :cpp:member:`~LCEVC_PictureBufferDesc::data` (or per-plane descriptors) to point to integration-owned memory. The CPU pipeline decodes directly into this memory; the Vulkan pipeline decodes into its own GPU-visible buffer and copies the result out when the picture is complete. The picture's format is the stream's natural output format, as for internal allocation.

**AHardwareBuffer** - set :cpp:member:`~LCEVC_PictureBufferDesc::accelBuffer` to an ``AHardwareBuffer*`` cast to ``uintptr_t``. The decoder writes directly into the hardware buffer, so the player application can present it with no extra copy. Check the return code of :cpp:func:`LCEVC_AllocPictureExternal` to confirm the pipeline accepted the buffer, and use another mode for buffers it does not accept.

.. code-block:: C++

  LCEVC_PictureDesc desc;
  LCEVC_PeekDecoderPictureDesc(decoder, timestamp, &desc);

  LCEVC_PictureBufferDesc bufferDesc = {};
  bufferDesc.accelBuffer.hdl = reinterpret_cast<uintptr_t>(hardwareBuffer);
  bufferDesc.access = LCEVC_Access_Write;

  LCEVC_PictureHandle picture;
  if (LCEVC_AllocPictureExternal(decoder, &desc, &bufferDesc, nullptr, &picture) != LCEVC_Success) {
      // Fall back to LCEVC_AllocPicture and the render API
  }

Supported hardware buffer formats, and the picture format each is decoded as:

======================== ================ ============================
AHardwareBuffer format   CPU pipeline     Vulkan pipeline
======================== ================ ============================
``Y8Cb8Cr8_420``         ``NV12_8``       ``I420_8``, ``NV12_8`` or
                                          ``NV21_8``
``YCbCr_P010``           \-               ``P010_10``
``R8G8B8A8_UNORM``       \-               ``RGBA_8``
``R10G10B10A2_UNORM``    \-               ``RGBA_10_2_LE``
======================== ================ ============================

In the CPU pipeline (Android API 29 and later) the picture is ``LCEVC_NV12_8`` and the buffer's memory is semi-planar, either NV12 or NV21. The buffer is CPU-locked from allocation until :cpp:func:`LCEVC_FreePicture`, so free the picture before presenting the buffer. When the buffer's memory is NV21, the chroma is reordered from NV12 to NV21 as the picture is freed. The buffer needs ``AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN`` usage.

In the Vulkan pipeline the buffer is imported into Vulkan, and the hardware buffer's format sets the picture's colour format. For ``Y8Cb8Cr8_420`` buffers the memory layout (I420, NV12 or NV21) is detected from the buffer.

Output Formats
..............

**CPU pipeline** - the CPU pipeline writes the stream's natural output format directly:

* If the base picture is ``LCEVC_NV12_8`` or ``LCEVC_NV21_8``, the output has the same 8-bit format.
* Otherwise the output is planar YUV (or ``GRAY``) with the base's chroma subsampling, at the LCEVC enhanced bit depth - for example an ``LCEVC_I420_8`` base with a 10-bit enhancement produces ``LCEVC_I420_10_LE``.

Conversions performed are bit depth conversion from the base to the enhanced depth, and the NV12 to NV21 chroma reorder when writing into an NV21 hardware buffer.

**Vulkan pipeline** - the Vulkan pipeline converts while writing the output picture, depending on the output's format and whether its memory is a buffer or an image:

========================================= ======================= ===============================================
Output format                             Source chroma           Notes
========================================= ======================= ===============================================
Planar 8-bit (``I420_8``, ``I422_8``,     Same as the output      8-bit enhancement.
``I444_8``, ``GRAY_8``)
Planar 10/12/14-bit (``*_10_LE`` etc.)    Same as the output      10, 12 or 14-bit enhancement.
``LCEVC_NV12_8``, ``LCEVC_NV21_8``        4:2:0                   Planar to semi-planar interleave. Even
                                                                  dimensions required.
``LCEVC_P010_10``                         4:2:0                   Even dimensions required.
``LCEVC_RGBA_8``                          4:2:0, 4:2:2 or 4:4:4   YUV to RGB, BT.601 limited range. For
                                                                  hardware buffer outputs.
``LCEVC_RGBA_10_2_LE``                    4:2:0, 4:2:2 or 4:4:4   YUV to RGB, BT.601 limited range. For
                                                                  hardware buffer outputs.
========================================= ======================= ===============================================

The RGBA, P010, and 2-plane YUV conversions write into a Vulkan image, so they are used when the output picture is backed by an ``AHardwareBuffer`` of the matching format. Internally allocated and external memory pictures use the stream's natural output format, as in the CPU pipeline.

Video Rendering
---------------

Once a picture has been decoded, it can be rendered by LCEVCdec or by the player application. The options depend on how the picture was allocated:

=========================== ======================= =================================
Output buffer               LCEVCdec rendering      Player application rendering
=========================== ======================= =================================
Internal allocation         Yes                     Yes, by reading the picture back
External memory             Yes                     Yes, from its own memory
AHardwareBuffer             \-                      Yes, by presenting the buffer
=========================== ======================= =================================

The two can be combined: after LCEVCdec renders a picture the integration still owns it, so the same picture can also be read by the player application.

Rendering with the LCEVCdec API
...............................

The render API copies each output picture into an ``ANativeWindow`` (for example the surface of a ``SurfaceView``). This is the simplest way to get video on screen and works with both the CPU and Vulkan pipelines.

Setup
~~~~~

Request rendering when the decoder is configured, before :cpp:func:`LCEVC_InitializeDecoder`, so that the Vulkan pipeline can enable the instance and device extensions needed for presentation. After initialization, call :cpp:func:`LCEVC_RenderInit` once, then hand the decoder a window with :cpp:func:`LCEVC_RenderSetWindow`:

.. code-block:: C++

  #include <android/native_window_jni.h>

  LCEVC_ConfigureDecoderString(decoder, "pipeline", "auto");
  LCEVC_ConfigureDecoderBool(decoder, "will_render", true);
  LCEVC_InitializeDecoder(decoder);

  if (LCEVC_RenderInit(decoder) != LCEVC_Success) {
      // This pipeline cannot render on this platform - fall back to reading pictures back
  }

  // 'surface' is a jobject android.view.Surface, e.g. from SurfaceHolder.getSurface()
  ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
  LCEVC_RenderSetWindow(decoder, window, false);

The integration owns the window's lifetime, so keep it alive until it has been replaced or cleared. Call :cpp:func:`LCEVC_RenderSetWindow` again whenever the surface is recreated or resized, and with ``nullptr`` before the surface is destroyed, then release the old window with ``ANativeWindow_release``. While no window is set, :cpp:func:`LCEVC_RenderSendPicture` returns :cpp:enumerator:`LCEVC_Again` and the picture is not drawn.

The ``secure`` argument of :cpp:func:`LCEVC_RenderSetWindow` is not yet implemented.

Rendering a Picture
~~~~~~~~~~~~~~~~~~~

Receive a decoded picture as normal, then pass it to :cpp:func:`LCEVC_RenderSendPicture` with the timestamp it was decoded with:

.. code-block:: C++

  LCEVC_PictureHandle output;
  LCEVC_DecodeInformation info;
  if (LCEVC_ReceiveDecoderPicture(decoder, &output, &info) == LCEVC_Success) {
      LCEVC_RenderSendInformation sendInfo = {};
      sendInfo.rotation = 0; // 0, 90, 180 or 270 degrees clockwise

      LCEVC_RenderSendPicture(decoder, info.timestamp, output, &sendInfo, 0);

      // The picture is still owned by the integration - it can be locked and read here
      // before being reused for the next decode or freed.
      LCEVC_FreePicture(decoder, output);
  }

Always receive the picture before rendering it: :cpp:func:`LCEVC_ReceiveDecoderPicture` blocks until the decode has finished writing the picture, whereas :cpp:func:`LCEVC_RenderSendPicture` draws whatever the picture contains at the time it is called.

:cpp:func:`LCEVC_RenderSendPicture` copies the picture into the window before it returns, so the picture can be reused or freed immediately afterwards.

Current limitations of the render API:

* ``delayUs`` is not implemented - all pipelines render immediately and log a warning if a delay is requested.
* :cpp:func:`LCEVC_RenderReceivePicture` is not implemented and always returns :cpp:enumerator:`LCEVC_NotSupported`.

CPU Pipeline Rendering
~~~~~~~~~~~~~~~~~~~~~~

The CPU pipeline renders with ``ANativeWindow_lock`` / ``ANativeWindow_unlockAndPost``, copying the picture into the window's next buffer on the calling thread:

* The window buffers are configured to the picture's size in the semi-planar YUV 4:2:0 format (``HAL_PIXEL_FORMAT_YCrCb_420_SP``, i.e. NV21). The luma plane is copied directly, and the chroma is reordered into NV21 while copying. Colour conversion to RGB is left to the system compositor.
* On devices without CPU-writable YUV window buffers, the pipeline renders through ``WINDOW_FORMAT_RGBA_8888`` buffers instead, converting YUV to RGB on the CPU. This uses the picture's colour range, and BT.709 coefficients when the stream signals BT.709 (or when the matrix is unspecified and the picture is at least 1280 pixels wide), otherwise BT.601. The YUV path is the faster of the two and is used whenever the device supports it.
* Supported formats are 8-bit ``LCEVC_NV12_8`` and ``LCEVC_I420_8``.
* Display rotation is handled by the system compositor, which rotates the posted window buffers for the display, so ``rotation`` can be left at 0 (a non-zero value logs a warning). The compositor also scales the buffer to fill the view, so size the view to the video's aspect ratio.

Vulkan Pipeline Rendering
~~~~~~~~~~~~~~~~~~~~~~~~~

The Vulkan pipeline creates a Vulkan surface and swapchain on the window and draws each picture with a graphics pipeline:

* The picture's planes are uploaded into three single-channel textures (``R8`` for 8-bit, ``R16`` for higher bit depths) and converted from YUV to RGB in a fragment shader using BT.601 coefficients and the picture's colour range.
* The stream's dither, if enabled, is applied in the fragment shader at render time.
* ``rotation`` is applied by the decoder in 90 degree steps, as the swapchain renders directly in the surface's current orientation. Pass the display rotation here. The picture is stretched to fill the swapchain, so size the view to the video's aspect ratio.
* The swapchain uses ``VK_FORMAT_B8G8R8A8_SRGB`` when available, and ``VK_PRESENT_MODE_MAILBOX_KHR`` when available, otherwise ``VK_PRESENT_MODE_FIFO_KHR``.
* Supported formats are the 4:2:0 formats ``LCEVC_I420_8``, ``LCEVC_NV12_8`` and ``LCEVC_I420_10_LE``.
* Internally allocated and external memory pictures can be rendered. Pictures backed by an ``AHardwareBuffer`` are presented directly by the player application (see `Rendering outside LCEVCdec`_).

Rendering outside LCEVCdec
..........................

Presenting the decoded pictures is the player application's responsibility in this mode, so the details depend on the player's own rendering stack. The notes below share approaches that work well with LCEVCdec.

Presenting an AHardwareBuffer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Pictures allocated with an ``AHardwareBuffer`` are decoded straight into a buffer the player application already holds, so it can present that buffer with no extra copy - for example by attaching it to an ``ASurfaceControl`` with ``ASurfaceTransaction_setBuffer``, or by importing it as a GPU texture. Present the buffer once the picture has been received with :cpp:func:`LCEVC_ReceiveDecoderPicture`, and in the CPU pipeline once it has also been freed with :cpp:func:`LCEVC_FreePicture`, which releases the decoder's CPU lock on the buffer.

Reading the Picture Back
~~~~~~~~~~~~~~~~~~~~~~~~

Internally allocated pictures, and pictures already rendered by LCEVCdec, can be locked to read the decoded pixels - for example to upload them to the player's own renderer, take a screenshot or feed another consumer:

.. code-block:: C++

  LCEVC_PictureLockHandle lock;
  if (LCEVC_LockPicture(decoder, output, LCEVC_Access_Read, &lock) == LCEVC_Success) {
      uint32_t planeCount = 0;
      LCEVC_GetPicturePlaneCount(decoder, output, &planeCount);
      for (uint32_t plane = 0; plane < planeCount; ++plane) {
          LCEVC_PicturePlaneDesc planeDesc;
          LCEVC_GetPictureLockPlaneDesc(decoder, lock, plane, &planeDesc);
          // planeDesc.firstSample, planeDesc.rowByteStride ...
      }
      LCEVC_UnlockPicture(decoder, lock);
  }

For pictures in external memory the player application can also read its own memory directly once the picture has been received.
