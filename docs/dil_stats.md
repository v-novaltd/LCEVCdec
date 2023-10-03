# DIL Stats

The DIL can produce a JSON stats file containing detailed information about the device, the DIL configuration and per-frame timing information.  
To activate the stats collection use the configuration key [stats](./dil_config_json.md#debug-and-stats) and (optionally) specify the name and path of the file to create using [stats_file](./dil_config_json.md#debug-and-stats).  
The JSON file produced is not intended to be human readable, all the data has been stored as tightly packed as possible to save space (whilst still being a JSON text file). A python script `processStats.py` is provided for post processing the JSON file into a CSV file for further processing by a spreadsheet.
When enabled, the software will generate a row of data per frame, the contents of the row will depend on which DIL functions have been called and if everything worked without errors. Each row represents a frame that has been processed. Rows are written to disk from a least recently used FIFO. When everything is working correctly the file will contain rows in correct time order, but if things go wrong, e.g. a frame is not rendered, then the time order is not guaranteed. The python script can sort the results, if required, to try to get the data in correct time order, but even that is not guaranteed to be successful, although this would usually indicate that the client has incorrectly handled the frames.

# Post processing python script

The python script `processStats.py` is provided to help read and process the compact JSON file. By default the script will assume that the JSON needs to be converted into a csv file for importing into the stats excel spreadsheet.  
The script allows for selecting a specific set of columns and a range of rows via its command line. The following command will select the CC, PTS, DecodeStartTime and DecodeEndTime for row 100 with no extra headers of information.  
  `python processStats.py  -f ./stats.json -c "CC, PTS, DecodeStartTime, DecodeEndTime" -n 1 -s 100 -b`

## Command line arguments

| Parameter | Type | Default value | Description |
| :---- | :---- | :----- | :----- |
| b | Flag | off | Controls if the headers and extra data are output. When `on` only the row data is output |
| c | String | "" | Controls the column selection and the order they are output. The default is all columns in a specific order defined in the script. |
| e | Number | 0 | Last frame number to be processed. |
| f | String | "" | The name of the JSON file to be processed. This must be provided |
| n | Number |  | Number of frames to process. |
| o | String | "FrameCount" | The column name to order the output by, an empty string will not sort. |
| s | Number |  | Starting frame to process. Default is first frame in file. |
| x | String | "" | Extra calculated column. See Custom Columns below for more details. |

The `-e` and `-n` parameters can't be used at the same time. The `-e` parameter will take precedence over the `-n` parameter. The last row to process is calculated like this, assuming start_frame = `-s`, end_frame = `-e` and num_frames = `-n`  
`if end_frame == 0 then end =  start_frame + num_frames else end = end_frame`  
With a final check `if end <= start_frame then end = length(frames)`

## Custom Columns

The script can produce a new `custom` column based on a calculation of 2 other columns. The parameter `x` is used to process a JSON string that defines 1 or more `custom` columns. It is in the form '{"<custom name>": { "fields": ["<column name 1>","<column name 2>"], "type": "double", "operation": "subtract"} }' by using a JSON form string many custom columns can be defined at once.

### Custom Column definition

| Tag | Type | Default value | Description |
| :---- | :---- | :----- | :----- |
| fields | Array | [] | An array of 2 column names. The special tag `last.` can be added to the column name to indicate its value should come from the previous row's data |
| operation | String | subtract | Defines the calculation done to produce the new column. Options are `add` : c1 + c2, `subtract` : c1 - c2, `multiply` : c1 * c2, `divide` : c1 / c2 |
| type | String | double | Defines the type of column produced. Options are what are supported by the DIL, e.g. `double`, `string`, `int32_t`, `uint64_t` |

# JSON file

The stats file consists of a number of blocks of data, with the per frame data being the last entry in the file.

## Test information

Tag `TestInfo`.  
This block of data provides information about how the DIL was configured for the playback. See the [DIL configuration](./dil_config_json.md) for more details.  

| Tag | Type | Description |
| :---- | :---- | :----- |
| Dithering | Boolean | The value of the DIL configuration parameter `dithering`. |  
| passthrough mode | Integer | The value of the DIL configuration parameter `passthrough_mode`. |  
| GL Decode | Boolean | The value of the DIL configuration parameter `gl_decode`. |  
| Perseus Surface FP Setting| Integer | The value of the DIL configuration parameter `pss_surface_fp_setting`. |  
| Pipeline Format | Integer | Input pipeline format, if known. |  
| Pre-Calculate | Boolean | The value of the DIL configuration parameter `can_precalculate`. |  
| predicted average method | Integer | The value of the DIL configuration parameter `predicted_average_method`. |  
| Residual Type | Integer | The value of the DIL configuration parameter `loq_residual_image_type`. |  
| stable frame count | Integer | The value of the DIL configuration parameter `stable_frame_count`. |  
| supports protected mode | Boolean | A flag that indicates if the DIL supports protected (DRM) mode. |  
| URI | String | The URI of the stream being played, where known. Not all clients can provided this information. |  
| use LOQ0 | Boolean | The value of the DIL configuration parameter `use_loq0`. |  
| use LOQ1 | Boolean | The value of the DIL configuration parameter `use_loq1`. |  
| UTC Run | String | When the client created the DIL object. This is a date time string in the form YYYY-MM-DD@HH-MM-SS, e.g 2022-03-22@18-17-12. This is in UTC time NOT local time. |  

## Version information

Tag `Version`.

| Tag | Type | Description |
| :---- | :---- | :----- |
| DILVersion | Object | The DIL version information in a `Version object`, see below definition. |
| DPIVersion | Object | The DIL version information in a `Version object`, see below definition. |

### Version object

| Tag | Type | Description |
| :---- | :---- | :----- |
| BuildDate | String | Build date of software. |
| Full | String | Full version information. |

## JSON Config

Tag `JSONConfig`.

| Tag | Type | Description |
| :---- | :---- | :----- |
| final | String | The actual JSON string used by the DIL after, possibly, merging with the contents of the `lcevc_dil_config.json` file. |
| initial | String | The JSON string passed to the `DIL_Create` function. |

## CPU information

Tag `CPUInfo`.  
The format of this information is vendor specific.

| Tag | Type | Description |
| :---- | :---- | :----- |
| CPU | String | CPU identification string. |

## Base Decoder information

Tag `BaseDecoderInfo`.  
This information is not always available and may be unreliable for certain clients, e.g. ExoPlayer. 

| Tag | Type | Description |
| :---- | :---- | :----- |
| Decoder | String | Base decoder being used. |
| HWDecodingEnabled | Boolean | If hardware base decoding has been requested. |
| UsingHWDecoding | Boolean | If hardware base decoding is being used. |

## OpenGL information

Tag `GLInfo`.  
The format of this information is vendor specific.

| Tag | Type | Description |
| :---- | :---- | :----- |
| GLSLVersion | String | OpenGL shading language version. |
| Renderer | String | OpenGL Renderer string. |
| UsingES2 | String | If OpenGL ES2 is being used. |
| Vendor | String | OpenGL vendor string. |
| Version | String | OpenGL Version string. |

## Stats

### Field names

Tag `FieldNames`.

This is a JSON array of field names sorted alphabetically.

### Field types

Tag `FieldTypes`.

This is a JSON array of field types, e.g. `double`, `int32_t`. This is in the same order as the `Field names`

### Frame stats

Tag `stats`

This is a JSON array of JSON arrays. Each row in the outer array represents 1 frames worth of data. Each row contains the data for the columns in `Field names` in the same order.

## Fields

This table defines each stats field available for each frame. By convention fields ending in `Time` are a time, in ms, relative to the creation time of the DIL object, and fields ending in `Duration` are an elapsed time in ms.

The sum of `DecodeFnStartDuration, DecodeFnPrepareDuration, DecodeFnInternalDuration` and `DecodeFnFinishDuration` should match the `DecodeDuration` value. In practice this doesn't happen but they do get close to `DecodeApplyDuration`

| Tag | Type | Description |
| :---- | :---- | :----- |
| BaseKeyFrame | boolean | false | If the frame is an BASE key frame. |
| CC | uint32_t | Input CC value. |
| DecodeApplyDuration | double | Time taken for the complete LCEVC processing. |
| DecodeBaseDuration | double | Time taken for the LCEVC base processing. When in pre-calc mode this is the time to produce the LOQ1 residuals, including any texture upload. |
| DecodeCallbackStartTime | double | Time the `decode` complete callback started. |
| DecodeCallbackEndTime | double | Time the `decode` complete callback finished. |
| DecodeCallbackWaitDuration | double | Time the `decode` complete callback request was waiting on the thread message queue before it started. |
| DecodeCallbackQueueDepth | uint32_t | Callback job queue depth when `decode` job was posted to the queue. |
| DecodeEndTime | double | Time the decode function finished. |
| DecodeFnFinishDuration | double | Time taken at end of the decode function. This times any code not involved in the actual decode operation. |
| DecodeFnInternalDuration | double | Time taken for the decode function. |
| DecodeFnPrepareDuration | double | Time taken at beginning of the decode function to prepare for the decode call. This times any code not involved in the actual decode operation. |
| DecodeFnStartDuration | double | Time taken at start of the decode function. This times any code not involved in the actual decode operation. |
| DecodeFrameDuration | double | Time between successive decode starting calls. |
| DecodeGLColorInfoDuration | double | Time taken to apply color conversion for rgb to yuv conversion. |
| DecodeGLConvertDuration | double | Time taken to convert the input image to the pipeline format. |
| DecodeGLPipelineDuration | double | Time taken to run the GL pipeline. |
| DecodeGLUploadDuration | double | Time taken to do any texture uploads/drawing prior to running the GL pipeline. |
| DecodeHighDuration | double | Time taken for the LCEVC high processing. When in pre-calc mode this is the time to produce the LOQ0 residuals, including any texture upload. |
| DecodeLcevcWaitDuration | double | Time the decode request waited for the LCEVC data to be processed. |
| DecodeOutputDuration | double | Time taken to convert the decode result into the desired output image. |
| DecodePrepareDuration | double | Time taken to prepare for CPU or GPU decode including buffer to texture conversion and program loading for GPU, and copying the buffer for CPU. |
| DecodeRequireByTime | double | Time the decoded frame was required to be ready. A value of 0 indicates this feature is disabled. |
| DecodeResult | int32_t | Result of the `DIL_Decode` or `DIL_DecodeSkipped` call. |
| DecodeSkipped | boolean | If the decode was skipped. |
| DecodeStartTime | double | Time the decode function started. |
| DecodeSFilterDuration | double | Time the SFilter took to run, only available for CPU mode. |
| DecodeUploadLoq0Duration | double | Time taken to upload the LOQ0 texture. When in geometry mode this is the time taken to produce the texture. |
| DecodeUploadLoq1Duration | double | Time taken to upload the LOQ1 texture. When in geometry mode this is the time taken to produce the texture. |
| DecodeUploadTextureDuration | double | Time taken to upload the input image. |
| DecodeUpscaleDuration | double | Time taken for CPU upscale (if in CPU mode). |
| DecodeWaitDuration | double | Time the decode request was waiting on the thread message queue before it started. |
| DeinterlaceCallbackStartTime | double | Time the `deinterlace` complete callback started. |
| DeinterlaceCallbackEndTime | double | Time the `deinterlace` complete callback finished. |
| DeinterlaceCallbackWaitDuration | double | Time the `deinterlace` complete callback request was waiting on the thread message queue before it started. |
| DeinterlaceCallbackQueueDepth | uint32_t | Callback job queue depth when `deinterlace` job was posted to the queue. |
| DeinterlaceEndTime | double | Time the deinterlace function finished. |
| DeinterlaceFnDuration | double | Time taken to run the deinterlace function (when active). |
| DeinterlaceResult | int32_t | Result of the deinterlace function. |
| DeinterlaceStartTime | double | Time the deinterlace function started. |
| DeinterlaceWaitDuration | double | Time the deinterlace request was waiting on the thread message queue before it started. |
| DitherStrength | int32_t | The dither strength used. |
| DumpTime | double | Time the stats data was dumped. |
| FrameCount | uint64_t | Count of every `DIL_Decode` or `DIL_DecodeSkip` call. |
| FrameCountForCC | uint64_t | Frame count for the current input cc value. Reset to 0 every time the input cc changes when a decode/skip occurs |
| GpuDecode | boolean | The frame was decoded using the GPU (true) or CPU (false). |
| IDRFrame | boolean | false | If the frame is an IDR frame. |
| InputBitdepth | uint32_t | Input frame bitdepth. |
| InputColorFormat | uint32_t | Input frame image color format (DIL_ColorFormat). |
| InputColorPrimaries | uint32_t | Input image (base) color info ColorPrimaries, see Image::ColorPrimaries::Enum for more details. |
| InputColorTransfer | uint32_t | Input image (base) color info ColorTransfer, see Image::ColorTransfer::Enum for more details. |
| InputHeight | uint32_t | Input frame height. |
| InputImageType | uint32_t | Input frame image type (DIL_ImageType). |
| InputLimitedRange | uint32_t | Input image (base) color info LimitedRange, see Image::ColorRange::Enum for more details. |
| InputMatrixCoefficients | uint32_t | Input image (base) color info MatrixCoefficients, see Image::MatrixCoefficients::Enum for more details. |
| InputWidth | uint32_t | Input frame width. |
| LcevcBitdepth | uint32_t | Lcevc frame bitdepth. |
| LcevcColorPrimaries | uint32_t | LCEVC color info ColorPrimaries, see Image::ColorPrimaries::Enum for more details. |
| LcevcColorTransfer | uint32_t | LCEVC color info ColorTransfer, see Image::ColorTransfer::Enum for more details. |
| LcevcEndTime | double | Time the LCEVC processing finished. |
| LcevcHeight | uint32_t | Lcevc frame height, conformance window output. |
| LcevcInternalHeight | uint32_t | Height of the Lcevc internal LOQ-0 frame, before the conformance window cropping. |
| LcevcInternalWidth | uint32_t | Width of the Lcevc internal LOQ-0 frame, before the conformance window cropping. |
| LcevcIsValid | boolean | Is the LCEVC data valid. |
| LcevcKeyFrame | boolean | false | If the frame is an LCEVC key frame (global reset). |
| LcevcLimitedRange | uint32_t | LCEVC color info LimitedRange, see Image::ColorRange::Enum for more details. |
| LcevcMatrixCoefficients | uint32_t | LCEVC color info MatrixCoefficients, see Image::MatrixCoefficients::Enum for more details. |
| LcevcNALAddDuration | double | Time to add an LCEVC encoded data payload to the internal storage. It's possible for each NAL block to contain multiple LCEVC data blocks, however this is unusual and to date there is only one LCEVC block per NAL block. |
| LcevcNALProcessDuration | double | Time to process a sequence of NAL units from the source bitstream to extract the LCEVC payload and insert it in the internal storage. This includes the time for LcevcNALAddDuration. |
| LcevcNALSize | uint32_t | Size of the NAL block containing the LCEVC data. |
| LcevcOriginX | uint32_t | X coordinate of the LCEVC conformance window origin. |
| LcevcOriginY | uint32_t | Y coordinate of the LCEVC conformance window origin. |
| LcevcParseDuration | double | Time taken to parse the LCEVC data block. |
| LcevcProcessedQueueDepth | uint32_t | Size of the lcevc processed queue when calculate job posted to queue. |
| LcevcChromaResiduals | boolean | false | If the stream contains chroma plane residuals |
| LcevcQueueDuration | double | Time the LCEVC data was waiting to be used (by the decode/skip function). |
| LcevcQueueProcessedDuration | double | Time the processed data was waiting to be used (by the decode/skip function). |
| LcevcReceivedTime | double | Time the lcevc data was received. |
| LcevcRawQueueDepth | uint32_t | Size of the lcevc unprocessed queue when calculate job posted to queue. |
| LcevcSize | int32_t | Size in bytes of the LCEVC data block. |
| LcevcStartTime | double | Time the LCEVC processing started. |
| LcevcWaitDuration | double | Time the request to process an LCEVC data block was waiting on the queue. |
| LcevcWidth | uint32_t | Lcevc frame width, conformance window output. |
| Loq0Active | boolean | If LOQ0 is active. |
| Loq1Active | boolean | If LOQ1 is active. |
| Loq0Reset | boolean | If LOQ0 residuals should be reset. |
| Loq1Reset | boolean | If LOQ1 residuals should be reset. |
| Loq0Scale | uint32_t | The scaling mode used by LOQ0. |
| Loq1Scale | uint32_t | The scaling mode used by LOQ1. |
| Loq0Temporal | boolean | If temporal (residual accumulation) is active for LOQ0. |
| Loq1Temporal | boolean | If temporal (residual accumulation) is active for LOQ0. |
| OutputBitdepth | uint32_t | Output frame bitdepth. |
| OutputColorFormat | uint32_t | Output frame image color format (DIL_ColorFormat). |
| OutputColorPrimaries | uint32_t | Output image (enhanced) color info ColorPrimaries, see Image::ColorPrimaries::Enum for more details. |
| OutputColorTransfer | uint32_t | Output image (enhanced) color info ColorTransfer, see Image::ColorTransfer::Enum for more details. |
| OutputHeight | uint32_t | Output frame height. |
| OutputImageType | uint32_t | Output frame image type (DIL_ImageType). |
| OutputLimitedRange | uint32_t | Output image (enhanced) color info LimitedRange, see Image::ColorRange::Enum for more details. |
| OutputMatrixCoefficients | uint32_t | Output image (enhanced) color info MatrixCoefficients, see Image::MatrixCoefficients::Enum for more details. |
| OutputWidth | uint32_t | Output frame width. |
| PeakMemory | uint64_t | Program peak memory at time of decode/skip. |
| PTS | int64_t | PTS value for frame. |
| RenderBitdepth | uint32_t | Render frame bitdepth. |
| RenderCallbackStartTime | double | Time the `render` complete callback started. |
| RenderCallbackEndTime | double | Time the `render` complete callback finished. |
| RenderCallbackWaitDuration | double | Time the `render` complete callback request was waiting on the thread message queue before it started. |
| RenderCallbackQueueDepth | uint32_t | Callback job queue depth when `render` job was posted to the queue. |
| RenderColorFormat | uint32_t | Render frame image color format (DIL_ColorFormat). |
| RenderCount | uint64_t | Count of every `DIL_Render` call. |
| RenderDelayDuration | double | Time that render was asked to delay before running. |
| RenderDesiredStartTime | double | Time that the render needed to be run. |
| RenderFrameDuration | double | Time between successive render starting calls. |
| RenderGLConvertDuration | double | Time taken to convert texture ready to render. |
| RenderGLPrepareDuration | double | Time taken to prepare the GPU render process. |
| RenderGLSetupDuration | double | Time taken to setup ready for rendering. |
| RenderHeight | uint32_t | Rendered frame height. |
| RenderImageType | uint32_t | Render frame image type (DIL_ImageType). |
| RenderResult | int32_t | Result of the `DIL_Render` call. |
| RenderPrepareEndTime | double | Time the render prepare (GL drawing) function finished. |
| RenderPrepareInitialiseDuration | double | Time taken to initialise ready for rendering. |
| RenderPrepareStartTime | double | Time the render prepare (GL drawing) function started. |
| RenderSkipped | boolean | If the render was skipped. |
| RenderSwapEndTime | double | Time the render GL swap function finished. |
| RenderSwapStartTime | double | Time the render GL swap function started. |
| RenderWaitDuration | double | Time the render request was waiting on the thread message queue before it started. |
| RenderWidth | uint32_t | Rendered frame width. |
| TIMEHANDLE | uint64_t | DIL's timehandle, combination of input CC and PTS values. |

## `processStats.py` Calculated Fields

The table details the extra calculated fields that the python script adds to it's output

| Tag | Type | Description |
| :---- | :---- | :----- |
| DecodeCallbackDuration | double | Calculated by `DecodeCallbackEndTime` - `DecodeCallbackStartTime` |
| DecodeDuration | double | Calculated by `DecodeEndTime` - `DecodeStartTime` |
| DecodeGapDuration | double | Calculated by `DecodeStartTime` - `last.DecodeEndTime`, this uses the previous row's `DecodeEndTime` |
| DecodeRequiredByGapDuration | double | Calculated by `DecodeRequireByTime` - `last.DecodeRequireByTime`, this uses the previous row's `DecodeRequireByTime` |
| LcevcDuration | double | Calculated by `LcevcEndTime` - `LcevcStartTime` |
| LcevcGapDuration | double | Calculated by `LcevcStartTime` - `last.LcevcEndTime`, this uses the previous row's `LcevcEndTime`  |
| RenderCallbackDuration | double | Calculated by `RenderCallbackEndTime` - `RenderCallbackStartTime` |
| RenderDuration | double | Calculated by `RenderPrepareDuration` + `RenderSwapDuration` |
| RenderDesiredGapDuration | double | Calculated by `RenderDesiredStartTime` - `last.RenderDesiredStartTime`, this uses the previous row's `RenderDesiredStartTime` |
| RenderGapDuration | double | Calculated by `DecodeRequireByTime` - `last.DecodeRequireByTime`, this uses the previous row's `DecodeRequireByTime` |
| RenderPrepareDuration | double | Time the render prepare stage took. Calculated by `RenderPrepareEndTime` - `RenderPrepareStartTime`|
| RenderResizeCallbackDuration | double | Calculated by `RenderResizeCallbackEndTime` - `RenderResizeCallbackStartTime` |
| RenderSwapDuration | double | Time the render swap phase took. Calculated by `RenderSwapEndTime` - `RenderSwapStartTime`|
