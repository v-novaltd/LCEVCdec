###############################
Enhancement Bitstream Processor
###############################

As well as a high-level C++ :ref:`API<api:API>` for full integrations where the built-in Pixel Processing libraries are supported and performant, the enhancement module (LDE) is provided in C for low-level integrations where pixel processing operations require a bespoke solution. In this mode functionality is provided to decode raw LCEVC NAL units to the :ref:`command buffer<enhancement:Command Buffers Intro>` intermediate residual format via the headers in ``src/enhancement/include``. Note that although this module is structured as an internal component of LCEVCdec, the headers are used externally by downstream projects and care is taken to maintain a stable interface. Any changes to this interface will be communicated via release notes.

Enhancement
###########

After sequencing the enhancer takes ordered NAL units and first parses them, then decodes them to either CPU or GPU cmdbuffer formats.

Config Parsing
--------------

Configs are split into global and frame structures. A new frame structure is required for each frame and it also contains the compressed data that makes up the residuals however a global config may have a much longer lifetime. The global config contains state that must be re-input to :cpp:func:`ldeConfigsParse` for each subsequent frame, a flag ``globalConfigModified`` is set to true if anything changed in the global config. The frame config should also be reused to reduce memory allocations but a freshly initialized or copied frame config can also be used.

Config Parsing - Data Structures
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenstruct:: LdeGlobalConfig
   :members:

.. doxygenstruct:: LdeFrameConfig
   :members:

Config Parsing - Functions
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenfunction:: ldeGlobalConfigInitialize

.. doxygenfunction:: ldeFrameConfigInitialize

.. doxygenfunction:: ldeConfigsReleaseFrame

.. doxygenfunction:: ldeConfigsParse

Config Pool
-----------

The config pool keeps track of multiple frame configs when multiple frames are being processed in a pipeline. It keeps a shared pool of global configs so that multiple identical copies are not stored. Only once all frames associated with a global config are released, is the global config released.

Config Pool - Data Structures
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenstruct:: LdeConfigPool
   :members:

Config Pool - Functions
^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenfunction:: ldeConfigPoolInitialize

.. doxygenfunction:: ldeConfigPoolRelease

.. doxygenfunction:: ldeConfigPoolFrameInsert

.. doxygenfunction:: ldeConfigPoolFrameRelease

Decoding
--------

.. doxygenfunction:: ldeDecodeEnhancement

Command Buffers Intro
---------------------

Command buffers (cmdbuffers) are a memory-efficient but uncompressed representation of residuals within memory. The command buffers allow decoupling of the decompression via :cpp:func:`ldeDecodeEnhancement` and the application of the residuals which is considered a 'pixel processing' task and is often better handled by different hardware like a GPU. They come in two flavours, CPU and GPU each with optimisations to be efficiently read sequentially or in parallel. Both are designed around the specific format of LCEVC residuals and the block layout of their application on the temporal plane.

Command buffers are heavily linked to the transform unit (TU) layout of LCEVC residuals. A transform unit contains either 4 or 16 residuals, which is either 2x2 pixels for a DD transform type or 4x4 pixels for a DDS transform type (:cpp:member:`LdeGlobalConfig::transform`). With temporal mode on in a stream (:cpp:member:`LdeGlobalConfig::temporalEnabled` is true), the frame is partitioned into 32x32 pixel 'blocks' - therefore each block contains either 64 or 256 TUs for DDS or DD transform types respectively.

The following diagram shows the 'block raster order' of a 76x72 pixel DDS frame. The black borders show the border of each block and the grey squares represent each DDS TU. The block raster order starts at the top left corner and proceeds right until the edge of the first block where it wraps to the next row of the first block. At the end of the first block it jumps to the top left TU of the second block in the row of blocks. At the end of the row of blocks we jump to the first block in the second row of blocks and so on. Importantly at the right hand edge the index of each TU behaves as if there is always a full block, therefore the frame size is essentially rounded **up** to the nearest 32px.

.. image:: ../img/tu_order.svg

Note that when :cpp:member:`LdeGlobalConfig::temporalEnabled` is false the block raster order is not followed and the frame follows a standard raster order from the top left across the entire width of the frame (not rounded up to the nearest 32px). A GPU command buffer mask will be 265 TUs long, wrapping at the right edge of the frame to the next row of TUs, the final command reaching off the right of the last row will always have a zero mask for non-existent TUs.

CPU Command Buffers
-------------------

The CPU command buffer format consists of a single block of memory that doubles in size when at capacity. It is populated with data from both ends, with commands at the front and residuals at the rear. A standard command consists of a single byte where the first two bits signal the operation to apply at the TU location and the remaining six bits represent a 'jump value' that signals how many TUs to jump to get to the location.

.. image:: ../img/cmdbuffer_cpu.svg

Residuals added from the rear of the buffer are always in signed 16-bit format. Although the residual is added to the rear of the buffer, each int16 residual within a TU is facing forwards in the buffer. The diagram shows 16 residuals added for each ADD or SET command which is correct for DDS, 4 16-bit residuals are added for each ADD or SET command in DD transform type mode. Residuals can be negative values so that when added to an existing pixel the value is reduced.

