/* Copyright (c) V-Nova International Limited 2022. All rights reserved. */
/** \file PerseusDecoder.h
 * C API for Perseus Decoder
 *
 * Perseus decoding proceeds as follows:
 *
 * -# Open a decoder instance
 *   - initialise a perseus_decoder_config object, manually, or using
 *     perseus_decoder_config_init() and then modifying.
 *   - call perseus_decoder_open()
 * -# Process frames
 *   - If you are using external scaling:
 *      -# Call perseus_decoder_parse()
 *      -# (optional) check perseus_decoder_stream for sanity, especially colourspace
 *      -# Call perseus_decoder_decode_base()
 *      -# Perform external upscale
 *      -# Call perseus_decoder_decode_high()
 *   - If you are doing a full software decode:
 *      -# Call perseus_decoder_parse()
 *      -# (optional) check perseus_decoder_stream for sanity
 *      -# Call perseus_decoder_decode()
 * -# Repeat for all frames
 * -# Close the decoder
 *    By calling perseus_decoder_close()
 */
#ifndef PERSEUS_DECODER
#define PERSEUS_DECODER

/* clang-format off */

#include <LCEVC/lcevc.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define VN_DEC_CORE_API VN_LCEVC_PublicAPI

		/*!
 * VN_IMAGE_NUM_PLANES.
 *
 * The number of planes that a perseus_image can have. This does not include alpha, since none of
 * the formats in perseus_colourspace have an alpha plane.
 */	
#define VN_IMAGE_NUM_PLANES 3

