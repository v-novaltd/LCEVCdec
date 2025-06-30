# LCEVCdec Source

## API

 * `api`
  LCEVCdec API - interface admin - hands off real work to pipeline (C++)

## Internal

Modules that are 'beneath' the API

 * `common`
  Common support code internal to LCEVCdec - tracing,memory,logging (C)

 * `enhancement`
  LCEVC bitstream parsing and enhancement decoding to command buffers (C)

 * `pixel_processing`
 Bulk surface operations to be used by pipelines as needed (C)

* `api_utility`
 Copies of some of the `utility` support library code that is used within decoder.
 (Will be integrated into/replaced by `common`)

## Pipeline

Decoding pipelines that are built as shared libraries, and loaded by API according
to configuration.

 * `pipeline_cpu`
 CPU (Scalar and SIMD) implementation of LCEVC pipeline (C++)

 * `pipeline_vulkan`
 GPU implementation of pipeline (C++)

 * `pipeline_metal`
 GPU (Apple) implementation of pipeline (C++)

* `pipeline_legacy`
  Legacy non pipelined CPU implementation that uses core/DPI

## Support

Static libraries that are delivered alongside the API as optional support code for common
operations needed in a decoding integration.

 * `extract`
 Extract LCEVC data from elementary streams (C)

 * `utility`
 Utility functions that are also exposed to integration as a static library (C++)

## Tests

 * `func_tests`
  Functional tests of LCEVCdec API

## Samples

 * `sample_cpp`
  LCEVCdec API sample

## Legacy Code

 * `legacy`
  The old `perseus_...()` DPI

 * `color_conversion`
  Tonemapping and color conversion

 * `overlay_images`
  Branding images used by the legacy DPI

* `sequencer`
  Timecode prediction for timestamps (C)
