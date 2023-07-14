// Copyright (c) V-Nova International Limited 2023. All rights reserved.
//
// Implementation of base::Decoder that uses libavcodec and libavfilter
//
#include "LCEVC/lcevc_dec.h"
#include "LCEVC/utility/base_decoder.h"
#include "LCEVC/utility/extract.h"
#include "LCEVC/utility/picture_layout.h"

extern "C"
{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244)
#endif
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}
//
#include <fmt/core.h>
//
#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lcevc_dec::utility {

static LCEVC_PictureDesc lcevcPictureDesc(AVCodecContext* ctx, AVFilterLink* filterLink);
static LCEVC_CodecType lcevcCodecType(enum AVCodecID avCodecID);
static const char* libavFormatFilter(LCEVC_ColorFormat fmt);

// Base Decoder that uses libav
//
class BaseDecoderLibAV final : public BaseDecoder
{
public:
    BaseDecoderLibAV() = default;
    ~BaseDecoderLibAV() final;

    BaseDecoderLibAV(const BaseDecoderLibAV&) = delete;
    BaseDecoderLibAV(BaseDecoderLibAV&&) = delete;
    BaseDecoderLibAV& operator=(const BaseDecoderLibAV&) = delete;
    BaseDecoderLibAV& operator=(BaseDecoderLibAV&&) = delete;

    const LCEVC_PictureDesc& description() const final { return m_pictureDesc; }
    const PictureLayout& layout() const final { return m_pictureLayout; }

    int maxReorder() const final;

    bool hasImage() const final;
    bool getImage(Data& data) const final;
    void clearImage() final;

    bool hasEnhancement() const final;
    bool getEnhancement(Data& data) const final;
    void clearEnhancement() final;

    bool update() final;

private:
    friend std::unique_ptr<BaseDecoder> createBaseDecoderLibAV(std::string_view source, LCEVC_ColorFormat baseFormat);
    int openInput(std::string_view source);
    int openStream(AVMediaType type);
    int addFilter(std::string_view filters);

    void close();
    AVPixelFormat libavCodecID() const
    {
        assert(m_videoDecCtx);
        return m_videoDecCtx->pix_fmt;
    }

    void copyImage(AVFrame* frame);

    // libav objects
    AVFormatContext* m_fmtCtx = nullptr;     // Container context
    AVCodecContext* m_videoDecCtx = nullptr; // Video stream context

    AVFilterGraph* m_filterGraph = nullptr;     // Optional filter
    AVFilterContext* m_bufferSrcCtx = nullptr;  // Input to filter
    AVFilterContext* m_bufferSinkCtx = nullptr; // Output from filter

    AVPacket* m_packet = nullptr; // In flight packet
    AVFrame* m_frame = nullptr;   // In flight frame

    // Current state of decoder
    enum class State
    {
        Start,
        Running,
        Flushing,
        Eof
    } m_state = State::Start;

    // Format of decoded frames
    AVPixelFormat m_format = AV_PIX_FMT_NONE;

    // Base picture description
    LCEVC_PictureDesc m_pictureDesc = {};

    PictureLayout m_pictureLayout;

    // Holding buffers for output packets - the pointers passed back out
    // are valid until next update
    Data m_imageData = {nullptr};
    Data m_enhancementData = {nullptr};

    std::vector<uint8_t> m_image;
    std::vector<uint8_t> m_enhancement;
};

int BaseDecoderLibAV::openInput(std::string_view source)
{
    if (int r = avformat_open_input(&m_fmtCtx, std::string(source).c_str(), nullptr, nullptr); r < 0) {
        return r;
    }

    return avformat_find_stream_info(m_fmtCtx, nullptr);
}

int BaseDecoderLibAV::openStream(AVMediaType type)
{
    // Find stream index
    const int stream = av_find_best_stream(m_fmtCtx, type, -1, -1, nullptr, 0);
    if (stream < 0) {
        return stream;
    }

    // find codec for the stream
    AVCodecParameters* const codecParameters = m_fmtCtx->streams[stream]->codecpar;
    const AVCodec* const codec = avcodec_find_decoder(codecParameters->codec_id);
    if (!codec) {
        return -1;
    }

    // Create the codec instance
    m_videoDecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_videoDecCtx, codecParameters);
    if (int r = avcodec_open2(m_videoDecCtx, codec, nullptr); r < 0) {
        return r;
    }

#if LIBAVCODEC_VERSION_MAJOR < 59
    m_videoDecCtx->reordered_opaque = 0;