A six bit jump value is not sufficient to access the entire frame so a subsequent two or three byte jump can be signalled with ``111110`` or ``111111`` respectively - referred to as a 'big jump' or 'extra big jump'.

The two bit command value is one of four options: ADD, SET, SETZERO or CLEAR they act as follows

* **ADD:** At the specified TU location, add each residual to the existing pixel value. ADD commands will not be present for LOQ1 command buffers or when temporal is off.
* **SET:** At the specified TU location, replace the current pixel values with the residuals.
* **SETZERO:** At the specified TU location, set all 4 or 16 pixels of the TU to zero. SETZERO commands will only be present when applying to a temporal plane to reset existing values.
* **CLEAR:** At the specified TU location - which will always be the start of a block, set all 64 or 256 pixels of the block to zero. CLEAR commands will only be present when applying to a temporal plane to reset exiting values. CLEAR commands can have a jump value of zero and a subsequent SET command can add a residual to the newly cleared location.

The populated cmdbuffer structure should be reused where possible after consumption. After several frames it should finish growing to it's final size for the stream, reusing the same buffers means that they don't have to 're-grow' and therefore reallocate the memory each time. Calling :cpp:func:`ldeCmdBufferCpuReset` will reset the structure for the next frame. Several command buffers can be in rotation at the same time for pipelining.

During :cpp:func:`ldeCmdBufferCpuInitialize` a number of entry points can be defined, if > 0 entry points are given then :cpp:func:`ldeCmdBufferCpuSplit` will be called as part of :cpp:func:`ldeDecodeEnhancement`. This creates pointers to positions within the generated command buffer at roughly even divisions. This allows multiple threads to apply the buffer to a frame, each with an even number of commands to work through. A maximum of 16 entrypoints can be requested. A slight performance penalty is incurred to generate the entrypoints, set to zero if they are not required.

CPU Command Buffers - Data Structures
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenstruct:: LdeCmdBufferCpu
   :members:

.. doxygenstruct:: LdeCmdBufferCpuStorage
   :members:

.. doxygenstruct:: LdeCmdBufferCpuEntryPoint
   :members:

.. doxygenenum:: LdeCmdBufferCpuCmd

CPU Command Buffers - Functions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenfunction:: ldeCmdBufferCpuInitialize

.. doxygenfunction:: ldeCmdBufferCpuFree

.. doxygenfunction:: ldeCmdBufferCpuReset

.. doxygenfunction:: ldeCmdBufferCpuAppend

.. doxygenfunction:: ldeCmdBufferCpuSplit

CPU Command Buffers - Helpers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenfunction:: ldeCmdBufferCpuGetResidualSize

.. doxygenfunction:: ldeCmdBufferCpuGetCommandsSize

.. doxygenfunction:: ldeCmdBufferCpuGetSize

.. doxygenfunction:: ldeCmdBufferCpuIsEmpty

GPU Command Buffers
-------------------

The GPU command buffer format consists of two buffers :cpp:member:`LdeCmdBufferGpu::commands` and :cpp:member:`LdeCmdBufferGpu::residuals`. The command buffer is populated with 64-bit :cpp:struct:`LdeCmdBufferGpuCmd` s, each containing the information to apply an operation to a 32x32 pixel block. The operation is similar to a CPU command buffer command with the exception of a optimized 'clear and set'. The :cpp:member:`LdeCmdBufferGpuCmd::blockIndex` specifies which block to act on and the :cpp:member:`LdeCmdBufferGpuCmd::dataOffset` specifies the byte offset into the residual buffer to find the residual data for ADD, SET and CLEARANDSET commands.

The GPU command buffer is not as memory efficient as it's CPU counterpart however as each command acts on an entire block at once rather than a single TU, it can be processed much faster by GPU hardware. Importantly as commands have a fixed bit width, an application function can jump to the start of any command and apply it without context.

.. image:: ../img/cmdbuffer_gpu.svg

Note that the above diagram is for a DDS stream, a DD GPU command buffer will have a bitmask of 4 x uint64 to signal the 256 TUs in a DD block. Each DD residual is also only 4 x int16 to cover the 4 pixels in each DD TU.

The two bit command value is one of four options: ADD, SET, SETZERO or CLEARANDSET they act as follows

* **ADD:** At the specified block, add each residual to the existing pixel value. ADD commands will not be present for LOQ1 command buffers or when temporal is off.
* **SET:** At the specified block, replace the current pixel values with the residuals.
* **SETZERO:** At the specified block, set all 4 or 16 pixels of the TUs specified by the bitmask to zero. SETZERO commands will only be present when applying to a temporal plane to reset existing values.
* **CLEARANDSET:** At the specified block, first set the entire 32x32 px block to zero. Then set any values specified by the bitmask as with a standard SET operation. CLEARANDSET commands will only be present when applying to a temporal plane to reset exiting values

Note that ADD, SET and SETZERO operations can all be applied to the same block (aka. have the same block index) however they will **never** act on the same TUs within that block - the logical AND of their bitmasks will always be zero. A CLEARANDSET always be the only operation to apply to a given block.