/*!
* VN_MDCV_NUM_PRIMARIES.
*
* The number of primaries in the mastering_display_color_volume SEI message.
*/
#define VN_MDCV_NUM_PRIMARIES 3

	/*!
 * perseus_colourspace enum.
 *
 * This enum specifies the colour sub-sampling present in the Perseus stream.
 */
	typedef enum perseus_colourspace
	{
		PSS_CSP_YUV420P,     /**< 8 bits, 3 Planes, Y, U and V Chroma subsampled by 2 */
		PSS_CSP_YUV422P,     /**< 8 bits, 3 Planes, Y, U and V Chroma subsampled horizontally by 2 */
		PSS_CSP_YUV444P,     /**< 8 bits, 3 Planes, Y, U and V no subsampling */
		PSS_CSP_MONOCHROME,  /**< 8 bits, 1 Plane */
		PSS_CSP_UNSUPPORTED, /**< Format unknown / unsupported */
		PSS_CSP_LAST,        /**< Enum terminator */
	} perseus_colourspace;

	/*!
 * perseus_interleaving enum.
 *
 * This enum specifies the colour interleaving present in an image.
 */
	typedef enum perseus_interleaving
	{
		PSS_ILV_NONE, /**< In this image, all colour components are in their own planes */
		PSS_ILV_YUYV, /**< In this image, YUV422 is a single plane of YUYV */
		PSS_ILV_NV12, /**< In this image, YUV420 is two planes, Y and UV */
		PSS_ILV_UYVY, /**< In this image, YUV422 is single planes, 2pixels=4bytes UYVY */
		PSS_ILV_RGB,  /**< In this image, RGB is a single plane of RGB */
		PSS_ILV_RGBA, /**< In this image, RGBA is a single plane of RGBA */
	} perseus_interleaving;

	/*!
 * perseus_bitdepth enum.
 *
 * This enum specifies the colour bitdepth present in the image. Only valid bitdepths are listed (so no "invalid/final" enum value)
 */
	typedef enum perseus_bitdepth
	{
		PSS_DEPTH_8,
		PSS_DEPTH_10,
		PSS_DEPTH_12,
		PSS_DEPTH_14
	} perseus_bitdepth;

	/*!
 * perseus_upsample enum.
 *
 * This enum specifies the type of upsampling required, used by external upsamplers.
 */
	typedef enum perseus_upsample
	{
		PSS_UPS_DEFAULT,
		PSS_UPS_NEAREST,
		PSS_UPS_BILINEAR,
		PSS_UPS_BICUBIC,
		PSS_UPS_BICUBICPREDICTION,
		PSS_UPS_MISHUS,
		PSS_UPS_LANCZOS,
		PSS_UPS_MODIFIEDCUBIC,
		PSS_UPS_ADAPTIVECUBIC,
	} perseus_upsample;

	/*!
 * perseus_picture_type enum.
 *
 * This enums specifies the type of picture for the current picture.
 */
	typedef enum perseus_picture_type
	{
		PSS_FRAME,
		PSS_FIELD,
	} perseus_picture_type;

	/*!
 * \brief Error codes
 */
	typedef enum perseus_error_codes
	{
		PSS_PERSEUS_UNKNOWN_ERROR,
		PSS_PERSEUS_API_CALL_ERROR,
		PSS_PERSEUS_IMAGE_ERROR,
		PSS_PERSEUS_STREAM_CORRUPT,
		PSS_PERSEUS_MEMORY_ERROR,
		PSS_PERSEUS_INTERNAL_ERROR,
	} perseus_error_codes;

	/*!
 * \brief Debug modes.
 */
	typedef enum perseus_debug_mode
	{
		NO_DEBUG_MODE,
		HIGHLIGHT_RESIDUALS,
	} perseus_debug_mode;

	/*!
 * \brief Pipeline modes. This controls the operating model of the decoder to either
 *		  behave with high speed, or high precision.
 */
	typedef enum perseus_pipeline_mode
	{
		PPM_SPEED,    /**< The decoder attempts to utilise less bandwidth in this mode. */
		PPM_PRECISION /**< The decoder attempts to retain maximum numerical accuracy in this mode. */
	} perseus_pipeline_mode;

	/*!
 * \brief perseus_surface_format enum.
 *		  This enums specifies the type of surface that the perseus residuals will be written to.
 */
	typedef enum perseus_surface_format
	{
		PSS_SURFACE_S16,
		PSS_SURFACE_U8
	} perseus_surface_format;

	/*!
 * \brief Perseus dither type enum. This specifies the type of dithering to be applied
 */
	typedef enum perseus_dither_type
	{
		PSS_DITHER_NONE,
		PSS_DITHER_UNIFORM
	} perseus_dither_type;

	/*!
 * \brief Perseus s mode enum. This specifies the mode of s to be applied
 */
	typedef enum perseus_s_mode
	{
		PSS_S_MODE_DISABLED,
		PSS_S_MODE_IN_LOOP,
		PSS_S_MODE_OUT_OF_LOOP
	} perseus_s_mode;

	/*!
 * \brief SIMD types. This controls the behaviour of SIMD utilisation of the decoder.
 */
	typedef enum perseus_simd_type
	{
		PSS_SIMD_AUTO,    /**< Auto-detects best SIMD code-path based upon architecture */
		PSS_SIMD_DISABLED /**< Disables all SIMD code-paths */
	} perseus_simd_type;

	/*!
	* \brief Scaling mode enum. This specifies the mode of scaling to utilise at each level.
	*/
	typedef enum perseus_scaling_mode
	{
		PSS_SCALE_0D,
		PSS_SCALE_1D,
		PSS_SCALE_2D
	} perseus_scaling_mode;

	/*!
	* \brief LOQ index enum. This provides specialisations for data at a given LOQ.
	*        The high/base terminology can be mapped to LOQ_0/LOQ_1 respectively.
	*        LOQ_2 is a special case for when `perseus_global_config.scaling_modes[PSS_LOQ_1]
	*        != PSS_SCALE_0D`
	*/
	typedef enum perseus_loq_index
	{
		PSS_LOQ_0,
		PSS_LOQ_1,
		PSS_LOQ_2
	} perseus_loq_index;

	/*!
 * \brief Perseus buffer format information
 */
	typedef struct perseus_buffer_info
	{
		uint32_t size[VN_IMAGE_NUM_PLANES]; /**< buffer size in pixels for, PSS_LOQ_0, PSS_LOQ_1, and PSS_LOQ_2 (if required) */
		perseus_surface_format format; /**< buffer format */
		uint8_t using_external_buffers; /**< whether or not an external buffer is expected to be used */
	} perseus_buffer_info;

	/*!
 * \brief Perseus dithering information
 */
	typedef struct perseus_dither_info
	{
		perseus_dither_type dither_type;
		uint8_t             dither_strength;
	} perseus_dither_info;

	/*!
 * \brief Perseus s information
 */
	typedef struct perseus_s_info
	{
		perseus_s_mode mode;
		float          strength;
	} perseus_s_info;

	/*!
	* \brief LCEVC HDR flags. Used to indicate the validity of the various fields in the HDR info structure.
	*/
	typedef enum lcevc_hdr_flags
	{
		PSS_HDRF_MASTERING_DISPLAY_COLOUR_VOLUME_PRESENT = 0x00000001,
		PSS_HDRF_CONTENT_LIGHT_LEVEL_INFO_PRESENT        = 0x00000002
	} lcevc_hdr_flags;

	/*!
	 * \brief LCEVC mastering display colour volume. Seek out the LCEVC standard documentation for explanation
	 *        on these fields and how to use them.
	 */
	typedef struct lcevc_mastering_display_colour_volume
	{
		uint16_t display_primaries_x[VN_MDCV_NUM_PRIMARIES];
		uint16_t display_primaries_y[VN_MDCV_NUM_PRIMARIES];
		uint16_t white_point_x;
		uint16_t white_point_y;
		uint32_t max_display_mastering_luminance;
		uint32_t min_display_mastering_luminance;
	} lcevc_mastering_display_colour_volume;

	/*!
	 * \brief LCEVC content light level. Seek out the LCEVC standard documentation for explanation
	 *        on these fields and how to use them.
	 */
	typedef struct lcevc_content_light_level
	{
		uint16_t max_content_light_level;
		uint16_t max_pic_average_light_level;
	} lcevc_content_light_level;

	/*!
	 * \brief LCEVC HDR info. This contains additional info regarding HDR configuration that may be 
	 *        signaled in the LCEVC bitstream.
	 */
	typedef struct lcevc_hdr_info
	{
		uint32_t flags; /**< Combination of `lcevc_hdr_flags` that can be inspected for data-validity. */
		lcevc_mastering_display_colour_volume mastering_display; /**< Valid if `PSS_HDRF_MASTERING_DISPLAY_COLOUR_VOLUME_PRESENT` flag is set */
		lcevc_content_light_level content_light_level; /**< Valid if `PSS_HDRF_CONTENT_LIGHT_LEVEL_INFO_PRESENT` flag is set */
	} lcevc_hdr_info;

	/*!
	 * \brief LCEVC VUI flags. Used to indicate the validity of the various fields in the VUI info structure
	 */
	typedef enum lcevc_vui_flags
	{
		PSS_VUIF_ASPECT_RATIO_INFO_PRESENT        = 0x00000001,
		PSS_VUIF_OVERSCAN_INFO_PRESENT            = 0x00000010,
		PSS_VUIF_OVERSCAN_APPROPRIATE             = 0x00000020,
		PSS_VUIF_VIDEO_SIGNAL_TYPE_PRESENT        = 0x00000100,
		PSS_VUIF_VIDEO_SIGNAL_FULL_RANGE_FLAG     = 0x00000200,
		PSS_VUIF_VIDEO_SIGNAL_COLOUR_DESC_PRESENT = 0x00000400,
		PSS_VUIF_CHROMA_LOC_INFO_PRESENT          = 0x00001000,
	} lcevc_vui_flags;

	/*!
	 * \brief LCEVC VUI video format.
	 */
	typedef enum lcevc_vui_video_format
	{
		PSS_VUI_VF_COMPONENT,
		PSS_VUI_VF_PAL,
		PSS_VUI_VF_NTSC,
		PSS_VUI_VF_SECAM,
		PSS_VUI_VF_MAC,
		PSS_VUI_VF_UNSPECIFIED,
		PSS_VUI_VF_RESERVED0,
		PSS_VUI_VF_RESERVED1
	} lcevc_vui_video_format;

	/*!
	 * \brief LCEVC VUI info. This contains the VUI info signaled in the LCEVC bitstream. More information
	 *        on what these parameters mean can be found in the LCEVC standard documentation.
	 */
	typedef struct lcevc_vui_info
	{
		uint32_t flags; /**< Combination of `lcevc_vui_video_format` that can be inspected for data-validity or
						     sub-flag presence. */

		/* Aspect ratio info. Valid if `PSS_VUIF_ASPECT_RATIO_INFO_PRESENT` flag is set. */
		uint8_t  aspect_ratio_idc;
		uint16_t sar_width;
		uint16_t sar_height;

		/* Video signal type - Valid if `PSS_VUIF_VIDEO_SIGNAL_TYPE_PRESENT` flag is set. */
		lcevc_vui_video_format video_format;
		uint8_t                colour_primaries;
		uint8_t                transfer_characteristics;
		uint8_t                matrix_coefficients;

		/* Chroma loc info - Valid if `PSS_VUIF_CHROMA_LOC_INFO_PRESENT` flag is set. */
		uint32_t chroma_sample_loc_type_top_field;
		uint32_t chroma_sample_loc_type_bottom_field;
	} lcevc_vui_info;


	typedef struct lcevc_conformance_window_plane
	{
		uint16_t left;    /**< Number of pixels from the left edge to crop for a plane. */
		uint16_t right;   /**< Number of pixels from the right edge to crop for a plane. */
		uint16_t top;     /**< Number of pixels from the top edge to crop for a plane. */
		uint16_t bottom;  /**< Number of pixels from the bottom edge to crop for a plane. */
	} lcevc_conformance_window_plane;

	/*!
	 * \brief LCEVC conformance window. This contains the conformance window scaled
	 *        accordingly for each plane based upon the `colourspace` setting in
	 *        perseus_decoder_stream. More information on what these parameters 
	 *        mean can be found in the LCEVC standard documentation.
	 */
	typedef struct lcevc_conformance_window
	{
		uint8_t                        enabled;   /**< Window is enabled and should be applied */
		lcevc_conformance_window_plane planes[VN_IMAGE_NUM_PLANES]; /**< Window for each plane scaled based on `colourspace`. */
	} lcevc_conformance_window;

	/*!
	 * \brief Perseus global config block information
	 */
	typedef struct perseus_global_config
	{
		uint8_t              global_config_set; /**< 1 if global config is set */
		uint8_t              nal_idr_set; /**< 1 if NAL type is IDR */
		uint32_t             width;             /**< Height of output picture */
		uint32_t             height;            /**< Width of output picture */
		uint8_t              num_planes;
		uint8_t              num_layers;
		perseus_colourspace  colourspace;  /**< colourspace of this stream */
		perseus_bitdepth     bitdepths[VN_IMAGE_NUM_PLANES]; /**< bit-depth for each level. Indexed with perseus_loq_index. */
		uint8_t              use_predicted_average; /**< 1 if the use of predicted-average computation is signalled, or 0 otherwise. NOTE: some non-standard upscaling kernels have predicted-average computation pre-baked */
		uint8_t              temporal_use_reduced_signalling;
		uint8_t              temporal_enabled; /**< 0: temporal is disabled, 1: temporal is enabled, >1: future use */
		perseus_upsample     upsample;         /**< upsample type required */
		uint8_t              use_deblocking;
		perseus_scaling_mode scaling_modes[2]; /**< scaling mode used for each level. Should be indexed with PSS_LOQ_0 or PSS_LOQ_1. */
		uint8_t              temporal_step_width_modifier;
		uint8_t              chroma_stepwidth_multiplier;
	} perseus_global_config;

	/*!
	 * \brief Perseus decoded stream information
	 */
	typedef struct perseus_decoder_stream
	{
		perseus_global_config    global_config;
		perseus_picture_type     pic_type;       /**< Picture type , frame or field */
		perseus_dither_info      dither_info;
		perseus_s_info           s_info;
		uint64_t                 base_hash;
		uint8_t                  loq_enabled[2]; /**< 1 if LOQ is enabled, 0 otherwise, indexed using perseus_loq_index. */
		uint8_t                  loq_reset[2]; /**< 1 if LOQ is reset, 0 otherwise, indexed using perseus_loq_index. */
		perseus_pipeline_mode    pipeline_mode;
		lcevc_hdr_info           hdr_info;
		lcevc_vui_info           vui_info;
		lcevc_conformance_window conformance_window;
	} perseus_decoder_stream;

	/*!
 * \brief Perseus planar image
 */
	typedef struct perseus_image
	{
		void*                plane[VN_IMAGE_NUM_PLANES];  /**< plane pointers				*/
		uint32_t             stride[VN_IMAGE_NUM_PLANES]; /**< line strides in pixels		*/
		perseus_interleaving ilv;       /**< colour interleave flag		*/
		perseus_bitdepth     depth;     /**< colour bit-depth			*/
	} perseus_image;

	/*!
 * \brief Return the library version hash
 */
	VN_DEC_CORE_API const char* perseus_get_version(void);

	/*!
 * \brief Opaque decoder state
 */
	typedef struct perseus_decoder_impl* perseus_decoder;

	typedef enum perseus_decoder_log_type
	{
		PSS_LT_ERROR,
		PSS_LT_INFO,
		PSS_LT_WARNING,
		PSS_LT_DEBUG,
		PSS_LT_UNKNOWN
	} perseus_decoder_log_type;

	/* \brief Function pointer type for log callback messages. */
	typedef void (*perseus_decoder_log_callback)(void* userData, perseus_decoder_log_type type, const char* msg, size_t msgLength);

	/*!
 * \brief Perseus decoder configuration
 */
	typedef struct perseus_decoder_config
	{
		int                   num_worker_threads;         /**< The number of worker threads that the decoder should create and delegate work to. (-1 for auto). */
		perseus_pipeline_mode pipeline_mode;
		uint8_t               use_external_buffers;       /**< Make use of externally allocated buffers to write the perseus surface into */
		perseus_simd_type     simd_type;                  /**< Specify to override the SIMD behaviour of the DPI, by default it will determine the best possible path given the platform. */
		uint8_t               disable_dithering;          /**< Specify to force dithering to be disabled >**/
		uint8_t               use_approximate_pa;         /**< Set to 1 to use an approximate predicted-average computation that is slightly more efficient. This is done by pre-baking the PA computation into the upscaling kernel. Will cause perseus_decoder_get_upsample_kernel to return a kernel with is_pa_pre_baked=1. >**/
		const char*           debug_config_path;          /**< Where the debug config file should be written, null will disable the writing */
		float                 s_strength;                 /**< S strength in the range 0 to 1 to override signalling with 0 to disable, -1 to use whatever is signalled >**/
		uint64_t              dither_seed;                /**< The value used to seed the dither buffer with, a value of 0 will use the current time >**/
		int32_t               dither_override_strength;   /**< If positive, and less than kMaxDitherStrength, this value overrides the stream's dither strength. */
		uint8_t               generate_cmdbuffers;        /**< Set to 1 to enable cmdbuffer generation - when initialised with 1 then no surfaces are written to. */
		uint8_t               logo_overlay_enable;        /**< Specify to enable overlay watermark >**/
		uint16_t              logo_overlay_position_x;    /**< Specify displacement in pixels of left edge of overlay watermark from left edge of video>**/
		uint16_t              logo_overlay_position_y;    /**< Specify displacement in pixels of top edge of overlay watermark from top edge of video>**/
		uint16_t              logo_overlay_delay;         /**< Specify number of frames before displaying overlay>**/
		const char*           dump_path;                  /**< Optional folder path where debug data can be written to. */
		uint8_t               dump_surfaces;              /**< If non-zero then surfaces at key points will be dumped to file. */
		uint8_t               use_old_code_lengths;       /**< If non-zero then the compressed coefficients are processed using the "old" code-lengths logic (do not use unless you know exactly what this means). */
		perseus_decoder_log_callback log_callback;        /**< Optional pointer to receive codec generated log messages */
		void*                        log_userdata;        /**< Pointer to user data that will be pass into the first argument of `log_callback` */
		uint8_t                      use_parallel_decode; /**< If non-zero then `decode_base` and `decode_high` will perform decoding in parallel. */
		const char*                  debug_internal_stats_path; /**< Path to file to write internal stats to. */
	} perseus_decoder_config;