#endif

    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();

    m_state = State::Running;
    return 0;
}

int BaseDecoderLibAV::addFilter(std::string_view filter)
{
    m_filterGraph = avfilter_graph_alloc();

    // buffer video source: the decoded frames from the decoder will be added here.
    const AVFilter* bufferSrc = avfilter_get_by_name("buffer");

    const std::string args =
        fmt::format("{}:{}:{}:{}:{}:{}:{}", m_videoDecCtx->width, m_videoDecCtx->height,
                    m_videoDecCtx->pix_fmt, m_videoDecCtx->time_base.num, m_videoDecCtx->time_base.den,
                    m_videoDecCtx->sample_aspect_ratio.num, m_videoDecCtx->sample_aspect_ratio.den);

    if (int r = avfilter_graph_create_filter(&m_bufferSrcCtx, bufferSrc, "in", args.c_str(),
                                             nullptr, m_filterGraph);
        r < 0) {
        return r;
    }

    // buffer video sink: to terminate the filter chain.
    const AVFilter* bufferSink = avfilter_get_by_name("buffersink");
    enum AVPixelFormat pixFmts[] = {AV_PIX_FMT_GRAY8, AV_PIX_FMT_NONE};
    if (int r = avfilter_graph_create_filter(&m_bufferSinkCtx, bufferSink, "out", nullptr, pixFmts,
                                             m_filterGraph);
        r < 0) {
        return r;
    }

    // Inputs and outputs for the filter
    AVFilterInOut* outputs = avfilter_inout_alloc();
    outputs->name = av_strdup("in");
    outputs->filter_ctx = m_bufferSrcCtx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    AVFilterInOut* inputs = avfilter_inout_alloc();
    inputs->name = av_strdup("out");
    inputs->filter_ctx = m_bufferSinkCtx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    if (int r = avfilter_graph_parse(m_filterGraph, std::string(filter).c_str(), inputs, outputs, nullptr);
        r < 0) {
        return r;
    }

    if (int r = avfilter_graph_config(m_filterGraph, nullptr); r < 0) {
        return r;
    }

    return 0;
}

void BaseDecoderLibAV::close()
{
    m_image.clear();
    m_image.shrink_to_fit();

    m_enhancement.clear();
    m_enhancement.shrink_to_fit();

    if (m_filterGraph) {
        // Also releases m_bufferSinkCtx and m_bufferSinkCtx
        avfilter_graph_free(&m_filterGraph);
    }

    av_frame_free(&m_frame);
    av_packet_free(&m_packet);

    if (m_videoDecCtx) {
        avcodec_close(m_videoDecCtx);
        avcodec_free_context(&m_videoDecCtx);
    }

    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
    }
}

BaseDecoderLibAV::~BaseDecoderLibAV() { close(); }

int BaseDecoderLibAV::maxReorder() const
{
    assert(m_videoDecCtx);
    return m_videoDecCtx->has_b_frames + 1;
}

bool BaseDecoderLibAV::hasImage() const { return m_imageData.ptr && m_imageData.size; }

bool BaseDecoderLibAV::getImage(Data& data) const
{
    if (!hasImage()) {
        return false;
    }

    data = m_imageData;
    return true;
}

void BaseDecoderLibAV::clearImage()
{
    m_imageData = {};
}

bool BaseDecoderLibAV::hasEnhancement() const
{
    return m_enhancementData.ptr && m_enhancementData.size;
}

bool BaseDecoderLibAV::getEnhancement(Data& data) const
{
    if (!hasEnhancement()) {
        return false;
    }

    data = m_enhancementData;
    return true;
}

void BaseDecoderLibAV::clearEnhancement()
{
    m_enhancementData = {};
}

void BaseDecoderLibAV::copyImage(AVFrame* frame)
{
    if (int r = av_image_copy_to_buffer(
            m_image.data(), static_cast<int>(m_image.size()), static_cast<const uint8_t* const *>(frame->data),
            frame->linesize, static_cast<AVPixelFormat>(frame->format), frame->width, frame->height, 1);
        r < 0) {
        fmt::print(stderr, "av_image_copy_to_buffer error: {}\n", r);
        return;
    }

    m_imageData.ptr = m_image.data();
    m_imageData.size = static_cast<uint32_t>(m_image.size());

    if (frame->pts == static_cast<int64_t>(AV_NOPTS_VALUE)) {
#if LIBAVCODEC_VERSION_MAJOR >= 59
        m_imageData.timestamp = frame->pkt_pos;
#else
        m_imageData.timestamp = frame->reordered_opaque;
#endif
    } else {
        m_imageData.timestamp = frame->pts;
    }
}