Also note that ADD, SET and CLEARANDSET require residuals to be read from the residual buffer. These residuals will be contiguous within the residual buffer for a given command/block and can therefore be easily copied using :cpp:member:`LdeCmdBufferGpuCmd::bitCount` to determine the size. For SETZERO commands, :cpp:member:`LdeCmdBufferGpuCmd::dataOffset` will always be zero as no residuals are required.

GPU Command Buffers - Data Structures
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenstruct:: LdeCmdBufferGpu
   :members:

.. doxygenstruct:: LdeCmdBufferGpuCmd
   :members:

.. doxygenstruct:: LdeCmdBufferGpuBuilder

.. doxygenenum:: LdeCmdBufferGpuOperation

GPU Command Buffers - Functions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenfunction:: ldeCmdBufferGpuInitialize

.. doxygenfunction:: ldeCmdBufferGpuFree

.. doxygenfunction:: ldeCmdBufferGpuReset

.. doxygenfunction:: ldeCmdBufferGpuAppend

.. doxygenfunction:: ldeCmdBufferGpuBuild

Dimension helpers
-----------------
The :cpp:struct:`LdeGlobalConfig` contains some helpers for working with tiling mode, the following functional helpers also add some additional useful constants derived from these.

.. doxygenfunction:: ldePlaneDimensionsFromConfig

.. doxygenfunction:: ldeTileDimensionsFromConfig

.. doxygenfunction:: ldeTileStartFromConfig

.. doxygenfunction:: ldeTotalNumTiles

Memory
######

The enhancement module (as well as all non-legacy LCEVCdec modules) uses the ``common`` (LDC) memory API to allocate, reallocate and free memory. The API allows the C standard library memory functions to be replaced with custom functions depending on platform and toolchain requirements. Use :cpp:func:`ldcMemoryAllocatorMalloc` to return a C standard library populated :cpp:struct:`LdcMemoryAllocator` or build your own as documented below. All the following data structures, functions and macros can be included from ``LCEVC/common/memory.h``.

Data Structures
---------------

.. doxygenstruct:: LdcMemoryAllocator
   :members:

.. doxygenstruct:: LdcMemoryAllocatorFunctions
   :members:

.. doxygenstruct:: LdcMemoryAllocation
   :members:

Functions and Macros
--------------------

.. doxygenfunction:: ldcMemoryAllocatorMalloc

.. doxygenfunction:: ldcMemoryAllocate

.. doxygendefine:: VNAllocate
.. doxygendefine:: VNAllocateArray
.. doxygendefine:: VNAllocateZero
.. doxygendefine:: VNAllocateZeroArray
.. doxygendefine:: VNAllocateAligned
.. doxygendefine:: VNAllocateAlignedArray
.. doxygendefine:: VNAllocateAlignedZero
.. doxygendefine:: VNAllocateAlignedZeroArray

.. doxygenfunction:: ldcMemoryReallocate

.. doxygendefine:: VNReallocate
.. doxygendefine:: VNReallocateArray

.. doxygenfunction:: ldcMemoryFree

.. doxygendefine:: VNFree

.. doxygendefine:: VNClear
.. doxygendefine:: VNClearArray


Logger
######

Logging within LCEVCdec is handled within the larger ``diagnostics`` system. The LCEVCdec logger can be reused in downstream integrations after being initialized with :cpp:func:`ldcDiagnosticsInitialize`. Macros are used at log-time and the CMake flag ``VN_SDK_MAXIMUM_LOG_LEVEL`` can be used to compile libraries without logging to reduce binary size of libraries. See the below definitions of functions to setup logging and macros to log strings. All functions and macros can be included from ``LCEVC/common/diagnostics.h`` and ``LCEVC/common/log.h``

Functions and Enums
-------------------

.. doxygenfunction:: ldcDiagnosticsInitialize

.. doxygenfunction:: ldcDiagnosticsLogLevel

.. doxygenenum:: LdcLogLevel

Macros
------

Where possible use the non-'F' logging functions as the string formatting and writing to buffer (stdout by default) is done off the calling thread to reduce logging overhead. The 'F' variants execute the formatting on the calling thread.

.. doxygendefine:: VNLogFatal
.. doxygendefine:: VNLogFatalF
.. doxygendefine:: VNLogError
.. doxygendefine:: VNLogErrorF
.. doxygendefine:: VNLogWarning
.. doxygendefine:: VNLogWarningF
.. doxygendefine:: VNLogInfo
.. doxygendefine:: VNLogInfoF
.. doxygendefine:: VNLogDebug
.. doxygendefine:: VNLogDebugF
.. doxygendefine:: VNLogVerbose
.. doxygendefine:: VNLogVerboseF

Sample
######

The following sample reads an LCEVC .bin format stream, parses it and decodes it to CPU command buffers. An example stream can be found in the repo at ``LCEVCdec/src/enhancement/test/assets/decode_temp_on.bin``.

.. literalinclude:: ../../src/enhancement/test/sample/src/main.cpp
   :language: cpp