#define LOGO_OVERLAY_POSITION_X_DEFAULT 50
#define LOGO_OVERLAY_POSITION_Y_DEFAULT 28
#define LOGO_OVERLAY_DELAY_DEFAULT 250

	/*!
 * \brief Perseus decoder live configuration.
 */
	typedef struct perseus_decoder_live_config
	{
		uint8_t                use_external_buffers; /**< Make use of externally allocated buffers to write the perseus surface into */
		uint8_t                generate_surfaces;
		perseus_surface_format format; /**< buffer format used for the residuals */
	} perseus_decoder_live_config;

	/*! \brief upsample kernel description */
	typedef struct perseus_kernel
	{
		int16_t k[2][8]; /**< upsample kernels of length 'len', phase
							kernel and 180-degree phase kernel */
		size_t  len;     /**< length (taps) of upsample kernels     */
		uint8_t is_pre_baked_pa; /**< 1 if predicted-average computation has been pre-baked into this kernel, else 0. Separate PA computation should not be applied if this is set to 1. */
	} perseus_kernel;

	/*! \brief Helper struct for the representation of coordinates for command
	 *         buffers */
	typedef struct perseus_cmdbuffer_coords
	{
		int16_t x;
		int16_t y;
	} perseus_cmdbuffer_coords;

	/*! \brief Helper struct for the representation of a command buffers data array
	 *         when the command buffer type is PSS_CBT_2x2. */
	typedef struct perseus_cmdbuffer_2x2
	{
		int16_t values[4]; /**< The 2x2 transform values for the transform unit. */
	} perseus_cmdbuffer_2x2;

	/*! \brief Helper struct for the representation of a command buffers data array
	 *        when the command buffer type is PSS_CBT_4x4. */
	typedef struct perseus_cmdbuffer_4x4
	{
		int16_t values[16]; /**< The 4x4 transform values for the transform unit. */
	} perseus_cmdbuffer_4x4;

	/*!
	 * \brief Enum for identifying the data type within a command buffer.
	 */
	typedef enum perseus_cmdbuffer_type
	{
		PSS_CBT_2x2,  /**< data should be treated as perseus_cmdbuffer_2x2*    */
		PSS_CBT_4x4,  /**< data should be treated as perseus_cmdbuffer_4x4*    */
		PSS_CBT_Clear /**< data should be treated as perseus_cmdbuffer_clear*  */
	} perseus_cmdbuffer_type;

	/*! 
	 * \brief Identifier type used to query the appropriate command buffer from the perseus decoder. 
	 */
	typedef enum perseus_cmdbuffer_id
	{
		PSS_CBID_Intra, /**< Intra commands write their values into the residual buffer. Valid for LOQ-0 and LOQ-1. */
		PSS_CBID_Inter, /**< Inter commands add their values onto the residual buffer. Only valid for LOQ-0. */
		PSS_CBID_Clear  /**< Clear commands reset a 32x32 region of the residual buffer back to 0 (clamped to edge). Only valid for LOQ-0 */
	} perseus_cmdbuffer_id;

	/*!
	 * \brief Buffer containing the commands to apply to a residual buffer for a
	 *        command type. `data` should be cast to the appropriate struct and
	 *        can be indexed by up to `count` number of elements.
	 */
	typedef struct perseus_cmdbuffer
	{
		perseus_cmdbuffer_type          type;   /**< Type of command buffer this is, use this to cast data to one of the above types. */
		const perseus_cmdbuffer_coords* coords; /**< Pointer to contiguous array of coordinates containing `count` entries. */
		const void*                     data;   /**< Optional pointer to contiguous array of residuals, this is an array of int16_t values
                                                     containing 4 or 16 times `count` values. It can be cast to pointers of `perseus_cmdbuffer_2x2` 
                                                     or `perseus_cmdbuffer_4x4` depending on the `type`, this will be null if `type` 
                                                     is `PSS_CBT_Clear`. */
		int32_t                         count;  /**< Number of entries in the coords and data array. */
	} perseus_cmdbuffer;

	/*! \brief Populate an instance of perseus_decoder_config with default values.
 * This configures the decoder to create enough threads to spread work across
 * all hardware threads.
 * This should be called to prepare your instance of perseus_decoder_config before you
 * use it when calling perseus_decoder_open()
 *
 * \param[out] cfg An instance of perseus_decoder_config to populate with default values */
	VN_DEC_CORE_API int perseus_decoder_config_init(perseus_decoder_config* cfg);

	/*! \brief Open a perseus decoder
 *
 * \param[out] pp A freshly allocated perseus decoder
 * \param[in] cfg pointer to config object
 * \returns 0 on success, error number otherwise */
	VN_DEC_CORE_API int perseus_decoder_open(perseus_decoder* pp, const perseus_decoder_config* const cfg);

	/*!\brief Close a perseus decoder
 *
 * \param[in] p The decoder to close.  Pointer is invalid on return.
 * \returns 0 on success, error number otherwise */
	VN_DEC_CORE_API int perseus_decoder_close(perseus_decoder p);

	/*! \brief Deserialises the current frame of perseus data's global 
 * config block and write the data to config if a global config block is present this frame
 *
 * \param[in] p the decoder
 * \param[in] perseus pointer to perseus bitstream
 * \param[in] perseus_len length of perseus bitstream data
 * \param[out] config a structure to store deserialised global config data
 * \returns 0 on success, error number otherwise */
	VN_DEC_CORE_API int perseus_decoder_config_deserialise(perseus_decoder const p,
															const uint8_t* perseus, size_t perseus_len,
															perseus_global_config* config);
	/*! \brief Set up the perseus frame
 *
 * \param[in] p the decoder
 * \param[in] perseus pointer to perseus bitstream
 * \param[in] perseus_len length of perseus bitstream data
 * \param[in] perseus_features bit mask with the features to be enabled
 * \param[out] stm a structure to return parsed stream info (cannot be NULL)
 * \returns 0 on success, error number otherwise */
	VN_DEC_CORE_API int perseus_decoder_parse(perseus_decoder const p, const uint8_t* perseus,
											   size_t perseus_len, perseus_decoder_stream* stm);

	/*! \brief Copy the current stream information
 *
 * \param[in] p the decoder
 * \param[out] stm parsed stream information
 * \returns 0 on success, error number otherwise */
	VN_DEC_CORE_API int perseus_decoder_get_stream(perseus_decoder p, perseus_decoder_stream* stm);

	/*! \brief Decode the base of perseus frame
 *
 * \param[in] p the decoder
 * \param[in/out] base_image pointer to base image
 * \returns 0 on success, error number otherwise */
	VN_DEC_CORE_API int perseus_decoder_decode_base(perseus_decoder const p, const perseus_image* base_image);

	/*! \brief Upscale the base image
 *
 * \param[in] p the decoder
 * \param[in] full_image pointer to output buffer (colour space must match base_image)
 * \param[out] base_image pointer to base image (colour space must match full_image)
 * \param[in] base_loq index of the base image, this is used for determining the scaling behaviour 
 *            based upon the streaming containing a level1 scaling mode.
 * \returns 0 on success, error number otherwise */
	VN_DEC_CORE_API int perseus_decoder_upscale(perseus_decoder const p, const perseus_image* full_image,
												 const perseus_image* base_image, perseus_loq_index base_loq);

	/*! \brief Decode the high res perseus frame
 *
 * \param[in] p the decoder
 * \param[in/out] full_image pointer to output buffer
 * \returns 0 on success, error number otherwise */
	VN_DEC_CORE_API int perseus_decoder_decode_high(perseus_decoder const p, const perseus_image* full_image);

	/*! \brief Apply the s
     *
     * \param[in] p the decoder
     * \param[in/out] src pointer to image to apply s to
     * \returns 0 on success, error number otherwise */
	VN_DEC_CORE_API int perseus_decoder_apply_s(perseus_decoder const p, const perseus_image* image);

	/*! \brief Apply the overlay
     *
     * \param[in] p the decoder
     * \param[in/out] src pointer to image to apply overlay to
     * \returns 0 on success, error number otherwise */
	VN_DEC_CORE_API int perseus_decoder_apply_overlay(perseus_decoder const p, const perseus_image* image);