bool BaseDecoderLibAV::update()
{
    // Loop until something happens ...
    for (;;) {
        // Get AU from stream
        if (m_packet->size == 0 && m_state == State::Running && !hasEnhancement()) {
            if (int r = av_read_frame(m_fmtCtx, m_packet); r < 0) {
                if (r == AVERROR_EOF) {
                    m_state = State::Flushing;
                } else {
                    fmt::print(stderr, "av_read_frame error: %d\n", r);
                    break;
                }
            } else {
                // Got AU - extract enhancement data
                m_enhancement.resize(m_packet->size); // Assume worst case for enhancement
                uint32_t enhancementSize = 0;
                LCEVC_extractEnhancementFromNAL(
                    m_packet->data, m_packet->size, LCEVC_NALFormat_AnnexeB,
                    lcevcCodecType(m_videoDecCtx->codec_id), m_enhancement.data(),
                    static_cast<uint32_t>(m_enhancement.size()), &enhancementSize);

                if (enhancementSize > 0) {
                    // Got some LCEVC data
                    m_enhancementData.ptr = m_enhancement.data();
                    m_enhancementData.size = enhancementSize;

                    if (m_packet->pts == static_cast<int64_t>(AV_NOPTS_VALUE)) {
                        m_enhancementData.timestamp = m_packet->pos;
                    } else {
                        m_enhancementData.timestamp = m_packet->pts;
                    }
                    return true;
                }
            }
        }

        // Put compressed packet to codec
        if (m_packet->size != 0 || m_state == State::Flushing) {
#if LIBAVCODEC_VERSION_MAJOR < 59
            m_videoDecCtx->reordered_opaque++;
#endif
            if (int r = avcodec_send_packet(m_videoDecCtx, m_packet); r < 0) {
                if (r != AVERROR(EAGAIN)) {
                    fmt::print(stderr, "avcodec_send_packet error: %d\n", r);
                    break;
                }
            } else {
                if (m_state == State::Flushing) {
                    m_state = State::Eof;
                }
                av_packet_unref(m_packet);
            }
        }

        // Get frame from codec if there is space, or if filtering
        if (!hasImage() || m_bufferSrcCtx) {
            // Receive the uncompressed base frame from codec
            if (int r = avcodec_receive_frame(m_videoDecCtx, m_frame); r < 0) {
                if (r == AVERROR_EOF) {
                    m_state = State::Eof;
                    break;
                }
                if (r != AVERROR(EAGAIN)) {
                    fmt::print(stderr, "avcodec_receive_frame error: %d\n", r);
                    break;
                }
            } else {
                // Got a picture from base decoder
                if (m_bufferSrcCtx) {
                    // Add frame to the filter
                    if (av_buffersrc_add_frame(m_bufferSrcCtx, m_frame) < 0) {
                        fmt::print(stderr, "av_buffersrc_add_frame error\n");
                        break;
                    }
                } else {
                    copyImage(m_frame);
                    av_frame_unref(m_frame);
                    return true;
                }
            }
        }

        // Get frame from filter, if filtering and have space
        if (m_bufferSinkCtx && !hasImage()) {
            // Get frames from filter
            if (int r = av_buffersink_get_frame(m_bufferSinkCtx, m_frame); r < 0) {
                if (r == AVERROR_EOF) {
                    m_state = State::Eof;
                    break;;
                }

                if (r != AVERROR(EAGAIN)) {
                    fmt::print(stderr, "av_buffersink_get_frame error\n");
                    break;
                }
            } else {
                copyImage(m_frame);
                av_frame_unref(m_frame);
                return true;
            }
        }
    }

    return false;
}

