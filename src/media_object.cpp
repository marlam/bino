/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <frederic.devernay@inrialpes.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

extern "C"
{
#define __STDC_CONSTANT_MACROS
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#include <deque>
#include <limits>
#include <cerrno>
#include <cstring>
#include <cctype>

#if HAVE_SYSCONF
#  include <unistd.h>
#else
#  include <windows.h>
#endif

#include "dbg.h"
#include "blob.h"
#include "exc.h"
#include "msg.h"
#include "str.h"

#include "media_object.h"


// Hide the FFmpeg stuff so that their messy header files cannot cause problems
// in other source files.
struct ffmpeg_stuff
{
    AVFormatContext *format_ctx;

    bool have_active_audio_stream;
    int64_t pos;

    std::vector<int> video_streams;
    std::vector<AVCodecContext *> video_codec_ctxs;
    std::vector<video_frame> video_frame_templates;
    std::vector<struct SwsContext *> video_img_conv_ctxs;
    std::vector<AVCodec *> video_codecs;
    std::vector<std::deque<AVPacket> > video_packet_queues;
    std::vector<AVPacket> video_packets;
    std::vector<bool> video_flush_flags;
    std::vector<AVFrame *> video_frames;
    std::vector<AVFrame *> video_out_frames;
    std::vector<uint8_t *> video_buffers;
    std::vector<int64_t> video_last_timestamps;

    std::vector<int> audio_streams;
    std::vector<AVCodecContext *> audio_codec_ctxs;
    std::vector<audio_blob> audio_blob_templates;
    std::vector<AVCodec *> audio_codecs;
    std::vector<std::deque<AVPacket> > audio_packet_queues;
    std::vector<blob> audio_blobs;
    std::vector<bool> audio_flush_flags;
    std::vector<std::vector<unsigned char> > audio_buffers;
    std::vector<int64_t> audio_last_timestamps;
};

// Use one decoding thread per processor.
static int decoding_threads()
{
    static long n = -1;
    if (n < 0)
    {
#ifdef HAVE_SYSCONF
        n = sysconf(_SC_NPROCESSORS_ONLN);
#else
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        n = si.dwNumberOfProcessors;
#endif
        if (n < 1)
        {
            n = 1;
        }
        else if (n > 16)
        {
            n = 16;
        }
    }
    return n;
}

// Return FFmpeg error as std::string.
static std::string my_av_strerror(int err)
{
    blob b(1024);
    av_strerror(err, b.ptr<char>(), b.size());
    return std::string(b.ptr<const char>());
}

// Convert FFmpeg log messages to our log messages.
static void my_av_log(void *ptr, int level, const char *fmt, va_list vl)
{
    static std::string line;
    if (level > av_log_get_level())
    {
        return;
    }

    std::string p;
    AVClass* avc = ptr ? *reinterpret_cast<AVClass**>(ptr) : NULL;
    if (avc)
    {
        p = str::asprintf("[%s @ %p] ", avc->item_name(ptr), ptr);
    }
    std::string s = str::vasprintf(fmt, vl);
    bool line_ends = false;
    if (s.length() > 0)
    {
        if (s[s.length() - 1] == '\n')
        {
            line_ends = true;
            s.erase(s.length() - 1);
        }
    }
    line += s;
    if (line_ends)
    {
        msg::level_t l;
        switch (level)
        {
        case AV_LOG_PANIC:
        case AV_LOG_FATAL:
        case AV_LOG_ERROR:
            l = msg::ERR;
            break;
        case AV_LOG_WARNING:
            l = msg::WRN;
            break;
        case AV_LOG_INFO:
        case AV_LOG_VERBOSE:
        case AV_LOG_DEBUG:
        default:
            l = msg::DBG;
            break;
        }
        size_t n;
        while ((n = line.find('\n')) != std::string::npos)
        {
            msg::msg(l, std::string("FFmpeg: ") + p + line.substr(0, n));
            line = line.substr(n + 1);
        }
        msg::msg(l, std::string("FFmpeg: ") + p + line);
        line.clear();
    }
}

media_object::media_object() : _ffmpeg(NULL)
{
    av_register_all();
    switch (msg::level())
    {
    case msg::DBG:
        av_log_set_level(AV_LOG_DEBUG);
        break;
    case msg::INF:
        av_log_set_level(AV_LOG_INFO);
        break;
    case msg::WRN:
        av_log_set_level(AV_LOG_WARNING);
        break;
    case msg::ERR:
        av_log_set_level(AV_LOG_ERROR);
        break;
    case msg::REQ:
    default:
        av_log_set_level(AV_LOG_FATAL);
        break;
    }
    av_log_set_callback(my_av_log);
}

media_object::~media_object()
{
    if (_ffmpeg)
    {
        close();
    }
}

void media_object::set_video_frame_template(int index)
{
    AVStream *video_stream = _ffmpeg->format_ctx->streams[_ffmpeg->video_streams[index]];
    AVCodecContext *video_codec_ctx = _ffmpeg->video_codec_ctxs[index];
    video_frame &video_frame_template = _ffmpeg->video_frame_templates[index];

    // Dimensions and aspect ratio
    video_frame_template.raw_width = video_codec_ctx->width;
    video_frame_template.raw_height = video_codec_ctx->height;
    int ar_num = 1;
    int ar_den = 1;
    int ar_snum = video_stream->sample_aspect_ratio.num;
    int ar_sden = video_stream->sample_aspect_ratio.den;
    int ar_cnum = video_codec_ctx->sample_aspect_ratio.num;
    int ar_cden = video_codec_ctx->sample_aspect_ratio.den;
    if (ar_cnum > 0 && ar_cden > 0)
    {
        ar_num = ar_cnum;
        ar_den = ar_cden;
    }
    else if (ar_snum > 0 && ar_sden > 0)
    {
        ar_num = ar_snum;
        ar_den = ar_sden;
    }
    video_frame_template.raw_aspect_ratio =
        static_cast<float>(ar_num * video_frame_template.raw_width)
        / static_cast<float>(ar_den * video_frame_template.raw_height);
    // Data layout and color space
    video_frame_template.layout = video_frame::bgra32;
    video_frame_template.color_space = video_frame::srgb;
    video_frame_template.value_range = video_frame::u8_full;
    video_frame_template.chroma_location = video_frame::center;
    if (video_codec_ctx->pix_fmt == PIX_FMT_YUV444P
            || video_codec_ctx->pix_fmt == PIX_FMT_YUV422P
            || video_codec_ctx->pix_fmt == PIX_FMT_YUV420P)
    {
        if (video_codec_ctx->pix_fmt == PIX_FMT_YUV444P)
        {
            video_frame_template.layout = video_frame::yuv444p;
        }
        else if (video_codec_ctx->pix_fmt == PIX_FMT_YUV422P)
        {
            video_frame_template.layout = video_frame::yuv422p;
        }
        else
        {
            video_frame_template.layout = video_frame::yuv420p;
        }
        video_frame_template.color_space = video_frame::yuv601;
        if (video_codec_ctx->colorspace == AVCOL_SPC_BT709)
        {
            video_frame_template.color_space = video_frame::yuv709;
        }
        video_frame_template.value_range = video_frame::u8_mpeg;
        if (video_codec_ctx->color_range == AVCOL_RANGE_JPEG)
        {
            video_frame_template.value_range = video_frame::u8_full;
        }
        video_frame_template.chroma_location = video_frame::center;
        if (video_codec_ctx->chroma_sample_location == AVCHROMA_LOC_LEFT)
        {
            video_frame_template.chroma_location = video_frame::left;
        }
        else if (video_codec_ctx->chroma_sample_location == AVCHROMA_LOC_TOPLEFT)
        {
            video_frame_template.chroma_location = video_frame::topleft;
        }
    }
    else if (video_codec_ctx->pix_fmt == PIX_FMT_YUVJ444P
            || video_codec_ctx->pix_fmt == PIX_FMT_YUVJ422P
            || video_codec_ctx->pix_fmt == PIX_FMT_YUVJ420P)
    {
        if (video_codec_ctx->pix_fmt == PIX_FMT_YUVJ444P)
        {
            video_frame_template.layout = video_frame::yuv444p;
        }
        else if (video_codec_ctx->pix_fmt == PIX_FMT_YUVJ422P)
        {
            video_frame_template.layout = video_frame::yuv422p;
        }
        else
        {
            video_frame_template.layout = video_frame::yuv420p;
        }
        video_frame_template.color_space = video_frame::yuv601;
        video_frame_template.value_range = video_frame::u8_full;
        video_frame_template.chroma_location = video_frame::center;
    }
    // Stereo layout
    video_frame_template.stereo_layout = video_frame::mono;
    video_frame_template.stereo_layout_swap = false;
    std::string val;
    /* Determine the stereo layout from the resolution.*/
    if (video_frame_template.raw_width / 2 > video_frame_template.raw_height)
    {
        video_frame_template.stereo_layout = video_frame::left_right;
    }
    else if (video_frame_template.raw_height > video_frame_template.raw_width)
    {
        video_frame_template.stereo_layout = video_frame::top_bottom;
    }
    /* Determine the input mode by looking at the file name.
     * This should be compatible to these conventions:
     * http://www.tru3d.com/technology/3D_Media_Formats_Software.php?file=TriDef%20Supported%203D%20Formats */
    std::string marker = _url.substr(0, _url.find_last_of('.'));
    size_t last_dash = marker.find_last_of('-');
    if (last_dash != std::string::npos)
    {
        marker = marker.substr(last_dash + 1);
    }
    else
    {
        marker = "";
    }
    for (size_t i = 0; i < marker.length(); i++)
    {
        marker[i] = std::tolower(marker[i]);
    }
    if (marker == "lr")
    {
        video_frame_template.stereo_layout = video_frame::left_right;
        video_frame_template.stereo_layout_swap = false;
    }
    else if (marker == "rl")
    {
        video_frame_template.stereo_layout = video_frame::left_right;
        video_frame_template.stereo_layout_swap = true;
    }
    else if (marker == "lrh" || marker == "lrq")
    {
        video_frame_template.stereo_layout = video_frame::left_right_half;
        video_frame_template.stereo_layout_swap = false;
    }
    else if (marker == "rlh" || marker == "rlq")
    {
        video_frame_template.stereo_layout = video_frame::left_right_half;
        video_frame_template.stereo_layout_swap = true;
    }
    else if (marker == "tb" || marker == "ab")
    {
        video_frame_template.stereo_layout = video_frame::top_bottom;
        video_frame_template.stereo_layout_swap = false;
    }
    else if (marker == "bt" || marker == "ba")
    {
        video_frame_template.stereo_layout = video_frame::top_bottom;
        video_frame_template.stereo_layout_swap = true;
    }
    else if (marker == "tbh" || marker == "abq")
    {
        video_frame_template.stereo_layout = video_frame::top_bottom_half;
        video_frame_template.stereo_layout_swap = false;
    }
    else if (marker == "bth" || marker == "baq")
    {
        video_frame_template.stereo_layout = video_frame::top_bottom_half;
        video_frame_template.stereo_layout_swap = true;
    }
    else if (marker == "eo")
    {
        video_frame_template.stereo_layout = video_frame::even_odd_rows;
        video_frame_template.stereo_layout_swap = false;
        // all image lines are given in this case, and there should be no interpolation [TODO]
    }
    else if (marker == "oe")
    {
        video_frame_template.stereo_layout = video_frame::even_odd_rows;
        video_frame_template.stereo_layout_swap = true;
        // all image lines are given in this case, and there should be no interpolation [TODO]
    }
    else if (marker == "eoq" || marker == "3dir")
    {
        video_frame_template.stereo_layout = video_frame::even_odd_rows;
        video_frame_template.stereo_layout_swap = false;
    }
    else if (marker == "oeq" || marker == "3di")
    {
        video_frame_template.stereo_layout = video_frame::even_odd_rows;
        video_frame_template.stereo_layout_swap = true;
    }
    else if (marker == "2d")
    {
        video_frame_template.stereo_layout = video_frame::mono;
        video_frame_template.stereo_layout_swap = false;
    }
    /* Check some tags defined at this link: http://www.3dtv.at/Knowhow/StereoWmvSpec_en.aspx
     * This is necessary to make the example movies provided by 3dtv.at work out of the box. */
    val = tag_value("StereoscopicLayout");
    if (val == "SideBySideRF" || val == "SideBySideLF")
    {
        video_frame_template.stereo_layout_swap = (val == "SideBySideRF");
        val = tag_value("StereoscopicHalfWidth");
        video_frame_template.stereo_layout = (val == "1" ? video_frame::left_right_half : video_frame::left_right);

    }
    else if (val == "OverUnderRT" || val == "OverUnderLT")
    {
        video_frame_template.stereo_layout_swap = (val == "OverUnderRT");
        val = tag_value("StereoscopicHalfHeight");
        video_frame_template.stereo_layout = (val == "1" ? video_frame::top_bottom_half : video_frame::top_bottom);
    }
    /* Sanity checks. If these fail, use safe fallback */
    if (((video_frame_template.stereo_layout == video_frame::left_right
                    || video_frame_template.stereo_layout == video_frame::left_right_half)
                && video_frame_template.raw_width % 2 != 0)
            || ((video_frame_template.stereo_layout == video_frame::top_bottom
                    || video_frame_template.stereo_layout == video_frame::top_bottom_half)
                && video_frame_template.raw_height % 2 != 0)
            || (video_frame_template.stereo_layout == video_frame::even_odd_rows
                && video_frame_template.raw_height % 2 != 0))
    {
        video_frame_template.stereo_layout = video_frame::mono;
        video_frame_template.stereo_layout_swap = false;
    }
    /* Set width and height of a single view */
    video_frame_template.set_view_dimensions();
}

void media_object::set_audio_blob_template(int index)
{
    AVStream *audio_stream = _ffmpeg->format_ctx->streams[_ffmpeg->audio_streams[index]];
    AVCodecContext *audio_codec_ctx = _ffmpeg->audio_codec_ctxs[index];
    audio_blob &audio_blob_template = _ffmpeg->audio_blob_templates[index];

    if (audio_codec_ctx->channels < 1
            || audio_codec_ctx->channels > 8
            || audio_codec_ctx->channels == 3
            || audio_codec_ctx->channels == 5)
    {
        throw exc(_url + " audio stream " + str::from(audio_stream)
                + ": Cannot handle audio with "
                + str::from(audio_codec_ctx->channels) + " channels");
    }
    audio_blob_template.channels = audio_codec_ctx->channels;
    audio_blob_template.rate = audio_codec_ctx->sample_rate;
    if (audio_codec_ctx->sample_fmt == SAMPLE_FMT_U8)
    {
        audio_blob_template.sample_format = audio_blob::u8;
    }
    else if (audio_codec_ctx->sample_fmt == SAMPLE_FMT_S16)
    {
        audio_blob_template.sample_format = audio_blob::s16;
    }
    else if (audio_codec_ctx->sample_fmt == SAMPLE_FMT_FLT)
    {
        audio_blob_template.sample_format = audio_blob::f32;
    }
    else if (audio_codec_ctx->sample_fmt == SAMPLE_FMT_DBL)
    {
        audio_blob_template.sample_format = audio_blob::d64;
    }
    else
    {
        throw exc(_url + " audio stream " + str::from(audio_stream)
                + ": Cannot handle audio with sample format "
                + str::from(static_cast<int>(audio_codec_ctx->sample_fmt)));
        // XXX: Once widely available, use av_get_sample_fmt_name(audio_codec_ctx->sample_fmt)
        // in this error message.
    }
}

void media_object::open(const std::string &url)
{
    _url = url;
    _ffmpeg = new struct ffmpeg_stuff;
    int e;

    if ((e = av_open_input_file(&_ffmpeg->format_ctx, _url.c_str(), NULL, 0, NULL)) != 0)
    {
        throw exc(_url + ": " + my_av_strerror(e));
    }
    if ((e = av_find_stream_info(_ffmpeg->format_ctx)) < 0)
    {
        throw exc(_url + ": Cannot read stream info: " + my_av_strerror(e));
    }
    dump_format(_ffmpeg->format_ctx, 0, _url.c_str(), 0);

    /* Metadata */
    AVMetadataTag *tag = NULL;
    while ((tag = av_metadata_get(_ffmpeg->format_ctx->metadata, "", tag, AV_METADATA_IGNORE_SUFFIX)))
    {
        _tag_names.push_back(tag->key);
        _tag_values.push_back(tag->value);
    }

    _ffmpeg->have_active_audio_stream = false;
    _ffmpeg->pos = std::numeric_limits<int64_t>::min();

    for (unsigned int i = 0; i < _ffmpeg->format_ctx->nb_streams
            && i < static_cast<unsigned int>(std::numeric_limits<int>::max()); i++)
    {
        _ffmpeg->format_ctx->streams[i]->discard = AVDISCARD_ALL;        // ignore by default; user must activate streams
        if (_ffmpeg->format_ctx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
        {
            _ffmpeg->video_streams.push_back(i);
            int j = _ffmpeg->video_streams.size() - 1;
            msg::dbg(_url + " stream " + str::from(i) + " is video stream " + str::from(j));
            _ffmpeg->video_codec_ctxs.push_back(_ffmpeg->format_ctx->streams[i]->codec);
            if (_ffmpeg->video_codec_ctxs[j]->width < 1 || _ffmpeg->video_codec_ctxs[j]->height < 1)
            {
                throw exc(_url + " stream " + str::from(i) + ": Invalid frame size");
            }
            _ffmpeg->video_codec_ctxs[j]->thread_count = decoding_threads();
            if (avcodec_thread_init(_ffmpeg->video_codec_ctxs[j], _ffmpeg->video_codec_ctxs[j]->thread_count) != 0)
            {
                _ffmpeg->video_codec_ctxs[j]->thread_count = 1;
            }
            _ffmpeg->video_codecs.push_back(avcodec_find_decoder(_ffmpeg->video_codec_ctxs[j]->codec_id));
            if (!_ffmpeg->video_codecs[j])
            {
                throw exc(_url + " stream " + str::from(i) + ": Unsupported video codec");
            }
            if ((e = avcodec_open(_ffmpeg->video_codec_ctxs[j], _ffmpeg->video_codecs[j])) < 0)
            {
                _ffmpeg->video_codecs[j] = NULL;
                throw exc(_url + " stream " + str::from(i) + ": Cannot open video codec: " + my_av_strerror(e));
            }
            // Determine frame template.
            _ffmpeg->video_frame_templates.push_back(video_frame());
            set_video_frame_template(j);
            // Allocate packets and frames
            _ffmpeg->video_packets.push_back(AVPacket());
            _ffmpeg->video_flush_flags.push_back(false);
            _ffmpeg->video_frames.push_back(avcodec_alloc_frame());
            if (!_ffmpeg->video_frames[j])
            {
                throw exc(HERE + ": " + strerror(ENOMEM));
            }
            if (_ffmpeg->video_frame_templates[j].layout == video_frame::bgra32)
            {
                // Initialize things needed for software pixel format conversion
                int bufsize = avpicture_get_size(PIX_FMT_BGRA,
                        _ffmpeg->video_codec_ctxs[j]->width, _ffmpeg->video_codec_ctxs[j]->height);
                _ffmpeg->video_out_frames.push_back(avcodec_alloc_frame());
                _ffmpeg->video_buffers.push_back(static_cast<uint8_t *>(av_malloc(bufsize)));
                if (!_ffmpeg->video_out_frames[j] || !_ffmpeg->video_buffers[j])
                {
                    throw exc(HERE + ": " + strerror(ENOMEM));
                }
                avpicture_fill(reinterpret_cast<AVPicture *>(_ffmpeg->video_out_frames[j]), _ffmpeg->video_buffers[j],
                        PIX_FMT_BGRA, _ffmpeg->video_codec_ctxs[j]->width, _ffmpeg->video_codec_ctxs[j]->height);
                _ffmpeg->video_img_conv_ctxs.push_back(sws_getContext(
                            _ffmpeg->video_codec_ctxs[j]->width, _ffmpeg->video_codec_ctxs[j]->height, _ffmpeg->video_codec_ctxs[j]->pix_fmt,
                            _ffmpeg->video_codec_ctxs[j]->width, _ffmpeg->video_codec_ctxs[j]->height, PIX_FMT_BGRA,
                            SWS_POINT, NULL, NULL, NULL));
                if (!_ffmpeg->video_img_conv_ctxs[j])
                {
                    throw exc(_url + " stream " + str::from(i) + ": Cannot initialize conversion context");
                }
            }
            else
            {
                _ffmpeg->video_out_frames.push_back(NULL);
                _ffmpeg->video_buffers.push_back(NULL);
                _ffmpeg->video_img_conv_ctxs.push_back(NULL);
            }
            _ffmpeg->video_last_timestamps.push_back(std::numeric_limits<int64_t>::min());
        }
        else if (_ffmpeg->format_ctx->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO)
        {
            _ffmpeg->audio_streams.push_back(i);
            int j = _ffmpeg->audio_streams.size() - 1;
            msg::dbg(_url + " stream " + str::from(i) + " is audio stream " + str::from(j));
            _ffmpeg->audio_codec_ctxs.push_back(_ffmpeg->format_ctx->streams[i]->codec);
            _ffmpeg->audio_codecs.push_back(avcodec_find_decoder(_ffmpeg->audio_codec_ctxs[j]->codec_id));
            if (!_ffmpeg->audio_codecs[j])
            {
                throw exc(_url + " stream " + str::from(i) + ": Unsupported audio codec");
            }
            if ((e = avcodec_open(_ffmpeg->audio_codec_ctxs[j], _ffmpeg->audio_codecs[j])) < 0)
            {
                _ffmpeg->audio_codecs[j] = NULL;
                throw exc(_url + " stream " + str::from(i) + ": Cannot open audio codec: " + my_av_strerror(e));
            }
            _ffmpeg->audio_blob_templates.push_back(audio_blob());
            set_audio_blob_template(j);
            _ffmpeg->audio_blobs.push_back(blob());
            _ffmpeg->audio_flush_flags.push_back(false);
            _ffmpeg->audio_buffers.push_back(std::vector<unsigned char>());
            _ffmpeg->audio_last_timestamps.push_back(std::numeric_limits<int64_t>::min());
        }
        else
        {
            msg::dbg(_url + " stream " + str::from(i) + " contains neither video nor audio");
        }
    }
    _ffmpeg->video_packet_queues.resize(video_streams());
    _ffmpeg->audio_packet_queues.resize(audio_streams());

    msg::inf(_url + ":");
    for (int i = 0; i < video_streams(); i++)
    {
        msg::inf("    Video stream %d: %s / %s, %g seconds", i,
                video_frame_template(i).format_info().c_str(),
                video_frame_template(i).format_name().c_str(),
                video_duration(i) / 1e6f);
        msg::inf("        Using up to %d threads for decoding",
                _ffmpeg->video_codec_ctxs.at(i)->thread_count);
    }
    for (int i = 0; i < audio_streams(); i++)
    {
        msg::inf("    Audio stream %d: %s / %s, %g seconds", i,
                audio_blob_template(i).format_info().c_str(),
                audio_blob_template(i).format_name().c_str(),
                audio_duration(i) / 1e6f);
    }
    if (video_streams() == 0 && audio_streams() == 0)
    {
        msg::inf("    No usable streams");
    }
}

const std::string &media_object::url() const
{
    return _url;
}

size_t media_object::tags() const
{
    return _tag_names.size();
}

const std::string &media_object::tag_name(size_t i) const
{
    assert(i < tags());
    return _tag_names[i];
}

const std::string &media_object::tag_value(size_t i) const
{
    assert(i < tags());
    return _tag_values[i];
}

const std::string &media_object::tag_value(const std::string &tag_name) const
{
    static std::string empty;
    for (size_t i = 0; i < _tag_names.size(); i++)
    {
        if (std::string(tag_name) == _tag_names[i])
        {
            return _tag_values[i];
        }
    }
    return empty;
}

int media_object::audio_streams() const
{
    return _ffmpeg->audio_streams.size();
}

int media_object::video_streams() const
{
    return _ffmpeg->video_streams.size();
}

void media_object::video_stream_set_active(int index, bool active)
{
    assert(index >= 0);
    assert(index < video_streams());
    _ffmpeg->format_ctx->streams[_ffmpeg->video_streams.at(index)]->discard =
        (active ? AVDISCARD_DEFAULT : AVDISCARD_ALL);
}

void media_object::audio_stream_set_active(int index, bool active)
{
    assert(index >= 0);
    assert(index < audio_streams());
    _ffmpeg->format_ctx->streams[_ffmpeg->audio_streams.at(index)]->discard =
        (active ? AVDISCARD_DEFAULT : AVDISCARD_ALL);
    _ffmpeg->have_active_audio_stream = false;
    for (int i = 0; i < audio_streams(); i++)
    {
        if (_ffmpeg->format_ctx->streams[_ffmpeg->audio_streams.at(index)]->discard == AVDISCARD_DEFAULT)
        {
            _ffmpeg->have_active_audio_stream = true;
            break;
        }
    }
}

const video_frame &media_object::video_frame_template(int video_stream) const
{
    assert(video_stream >= 0);
    assert(video_stream < video_streams());
    return _ffmpeg->video_frame_templates.at(video_stream);
}

int media_object::video_frame_rate_numerator(int index) const
{
    assert(index >= 0);
    assert(index < video_streams());
    return _ffmpeg->format_ctx->streams[_ffmpeg->video_streams.at(index)]->r_frame_rate.num;
}

int media_object::video_frame_rate_denominator(int index) const
{
    assert(index >= 0);
    assert(index < video_streams());
    return _ffmpeg->format_ctx->streams[_ffmpeg->video_streams.at(index)]->r_frame_rate.den;
}

int64_t media_object::video_duration(int index) const
{
    assert(index >= 0);
    assert(index < video_streams());
    // Try to get duration from the Codec first. If that fails, fall back to
    // the value provided by the container.
    int64_t duration = _ffmpeg->format_ctx->streams[_ffmpeg->video_streams.at(index)]->duration;
    if (duration > 0)
    {
        AVRational time_base = _ffmpeg->format_ctx->streams[_ffmpeg->video_streams.at(index)]->time_base;
        return duration * 1000000 * time_base.num / time_base.den;
    }
    else
    {
        duration = _ffmpeg->format_ctx->duration;
        return duration * 1000000 / AV_TIME_BASE;
    }
}

const audio_blob &media_object::audio_blob_template(int audio_stream) const
{
    assert(audio_stream >= 0);
    assert(audio_stream < audio_streams());
    return _ffmpeg->audio_blob_templates.at(audio_stream);
}

int64_t media_object::audio_duration(int index) const
{
    assert(index >= 0);
    assert(index < audio_streams());
    // Try to get duration from the Codec first. If that fails, fall back to
    // the value provided by the container.
    int64_t duration = _ffmpeg->format_ctx->streams[_ffmpeg->audio_streams.at(index)]->duration;
    if (duration > 0)
    {
        AVRational time_base = _ffmpeg->format_ctx->streams[_ffmpeg->audio_streams.at(index)]->time_base;
        return duration * 1000000 * time_base.num / time_base.den;
    }
    else
    {
        duration = _ffmpeg->format_ctx->duration;
        return duration * 1000000 / AV_TIME_BASE;
    }
}

bool media_object::read()
{
    msg::dbg(_url + ": Reading a packet");

    AVPacket packet;
    int e;
    if ((e = av_read_frame(_ffmpeg->format_ctx, &packet)) < 0)
    {
        if (e == AVERROR_EOF)
        {
            msg::dbg(_url + ": EOF");
            return false;
        }
        else
        {
            throw exc(_url + ": " + my_av_strerror(e));
        }
    }
    bool packet_queued = false;
    for (size_t i = 0; i < _ffmpeg->video_streams.size() && !packet_queued; i++)
    {
        if (packet.stream_index == _ffmpeg->video_streams[i])
        {
            if (av_dup_packet(&packet) < 0)
            {
                msg::dbg(_url + ": Cannot duplicate packet");
                return false;
            }
            _ffmpeg->video_packet_queues[i].push_back(packet);
            msg::dbg(_url + ": "
                    + str::from(_ffmpeg->video_packet_queues[i].size())
                    + " packets queued in video stream " + str::from(i));
            packet_queued = true;
        }
    }
    for (size_t i = 0; i < _ffmpeg->audio_streams.size() && !packet_queued; i++)
    {
        if (packet.stream_index == _ffmpeg->audio_streams[i])
        {
            if (av_dup_packet(&packet) < 0)
            {
                msg::dbg(_url + ": Cannot duplicate packet");
                return false;
            }
            _ffmpeg->audio_packet_queues[i].push_back(packet);
            msg::dbg(_url + ": "
                    + str::from(_ffmpeg->audio_packet_queues[i].size())
                    + " packets queued in audio stream " + str::from(i));
            packet_queued = true;
        }
    }
    if (!packet_queued)
    {
        av_free_packet(&packet);
    }

    return true;
}

int64_t media_object::handle_timestamp(int64_t &last_timestamp, int64_t timestamp)
{
    if (timestamp == std::numeric_limits<int64_t>::min())
    {
        timestamp = last_timestamp;
    }
    last_timestamp = timestamp;
    return timestamp;
}

int64_t media_object::handle_video_timestamp(int video_stream, int64_t timestamp)
{
    int64_t ts = handle_timestamp(_ffmpeg->video_last_timestamps[video_stream], timestamp);
    if (!_ffmpeg->have_active_audio_stream || _ffmpeg->pos == std::numeric_limits<int64_t>::min())
    {
        _ffmpeg->pos = ts;
    }
    return ts;
}

int64_t media_object::handle_audio_timestamp(int audio_stream, int64_t timestamp)
{
    int64_t ts = handle_timestamp(_ffmpeg->audio_last_timestamps[audio_stream], timestamp);
    _ffmpeg->pos = ts;
    return ts;
}

void media_object::start_video_frame_read(int video_stream)
{
    assert(video_stream >= 0);
    assert(video_stream < video_streams());
    if (_ffmpeg->video_flush_flags[video_stream])
    {
        avcodec_flush_buffers(_ffmpeg->format_ctx->streams[_ffmpeg->video_streams[video_stream]]->codec);
        for (size_t j = 0; j < _ffmpeg->video_packet_queues[video_stream].size(); j++)
        {
            av_free_packet(&_ffmpeg->video_packet_queues[video_stream][j]);
        }
        _ffmpeg->video_packet_queues[video_stream].clear();
        _ffmpeg->video_flush_flags[video_stream] = false;
    }

    // TODO: start thread that reads a frame
}

video_frame media_object::finish_video_frame_read(int video_stream)
{
    assert(video_stream >= 0);
    assert(video_stream < video_streams());
    // TODO: wait for thread, use its results

    int frame_finished = 0;
    do
    {
        while (_ffmpeg->video_packet_queues[video_stream].empty())
        {
            if (!read())
            {
                return video_frame();
            }
        }
        _ffmpeg->video_packets[video_stream] = _ffmpeg->video_packet_queues[video_stream].front();
        _ffmpeg->video_packet_queues[video_stream].pop_front();
        avcodec_decode_video2(_ffmpeg->video_codec_ctxs[video_stream],
                _ffmpeg->video_frames[video_stream], &frame_finished,
                &(_ffmpeg->video_packets[video_stream]));
        av_free_packet(&(_ffmpeg->video_packets[video_stream]));
    }
    while (!frame_finished);

    video_frame frame = _ffmpeg->video_frame_templates[video_stream];
    if (frame.layout == video_frame::bgra32)
    {
        sws_scale(_ffmpeg->video_img_conv_ctxs[video_stream],
                _ffmpeg->video_frames[video_stream]->data,
                _ffmpeg->video_frames[video_stream]->linesize,
                0, frame.raw_height,
                _ffmpeg->video_out_frames[video_stream]->data,
                _ffmpeg->video_out_frames[video_stream]->linesize);
        // TODO: Handle sws_scale errors. How?
        frame.data[0][0] = _ffmpeg->video_out_frames[video_stream]->data[0];
        frame.line_size[0][0] = _ffmpeg->video_out_frames[video_stream]->linesize[0];
    }
    else
    {
        frame.data[0][0] = _ffmpeg->video_frames[video_stream]->data[0];
        frame.data[0][1] = _ffmpeg->video_frames[video_stream]->data[1];
        frame.data[0][2] = _ffmpeg->video_frames[video_stream]->data[2];
        frame.line_size[0][0] = _ffmpeg->video_frames[video_stream]->linesize[0];
        frame.line_size[0][1] = _ffmpeg->video_frames[video_stream]->linesize[1];
        frame.line_size[0][2] = _ffmpeg->video_frames[video_stream]->linesize[2];
    }

    frame.presentation_time = handle_video_timestamp(video_stream,
            _ffmpeg->video_packets[video_stream].dts * 1000000
            * _ffmpeg->format_ctx->streams[_ffmpeg->video_streams[video_stream]]->time_base.num 
            / _ffmpeg->format_ctx->streams[_ffmpeg->video_streams[video_stream]]->time_base.den);

    return frame;
}

void media_object::start_audio_blob_read(int audio_stream, size_t size)
{
    assert(audio_stream >= 0);
    assert(audio_stream < audio_streams());
    _ffmpeg->audio_blobs[audio_stream].resize(size);
    if (_ffmpeg->audio_flush_flags[audio_stream])
    {
        avcodec_flush_buffers(_ffmpeg->format_ctx->streams[_ffmpeg->audio_streams[audio_stream]]->codec);
        _ffmpeg->audio_buffers[audio_stream].clear();
        for (size_t j = 0; j < _ffmpeg->audio_packet_queues[audio_stream].size(); j++)
        {
            av_free_packet(&_ffmpeg->audio_packet_queues[audio_stream][j]);
        }
        _ffmpeg->audio_packet_queues[audio_stream].clear();
        _ffmpeg->audio_flush_flags[audio_stream] = false;
    }

    // TODO: start thread that reads a blob
}

audio_blob media_object::finish_audio_blob_read(int audio_stream)
{
    assert(audio_stream >= 0);
    assert(audio_stream < audio_streams());

    // TODO: wait for thread, use its results

    size_t size = _ffmpeg->audio_blobs[audio_stream].size();
    void *buffer = _ffmpeg->audio_blobs[audio_stream].ptr();
    int64_t timestamp = std::numeric_limits<int64_t>::min();
    size_t i = 0;
    while (i < size)
    {
        if (_ffmpeg->audio_buffers[audio_stream].size() > 0)
        {
            // Use available decoded audio data
            size_t remaining = std::min(size - i, _ffmpeg->audio_buffers[audio_stream].size());
            memcpy(buffer, &(_ffmpeg->audio_buffers[audio_stream][0]), remaining);
            _ffmpeg->audio_buffers[audio_stream].erase(
                    _ffmpeg->audio_buffers[audio_stream].begin(),
                    _ffmpeg->audio_buffers[audio_stream].begin() + remaining);
            buffer = reinterpret_cast<unsigned char *>(buffer) + remaining;
            i += remaining;
        }
        if (_ffmpeg->audio_buffers[audio_stream].size() == 0)
        {
            // Read more audio data
            AVPacket packet, tmppacket;
            while (_ffmpeg->audio_packet_queues[audio_stream].empty())
            {
                if (!read())
                {
                    return audio_blob();
                }
            }
            packet = _ffmpeg->audio_packet_queues[audio_stream].front();
            _ffmpeg->audio_packet_queues[audio_stream].pop_front();
            if (timestamp == std::numeric_limits<int64_t>::min())
            {
                timestamp = packet.dts * 1000000
                    * _ffmpeg->format_ctx->streams[_ffmpeg->audio_streams[audio_stream]]->time_base.num 
                    / _ffmpeg->format_ctx->streams[_ffmpeg->audio_streams[audio_stream]]->time_base.den;
            }

            // Decode audio data
            tmppacket = packet;
            while (tmppacket.size > 0)
            {
                // Manage tmpbuf with av_malloc/av_free, to guarantee correct alignment.
                // Not doing this results in hard to debug crashes on some systems.
                int tmpbuf_size = (AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2;
                unsigned char *tmpbuf = static_cast<unsigned char *>(av_malloc(tmpbuf_size));
                int len = avcodec_decode_audio3(_ffmpeg->audio_codec_ctxs[audio_stream],
                        reinterpret_cast<int16_t *>(&(tmpbuf[0])), &tmpbuf_size, &tmppacket);
                if (len < 0)
                {
                    tmppacket.size = 0;
                    break;
                }
                tmppacket.data += len;
                tmppacket.size -= len;
                if (tmpbuf_size <= 0)
                {
                    continue;
                }
                // Put it in the decoded audio data buffer
                size_t old_size = _ffmpeg->audio_buffers[audio_stream].size();
                _ffmpeg->audio_buffers[audio_stream].resize(old_size + tmpbuf_size);
                memcpy(&(_ffmpeg->audio_buffers[audio_stream][old_size]), tmpbuf, tmpbuf_size);
                av_free(tmpbuf);
            }
            
            av_free_packet(&packet);
        }
    }

    audio_blob blob = _ffmpeg->audio_blob_templates[audio_stream];
    blob.data = _ffmpeg->audio_blobs[audio_stream].ptr();
    blob.size = _ffmpeg->audio_blobs[audio_stream].size();
    blob.presentation_time = handle_audio_timestamp(audio_stream, timestamp);
    return blob;
}

int64_t media_object::tell()
{
    return _ffmpeg->pos;
}

void media_object::seek(int64_t dest_pos)
{
    msg::dbg(_url + ": Seeking from " + str::from(_ffmpeg->pos / 1e6f) + " to " + str::from(dest_pos / 1e6f));

    int e = av_seek_frame(_ffmpeg->format_ctx, -1,
            dest_pos * AV_TIME_BASE / 1000000,
            dest_pos < _ffmpeg->pos ?  AVSEEK_FLAG_BACKWARD : 0);
    if (e < 0)
    {
        msg::err(_url + ": Seeking failed");
    }
    else
    {
        /* Set a flag to throw away all queued packets */
        for (size_t i = 0; i < _ffmpeg->video_packet_queues.size(); i++)
        {
            _ffmpeg->video_flush_flags[i] = true;
        }
        for (size_t i = 0; i < _ffmpeg->audio_packet_queues.size(); i++)
        {
            _ffmpeg->audio_flush_flags[i] = true;
        }
        /* The next read request must update the position */
        _ffmpeg->pos = std::numeric_limits<int64_t>::min();
    }
}

void media_object::close()
{
    for (size_t i = 0; i < _ffmpeg->video_frames.size(); i++)
    {
        av_free(_ffmpeg->video_frames[i]);
    }
    for (size_t i = 0; i < _ffmpeg->video_out_frames.size(); i++)
    {
        av_free(_ffmpeg->video_out_frames[i]);
    }
    for (size_t i = 0; i < _ffmpeg->video_buffers.size(); i++)
    {
        av_free(_ffmpeg->video_buffers[i]);
    }
    for (size_t i = 0; i < _ffmpeg->video_codec_ctxs.size(); i++)
    {
        if (i < _ffmpeg->video_codecs.size() && _ffmpeg->video_codecs[i])
        {
            avcodec_close(_ffmpeg->video_codec_ctxs[i]);
        }
    }
    for (size_t i = 0; i < _ffmpeg->video_img_conv_ctxs.size(); i++)
    {
        sws_freeContext(_ffmpeg->video_img_conv_ctxs[i]);
    }
    for (size_t i = 0; i < _ffmpeg->video_packet_queues.size(); i++)
    {
        if (_ffmpeg->video_packet_queues[i].size() > 0)
        {
            msg::dbg(_url + ": " + str::from(_ffmpeg->video_packet_queues[i].size())
                    + " unprocessed video packets in video stream " + str::from(i));
        }
        for (size_t j = 0; j < _ffmpeg->video_packet_queues[i].size(); j++)
        {
            av_free_packet(&_ffmpeg->video_packet_queues[i][j]);
        }
    }
    for (size_t i = 0; i < _ffmpeg->audio_codec_ctxs.size(); i++)
    {
        if (i < _ffmpeg->audio_codecs.size() && _ffmpeg->audio_codecs[i])
        {
            avcodec_close(_ffmpeg->audio_codec_ctxs[i]);
        }
    }
    for (size_t i = 0; i < _ffmpeg->audio_packet_queues.size(); i++)
    {
        if (_ffmpeg->audio_packet_queues[i].size() > 0)
        {
            msg::dbg(_url + ": " + str::from(_ffmpeg->audio_packet_queues[i].size())
                    + " unprocessed audio packets in audio stream " + str::from(i));
        }
        for (size_t j = 0; j < _ffmpeg->audio_packet_queues[i].size(); j++)
        {
            av_free_packet(&_ffmpeg->audio_packet_queues[i][j]);
        }
    }
    if (_ffmpeg->format_ctx)
    {
        av_close_input_file(_ffmpeg->format_ctx);
    }
    delete _ffmpeg;
    _ffmpeg = NULL;
}