/*! \brief Perform complete software decode, including upscale
 *		Note: starting with v1.4.X perseus bitstream pointer and length
 *		are not passed anymore, the parse function must be called beforehand
 *
 * \param[in] p the decoder
 * \param[in] perseus pointer to perseus bitstream (can be NULL if parse has been called)
 * \param[in] perseus_len length of perseus bitstream data
 * \param[out] full_image pointer to output buffer (colour space must match base_image)
 * \param[in] base_image pointer to base image (colour space must match full_image)
 * \returns 0 on success, error number otherwise */
#ifdef VNEmscriptenBuild
	VN_DEC_CORE_API int perseus_decoder_decode_wrapper(perseus_decoder const p, void* pbaseImage,
														void* pfullImage, uint32_t dst_width,
														uint32_t lumaStride, uint32_t interleaved);
#endif

	VN_DEC_CORE_API int perseus_decoder_decode(perseus_decoder const p, const perseus_image* full_image,
												const perseus_image* base_image);

	/*! \brief Sets the live configuration for the decoder. These settings can be modified at
 *			runtime without having to restart the decoder. This should be called after
 *			perseus_decoder_parse()
 *
 *  \param[in] p the decoder
 *  \param[in] cfg a perseus_decoder_live_config struct.
 */
	VN_DEC_CORE_API int perseus_decoder_set_live_config(perseus_decoder const p, perseus_decoder_live_config cfg);

	/*! \brief Populates perseus_buffer_info struct with key allocation information.
 *			This should be called after perseus_decoder_parse()
 *
 *  \param[in] p the decoder
 *  \param[in] plane_idx plane index
 *  \param[out] a pointer to a perseus_buffer_info struct into which the info for the
 *			buffer to a passed in to the DPI should follow.
 */
	VN_DEC_CORE_API void perseus_decoder_get_surface_info(perseus_decoder const p, int plane_idx,
														   perseus_buffer_info* bufferInfo);

	/*! \brief Set pointer to externally generated residual surface (must be zeroed at initialisation).
 *
 *  \param[in] p the decoder
 *  \param[in] plane_idx plane index
 *  \param[in] loq_idx surface loq
 *  \param[in] a pointer to the external buffer */
	VN_DEC_CORE_API void perseus_decoder_set_surface(perseus_decoder const p, int plane_idx,
													  perseus_loq_index loq_idx, void* buffer);

	/*! \brief Enable the generation of interanlly generated residuals surfaces. This enables generation of
 *		   residuals surfaces for multiple LOQs.
 *
 *  \param[in] p the decoder
 *  \param[in] useExternalBuffer whether to use externally allocated buffers or not,
 */
	VN_DEC_CORE_API void perseus_decoder_set_generate_surfaces(perseus_decoder const p, uint8_t enable,
																perseus_surface_format format,
																uint8_t useExternalBuffer);

	/*! \brief Obtain pointer to internally generated residual surface. The pointer returned will either represent
 *		   an s16 surface, or u8 surface depending on the platform the decoder is being run on (HTML5 will return
 *		   u8, otherwise s16). This will return null if generate_surfaces is not enabled.
 *
 *  \param[in] p the decoder
 *  \param[in] plane_idx plane index
 *  \param[in] loq_idx surface loq
 *  \returns pointer to surface on success, null otherwise */
	VN_DEC_CORE_API void* perseus_decoder_get_surface(perseus_decoder const p, int plane_idx,
													   perseus_loq_index loq_idx);

	/*! \brief Copy the upsampling kernel to be used
 * \param[in] upsample_method requested upsampler kernel type
 * \param[out] kernel_out pointer to struct into which the request kernel will be copied
 * \returns 0 on success, error number otherwise. If result is error, kernel_out will not be modified.
 */
	VN_DEC_CORE_API int perseus_decoder_get_upsample_kernel(perseus_decoder const p, perseus_kernel* kernel_out,
															 perseus_upsample upsample_method);

	/*! \brief retrieve last error code
 * \param[out] code the error code
 * \param[out] message the error message */
	VN_DEC_CORE_API void perseus_decoder_get_last_error(perseus_decoder const p, perseus_error_codes* code,
														 const char** const message);

	/*! \brief enable debugging modes
 * \param[in] p the decoder
 * \param[in] mode for debugging i.e. HIGHLIGHT_RESIDUALS */
	VN_DEC_CORE_API void perseus_decoder_debug(perseus_decoder const p, perseus_debug_mode mode);

	/*! \brief Applies the residuals to the input surface
 * \param[in/out] input is the base image (LOQ1) or upscaled image (LOQ1)
 * \param[in] residuals is a perseus_image with only residual data
 * \param[in] plane_idx is plane index
 * \param[in] loq_idx is LOQ index
 * \returns 0 on success, error number otherwise */
	VN_DEC_CORE_API int perseus_decoder_apply_ext_residuals(perseus_decoder const p, const perseus_image* input,
															 perseus_image* residuals, int plane_idx,
															 perseus_loq_index loq_idx);

	/*! \brief Retrieve the command buffer for a given id and LOQ.
	 * 
	 * This function can be called immediately after perseus_decoder_decode_base()
	 * (for LOQ-1) or perseus_decoder_decode_high() (for LOQ-0) to retrieve the
	 * relevant command buffers for that LOQ.
	 * 
	 * \note It is only valid to call this function on a decoder that has been 
	 *       configured with `generate_cmdbuffers`.
	 * 
	 * \note The lifetime of the buffer returned is only guaranteed until the 
	 *       next call either perseus_decoder_decode_base() or perseus_decoder_decode_high().
	 * 
	 * \param[in]     p          the decoder
	 * \param[in]     id         the identifier for which type of commands to retrieve.
	 * \param[in]     loq        the LOQ for the commands to retrieve.
	 * \param[in]     planeIdx   the plane for which to retrieve commands for.
	 * \param[in/out] buffer     the destination to populate with the command data.
	 */
	VN_DEC_CORE_API int perseus_decoder_get_cmd_buffer(perseus_decoder p, perseus_cmdbuffer_id id,
		perseus_loq_index loq, int planeIdx, perseus_cmdbuffer* buffer);

#ifdef __cplusplus
}
#endif
#endif

/* clang-format on */