// Convert image format from libav to LCEVC
//
static LCEVC_ColorFormat lcevcColorFormat(AVPixelFormat fmt)
{
    switch (fmt) {
        case AV_PIX_FMT_YUVJ420P:
        case AV_PIX_FMT_YUV420P: return LCEVC_I420_8;
        case AV_PIX_FMT_YUV420P10LE: return LCEVC_I420_10_LE;
#if LIBAVCODEC_VERSION_MAJOR >= 59
        case AV_PIX_FMT_YUV420P12LE: return LCEVC_I420_12_LE;
        case AV_PIX_FMT_YUV420P14LE: return LCEVC_I420_14_LE;
        case AV_PIX_FMT_YUV420P16LE: return LCEVC_I420_16_LE;
#endif
        case AV_PIX_FMT_NV12: return LCEVC_NV12_8;
        case AV_PIX_FMT_NV21: return LCEVC_NV21_8;
        case AV_PIX_FMT_RGB24: return LCEVC_RGB_8;
        case AV_PIX_FMT_BGR24: return LCEVC_BGR_8;
        case AV_PIX_FMT_RGBA: return LCEVC_RGBA_8;
        case AV_PIX_FMT_BGRA: return LCEVC_BGRA_8;
        case AV_PIX_FMT_ARGB: return LCEVC_ARGB_8;
        case AV_PIX_FMT_ABGR: return LCEVC_ABGR_8;
        case AV_PIX_FMT_GRAY8:
            return LCEVC_GRAY_8;
#if LIBAVCODEC_VERSION_MAJOR >= 59
        case AV_PIX_FMT_GRAY10LE: return LCEVC_GRAY_10_LE;
        case AV_PIX_FMT_GRAY12LE: return LCEVC_GRAY_12_LE;
        case AV_PIX_FMT_GRAY14LE: return LCEVC_GRAY_14_LE;
#endif
        case AV_PIX_FMT_GRAY16LE: return LCEVC_GRAY_16_LE;
        default: return LCEVC_ColorFormat_Unknown;
    }
}

static LCEVC_ColorRange lcevcColorRange(AVColorRange range)
{
    switch (range) {
        case AVCOL_RANGE_MPEG: return LCEVC_ColorRange_Limited;
        case AVCOL_RANGE_JPEG: return LCEVC_ColorRange_Full;
        default: return LCEVC_ColorRange_Unknown;
    }
}

static LCEVC_ColorStandard lcevcColorStandard(AVColorSpace space)
{
    switch (space) {
        case AVCOL_SPC_BT709: return LCEVC_ColorStandard_BT709;
        case AVCOL_SPC_BT470BG: return LCEVC_ColorStandard_BT601_PAL;
        case AVCOL_SPC_SMPTE170M:
        case AVCOL_SPC_SMPTE240M: return LCEVC_ColorStandard_BT601_NTSC;
        case AVCOL_SPC_BT2020_NCL:
        case AVCOL_SPC_BT2020_CL: return LCEVC_ColorStandard_BT2020;
        default: return LCEVC_ColorStandard_Unknown;
    }
}

static LCEVC_ColorTransfer lcevcColorTransfer(AVColorTransferCharacteristic transfer)
{
    switch (transfer) {
        case AVCOL_TRC_LINEAR: return LCEVC_ColorTransfer_Linear;
        case AVCOL_TRC_SMPTE170M: return LCEVC_ColorTransfer_SDR;
        case AVCOL_TRC_SMPTE2084: return LCEVC_ColorTransfer_ST2084;
        case AVCOL_TRC_ARIB_STD_B67: return LCEVC_ColorTransfer_HLG;
        default: return LCEVC_ColorTransfer_Unknown;
    }
}

static LCEVC_PictureDesc lcevcPictureDesc(AVCodecContext* ctx, AVFilterLink* filterLink)
{
    LCEVC_PictureDesc desc{};

    if (filterLink) {
        // Get picture format from filter sink
        desc.width = filterLink->w;
        desc.height = filterLink->h;
        desc.colorFormat = lcevcColorFormat(static_cast<AVPixelFormat>(filterLink->format));
        desc.sampleAspectRatioNum = filterLink->sample_aspect_ratio.num;
        desc.sampleAspectRatioDen = filterLink->sample_aspect_ratio.den;
    } else {
        // Get picture format from codec context
        desc.width = ctx->coded_width;
        desc.height = ctx->coded_height;
        desc.colorFormat = lcevcColorFormat(ctx->pix_fmt);
        desc.sampleAspectRatioNum = ctx->sample_aspect_ratio.num;
        desc.sampleAspectRatioDen = ctx->sample_aspect_ratio.den;
    }

    // Use original context for colourspace/range/transfer in lieu of anything better
    desc.colorRange = lcevcColorRange(ctx->color_range);
    desc.colorStandard = lcevcColorStandard(ctx->colorspace);
    desc.colorTransfer = lcevcColorTransfer(ctx->color_trc);
    // desc.hdrStaticInfo = lcevcHDRStaticInfot(ctx->?);

    return desc;
}

static LCEVC_CodecType lcevcCodecType(enum AVCodecID avCodecID)
{
    switch (avCodecID) {
        case AV_CODEC_ID_H264: return LCEVC_CodecType_H264;
        case AV_CODEC_ID_HEVC: return LCEVC_CodecType_H265;
#if LIBAVCODEC_VERSION_MAJOR >= 59
        case AV_CODEC_ID_H266: return LCEVC_CodecType_H266;
#endif
        default: return LCEVC_CodecType_Unknown;
    }
}

// Return a libav filter string that will convert to the given LCEVC color format
// or nullptr if not possible.
//
static const char* libavFormatFilter(LCEVC_ColorFormat fmt)
{
    switch (fmt) {
        case LCEVC_I420_8: return "format=pix_fmts=yuv420p";
        case LCEVC_I420_10_LE: return "format=pix_fmts=yuv420p10le";
        case LCEVC_I420_12_LE: return "format=pix_fmts=yuv420p12le";
        case LCEVC_I420_14_LE: return "format=pix_fmts=yuv420p14le";
        case LCEVC_I420_16_LE: return "format=pix_fmts=yuv420p16le";
        case LCEVC_NV12_8: return "format=pix_fmts=nv12";
        case LCEVC_NV21_8: return "format=pix_fmts=nv21";
        case LCEVC_RGB_8: return "format=pix_fmts=rgb24";
        case LCEVC_BGR_8: return "format=pix_fmts=bgr24";
        case LCEVC_RGBA_8: return "format=pix_fmts=rgba";
        case LCEVC_BGRA_8: return "format=pix_fmts=bgra";
        case LCEVC_ARGB_8: return "format=pix_fmts=argb";
        case LCEVC_ABGR_8: return "format=pix_fmts=abgr";
        case LCEVC_RGBA_10_2_LE: return "format=pix_fmts=x2rgb10le";
        case LCEVC_GRAY_8: return "format=pix_fmts=gray8";
        case LCEVC_GRAY_10_LE: return "format=pix_fmts=gray10le";
        case LCEVC_GRAY_12_LE: return "format=pix_fmts=gray12le";
        case LCEVC_GRAY_14_LE: return "format=pix_fmts=gray14le";
        case LCEVC_GRAY_16_LE: return "format=pix_fmts=gray16le";
        default: return nullptr;
    }
}

static void log_callback(void* avcl, int level, const char* fmt, va_list args)
{
    char tmp[1024];

    if (level <= AV_LOG_FATAL) {
        vsnprintf(tmp, sizeof(tmp) - 1, fmt, args);
        puts(tmp);
    }
}

// Factory function to create a decoder from a given source
//
// The source can be anything supported by libav
// If baseFormat is not Unknown, then the decoded images will be converted to the given format
//
std::unique_ptr<BaseDecoder> createBaseDecoderLibAV(std::string_view source, LCEVC_ColorFormat baseFormat)
{
    auto decoder = std::make_unique<BaseDecoderLibAV>();

    static bool registered = false;

    if (!registered) {
#if LIBAVCODEC_VERSION_MAJOR < 59
        avcodec_register_all();
        av_register_all();
#endif
        av_log_set_callback(log_callback);
        av_log_set_level(AV_LOG_ERROR);

        registered = true;
    }

    // Open container
    if (decoder->openInput(source) < 0) {
        return nullptr;
    }

    // Open video stream
    if (decoder->openStream(AVMEDIA_TYPE_VIDEO) < 0) {
        return nullptr;
    }

    // Optional base format filter
    if (baseFormat != LCEVC_ColorFormat_Unknown) {
        const char* filter = libavFormatFilter(baseFormat);
        if (!filter || decoder->addFilter(filter) < 0) {
            return nullptr;
        }
    }

    // Generate picture description
    if (decoder->m_bufferSinkCtx) {
        decoder->m_pictureDesc =
            lcevcPictureDesc(decoder->m_videoDecCtx, decoder->m_bufferSinkCtx->inputs[0]);
        decoder->m_format = static_cast<AVPixelFormat>(decoder->m_bufferSinkCtx->inputs[0]->format);
    } else {
        decoder->m_pictureDesc = lcevcPictureDesc(decoder->m_videoDecCtx, nullptr);
        decoder->m_format = decoder->m_videoDecCtx->pix_fmt;
    }

    decoder->m_pictureLayout = PictureLayout(decoder->m_pictureDesc);

    // Buffer for output picture
    const int imageBufferSize =
        av_image_get_buffer_size(decoder->m_format, static_cast<int>(decoder->m_pictureDesc.width),
                                 static_cast<int>(decoder->m_pictureDesc.height), 1);
    decoder->m_image.resize(imageBufferSize);

    return decoder;
}

} // namespace lcevc_dec::utility::base
