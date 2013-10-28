/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <frederic.devernay@inrialpes.fr>
 * Joe <cuchac@email.cz>
 * D. Matz <bandregent@yahoo.de>
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
#include <libavdevice/avdevice.h>
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

#include "gettext.h"
#define _(string) gettext(string)

#include "dbg.h"
#include "blob.h"
#include "exc.h"
#include "msg.h"
#include "str.h"
#include "thread.h"

#include "media_object.h"


// The read thread.
// This thread reads packets from the AVFormatContext and stores them in the
// appropriate packet queues.
class read_thread : public thread
{
private:
    const std::string _url;
    const bool _is_device;
    struct ffmpeg_stuff *_ffmpeg;
    bool _eof;

public:
    read_thread(const std::string &url, bool is_device, struct ffmpeg_stuff *ffmpeg);
    void run();
    void reset();
    bool eof() const
    {
        return _eof;
    }
};

// The video decode thread.
// This thread reads packets from its packet queue and decodes them to video frames.
class video_decode_thread : public thread
{
private:
    std::string _url;
    struct ffmpeg_stuff *_ffmpeg;
    int _video_stream;
    video_frame _frame;
    int _raw_frames;

    int64_t handle_timestamp(int64_t timestamp);

public:
    video_decode_thread(const std::string &url, struct ffmpeg_stuff *ffmpeg, int video_stream);
    void set_raw_frames(int raw_frames)
    {
        _raw_frames = raw_frames;
    }
    void run();
    const video_frame &frame()
    {
        return _frame;
    }
};

// The audio decode thread.
// This thread reads packets from its packet queue and decodes them to audio blobs.
class audio_decode_thread : public thread
{
private:
    std::string _url;
    struct ffmpeg_stuff *_ffmpeg;
    int _audio_stream;
    audio_blob _blob;

    int64_t handle_timestamp(int64_t timestamp);

public:
    audio_decode_thread(const std::string &url, struct ffmpeg_stuff *ffmpeg, int audio_stream);
    void run();
    const audio_blob &blob()
    {
        return _blob;
    }
};

// The subtitle decode thread.
// This thread reads packets from its packet queue and decodes them to subtitle boxes.
class subtitle_decode_thread : public thread
{
private:
    std::string _url;
    struct ffmpeg_stuff *_ffmpeg;
    int _subtitle_stream;
    subtitle_box _box;

    int64_t handle_timestamp(int64_t timestamp);

public:
    subtitle_decode_thread(const std::string &url, struct ffmpeg_stuff *ffmpeg, int subtitle_stream);
    void run();
    const subtitle_box &box()
    {
        return _box;
    }
};


// Hide the FFmpeg stuff so that their messy header files cannot cause problems
// in other source files.

static const size_t max_audio_frame_size = 19200; // 1 second of 48khz 32bit audio
static const size_t audio_tmpbuf_size = (max_audio_frame_size * 3) / 2;

struct ffmpeg_stuff
{
    AVFormatContext *format_ctx;

    bool have_active_audio_stream;
    int64_t pos;

    read_thread *reader;

    std::vector<int> video_streams;
    std::vector<AVCodecContext *> video_codec_ctxs;
    std::vector<video_frame> video_frame_templates;
    std::vector<struct SwsContext *> video_sws_ctxs;
    std::vector<AVCodec *> video_codecs;
    std::vector<std::deque<AVPacket> > video_packet_queues;
    std::vector<mutex> video_packet_queue_mutexes;
    std::vector<AVPacket> video_packets;
    std::vector<video_decode_thread> video_decode_threads;
    std::vector<AVFrame *> video_frames;
    std::vector<AVFrame *> video_buffered_frames;
    std::vector<uint8_t *> video_buffers;
    std::vector<AVFrame *> video_sws_frames;
    std::vector<uint8_t *> video_sws_buffers;
    std::vector<int64_t> video_last_timestamps;

    std::vector<int> audio_streams;
    std::vector<AVCodecContext *> audio_codec_ctxs;
    std::vector<audio_blob> audio_blob_templates;
    std::vector<AVCodec *> audio_codecs;
    std::vector<std::deque<AVPacket> > audio_packet_queues;
    std::vector<mutex> audio_packet_queue_mutexes;
    std::vector<audio_decode_thread> audio_decode_threads;
    std::vector<unsigned char *> audio_tmpbufs;
    std::vector<blob> audio_blobs;
    std::vector<std::vector<unsigned char> > audio_buffers;
    std::vector<int64_t> audio_last_timestamps;

    std::vector<int> subtitle_streams;
    std::vector<AVCodecContext *> subtitle_codec_ctxs;
    std::vector<subtitle_box> subtitle_box_templates;
    std::vector<AVCodec *> subtitle_codecs;
    std::vector<std::deque<AVPacket> > subtitle_packet_queues;
    std::vector<mutex> subtitle_packet_queue_mutexes;
    std::vector<subtitle_decode_thread> subtitle_decode_threads;
    std::vector<std::deque<subtitle_box> > subtitle_box_buffers;
    std::vector<int64_t> subtitle_last_timestamps;
};

// Use one decoding thread per processor for video decoding.
static int video_decoding_threads()
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
    static mutex line_mutex;
    static std::string line;
    if (level > av_log_get_level())
    {
        return;
    }

    line_mutex.lock();
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
    line_mutex.unlock();
}

// Handle timestamps
static int64_t timestamp_helper(int64_t &last_timestamp, int64_t timestamp)
{
    if (timestamp == std::numeric_limits<int64_t>::min())
    {
        timestamp = last_timestamp;
    }
    last_timestamp = timestamp;
    return timestamp;
}

// Get a stream duration
static int64_t stream_duration(AVStream *stream, AVFormatContext *format)
{
    // Try to get duration from the stream first. If that fails, fall back to
    // the value provided by the container.
    int64_t duration = stream->duration;
    if (duration > 0)
    {
        AVRational time_base = stream->time_base;
        return duration * 1000000 * time_base.num / time_base.den;
    }
    else
    {
        duration = format->duration;
        return duration * 1000000 / AV_TIME_BASE;
    }
}

// Get a file name (or URL) extension, if any
static std::string get_extension(const std::string& url)
{
    std::string extension;
    size_t dot_pos = url.find_last_of('.');
    if (dot_pos != std::string::npos) {
        extension = url.substr(dot_pos + 1);
        for (size_t i = 0; i < extension.length(); i++)
            extension[i] =  std::tolower(extension[i]);
    }
    return extension;
}


media_object::media_object(bool always_convert_to_bgra32) :
    _always_convert_to_bgra32(always_convert_to_bgra32), _ffmpeg(NULL)
{
    avdevice_register_all();
    av_register_all();
    avformat_network_init();
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

void media_object::set_video_frame_template(int index, int width_before_avcodec_open, int height_before_avcodec_open)
{
    AVStream *video_stream = _ffmpeg->format_ctx->streams[_ffmpeg->video_streams[index]];
    AVCodecContext *video_codec_ctx = _ffmpeg->video_codec_ctxs[index];
    video_frame &video_frame_template = _ffmpeg->video_frame_templates[index];

    // Dimensions and aspect ratio
    video_frame_template.raw_width = video_codec_ctx->width;
    video_frame_template.raw_height = video_codec_ctx->height;
    // XXX Use width/height values from before avcodec_open() if they
    // differ and seem safe to use. See also media_object::open().
    if (width_before_avcodec_open >= 1
            && height_before_avcodec_open >= 1
            && width_before_avcodec_open <= video_codec_ctx->width
            && height_before_avcodec_open <= video_codec_ctx->height
            && (width_before_avcodec_open != video_codec_ctx->width
                || height_before_avcodec_open != video_codec_ctx->height))
    {
        msg::dbg("%s video stream %d: using frame size %dx%d instead of %dx%d.",
                _url.c_str(), index + 1,
                width_before_avcodec_open, height_before_avcodec_open,
                video_codec_ctx->width, video_codec_ctx->height);
        video_frame_template.raw_width = width_before_avcodec_open;
        video_frame_template.raw_height = height_before_avcodec_open;
    }
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
    if (!_always_convert_to_bgra32
            && (video_codec_ctx->pix_fmt == PIX_FMT_YUV444P
                || video_codec_ctx->pix_fmt == PIX_FMT_YUV444P10
                || video_codec_ctx->pix_fmt == PIX_FMT_YUV422P
                || video_codec_ctx->pix_fmt == PIX_FMT_YUV422P10
                || video_codec_ctx->pix_fmt == PIX_FMT_YUV420P
                || video_codec_ctx->pix_fmt == PIX_FMT_YUV420P10))
    {
        if (video_codec_ctx->pix_fmt == PIX_FMT_YUV444P
                || video_codec_ctx->pix_fmt == PIX_FMT_YUV444P10)
        {
            video_frame_template.layout = video_frame::yuv444p;
        }
        else if (video_codec_ctx->pix_fmt == PIX_FMT_YUV422P
                || video_codec_ctx->pix_fmt == PIX_FMT_YUV422P10)
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
        if (video_codec_ctx->pix_fmt == PIX_FMT_YUV444P10
                || video_codec_ctx->pix_fmt == PIX_FMT_YUV422P10
                || video_codec_ctx->pix_fmt == PIX_FMT_YUV420P10)
        {
            video_frame_template.value_range = video_frame::u10_mpeg;
            if (video_codec_ctx->color_range == AVCOL_RANGE_JPEG)
            {
                video_frame_template.value_range = video_frame::u10_full;
            }
        }
        else
        {
            video_frame_template.value_range = video_frame::u8_mpeg;
            if (video_codec_ctx->color_range == AVCOL_RANGE_JPEG)
            {
                video_frame_template.value_range = video_frame::u8_full;
            }
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
    else if (!_always_convert_to_bgra32
            && (video_codec_ctx->pix_fmt == PIX_FMT_YUVJ444P
                || video_codec_ctx->pix_fmt == PIX_FMT_YUVJ422P
                || video_codec_ctx->pix_fmt == PIX_FMT_YUVJ420P))
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
    video_frame_template.stereo_layout = parameters::layout_mono;
    video_frame_template.stereo_layout_swap = false;
    std::string val;
    /* Determine the stereo layout from the resolution.*/
    if (video_frame_template.raw_width / 2 > video_frame_template.raw_height)
    {
        video_frame_template.stereo_layout = parameters::layout_left_right;
    }
    else if (video_frame_template.raw_height > video_frame_template.raw_width)
    {
        video_frame_template.stereo_layout = parameters::layout_top_bottom;
    }
    /* Gather hints from the filename extension */
    std::string extension = get_extension(_url);
    if (extension == "mpo")
    {
        /* MPO files are alternating-left-right. */
        video_frame_template.stereo_layout = parameters::layout_alternating;
    }
    else if (extension == "jps" || extension == "pns")
    {
        /* JPS and PNS are side-by-side in right-left mode */
        video_frame_template.stereo_layout = parameters::layout_left_right;
        video_frame_template.stereo_layout_swap = true;
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
        video_frame_template.stereo_layout = parameters::layout_left_right;
        video_frame_template.stereo_layout_swap = false;
    }
    else if (marker == "rl")
    {
        video_frame_template.stereo_layout = parameters::layout_left_right;
        video_frame_template.stereo_layout_swap = true;
    }
    else if (marker == "lrh" || marker == "lrq")
    {
        video_frame_template.stereo_layout = parameters::layout_left_right_half;
        video_frame_template.stereo_layout_swap = false;
    }
    else if (marker == "rlh" || marker == "rlq")
    {
        video_frame_template.stereo_layout = parameters::layout_left_right_half;
        video_frame_template.stereo_layout_swap = true;
    }
    else if (marker == "tb" || marker == "ab")
    {
        video_frame_template.stereo_layout = parameters::layout_top_bottom;
        video_frame_template.stereo_layout_swap = false;
    }
    else if (marker == "bt" || marker == "ba")
    {
        video_frame_template.stereo_layout = parameters::layout_top_bottom;
        video_frame_template.stereo_layout_swap = true;
    }
    else if (marker == "tbh" || marker == "abq")
    {
        video_frame_template.stereo_layout = parameters::layout_top_bottom_half;
        video_frame_template.stereo_layout_swap = false;
    }
    else if (marker == "bth" || marker == "baq")
    {
        video_frame_template.stereo_layout = parameters::layout_top_bottom_half;
        video_frame_template.stereo_layout_swap = true;
    }
    else if (marker == "eo")
    {
        video_frame_template.stereo_layout = parameters::layout_even_odd_rows;
        video_frame_template.stereo_layout_swap = false;
        // all image lines are given in this case, and there should be no interpolation [TODO]
    }
    else if (marker == "oe")
    {
        video_frame_template.stereo_layout = parameters::layout_even_odd_rows;
        video_frame_template.stereo_layout_swap = true;
        // all image lines are given in this case, and there should be no interpolation [TODO]
    }
    else if (marker == "eoq" || marker == "3dir")
    {
        video_frame_template.stereo_layout = parameters::layout_even_odd_rows;
        video_frame_template.stereo_layout_swap = false;
    }
    else if (marker == "oeq" || marker == "3di")
    {
        video_frame_template.stereo_layout = parameters::layout_even_odd_rows;
        video_frame_template.stereo_layout_swap = true;
    }
    else if (marker == "2d")
    {
        video_frame_template.stereo_layout = parameters::layout_mono;
        video_frame_template.stereo_layout_swap = false;
    }
    /* Check some tags defined at this link: http://www.3dtv.at/Knowhow/StereoWmvSpec_en.aspx
     * This is necessary to make the example movies provided by 3dtv.at work out of the box. */
    val = tag_value("StereoscopicLayout");
    if (val == "SideBySideRF" || val == "SideBySideLF")
    {
        video_frame_template.stereo_layout_swap = (val == "SideBySideRF");
        val = tag_value("StereoscopicHalfWidth");
        video_frame_template.stereo_layout = (val == "1" ? parameters::layout_left_right_half : parameters::layout_left_right);

    }
    else if (val == "OverUnderRT" || val == "OverUnderLT")
    {
        video_frame_template.stereo_layout_swap = (val == "OverUnderRT");
        val = tag_value("StereoscopicHalfHeight");
        video_frame_template.stereo_layout = (val == "1" ? parameters::layout_top_bottom_half : parameters::layout_top_bottom);
    }
    /* Check the Matroska StereoMode metadata, which is translated by FFmpeg to a "stereo_mode" tag.
     * This tag is per-track, not per-file!
     * This tag is the most reliable source of information about the stereo layout and should be used
     * by everyone. Unfortunately, we still have to look at the resolution to guess whether we have
     * a reduced resolution (*_half) stereo layout. */
    val = "";
    AVDictionaryEntry *tag = NULL;
    while ((tag = av_dict_get(video_stream->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
    {
        if (std::string(tag->key) == "stereo_mode")
        {
            val = tag->value;
            break;
        }
    }
    if (val == "mono")
    {
        video_frame_template.stereo_layout = parameters::layout_mono;
        video_frame_template.stereo_layout_swap = false;
    }
    else if (val == "left_right" || val == "right_left")
    {
        if (video_frame_template.raw_width / 2 > video_frame_template.raw_height)
        {
            video_frame_template.stereo_layout = parameters::layout_left_right;
        }
        else
        {
            video_frame_template.stereo_layout = parameters::layout_left_right_half;
        }
        video_frame_template.stereo_layout_swap = (val == "right_left");
    }
    else if (val == "top_bottom" || val == "bottom_top")
    {
        if (video_frame_template.raw_height > video_frame_template.raw_width)
        {
            video_frame_template.stereo_layout = parameters::layout_top_bottom;
        }
        else
        {
            video_frame_template.stereo_layout = parameters::layout_top_bottom_half;
        }
        video_frame_template.stereo_layout_swap = (val == "bottom_top");
    }
    else if (val == "row_interleaved_lr" || val == "row_interleaved_rl")
    {
        video_frame_template.stereo_layout = parameters::layout_even_odd_rows;
        video_frame_template.stereo_layout_swap = (val == "row_interleaved_rl");
    }
    else if (val == "block_lr" || val == "block_rl")
    {
        video_frame_template.stereo_layout = parameters::layout_alternating;
        video_frame_template.stereo_layout_swap = (val == "block_rl");
    }
    else if (!val.empty())
    {
        msg::wrn(_("%s video stream %d: Unsupported stereo layout %s."),
                _url.c_str(), index + 1, str::sanitize(val).c_str());
        video_frame_template.stereo_layout = parameters::layout_mono;
        video_frame_template.stereo_layout_swap = false;
    }
    /* Sanity checks. If these fail, use safe fallback */
    if (((video_frame_template.stereo_layout == parameters::layout_left_right
                    || video_frame_template.stereo_layout == parameters::layout_left_right_half)
                && video_frame_template.raw_width % 2 != 0)
            || ((video_frame_template.stereo_layout == parameters::layout_top_bottom
                    || video_frame_template.stereo_layout == parameters::layout_top_bottom_half)
                && video_frame_template.raw_height % 2 != 0)
            || (video_frame_template.stereo_layout == parameters::layout_even_odd_rows
                && video_frame_template.raw_height % 2 != 0))
    {
        video_frame_template.stereo_layout = parameters::layout_mono;
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

    AVDictionaryEntry *tag = av_dict_get(audio_stream->metadata, "language", NULL, AV_DICT_IGNORE_SUFFIX);
    if (tag)
    {
        audio_blob_template.language = tag->value;
    }
    if (audio_codec_ctx->channels < 1
            || audio_codec_ctx->channels > 8
            || audio_codec_ctx->channels == 3
            || audio_codec_ctx->channels == 5)
    {
        throw exc(str::asprintf(_("%s audio stream %d: Cannot handle audio with %d channels."),
                    _url.c_str(), index + 1, audio_codec_ctx->channels));
    }
    audio_blob_template.channels = audio_codec_ctx->channels;
    audio_blob_template.rate = audio_codec_ctx->sample_rate;
    if (audio_codec_ctx->sample_fmt == AV_SAMPLE_FMT_U8
            || audio_codec_ctx->sample_fmt == AV_SAMPLE_FMT_U8P)
    {
        audio_blob_template.sample_format = audio_blob::u8;
    }
    else if (audio_codec_ctx->sample_fmt == AV_SAMPLE_FMT_S16
            || audio_codec_ctx->sample_fmt == AV_SAMPLE_FMT_S16P)
    {
        audio_blob_template.sample_format = audio_blob::s16;
    }
    else if (audio_codec_ctx->sample_fmt == AV_SAMPLE_FMT_FLT
            || audio_codec_ctx->sample_fmt == AV_SAMPLE_FMT_FLTP)
    {
        audio_blob_template.sample_format = audio_blob::f32;
    }
    else if (audio_codec_ctx->sample_fmt == AV_SAMPLE_FMT_DBL
            || audio_codec_ctx->sample_fmt == AV_SAMPLE_FMT_DBLP)
    {
        audio_blob_template.sample_format = audio_blob::d64;
    }
    else if ((audio_codec_ctx->sample_fmt == AV_SAMPLE_FMT_S32
                || audio_codec_ctx->sample_fmt == AV_SAMPLE_FMT_S32P)
            && sizeof(int32_t) == sizeof(float))
    {
        // we need to convert this to AV_SAMPLE_FMT_FLT after decoding
        audio_blob_template.sample_format = audio_blob::f32;
    }
    else
    {
        throw exc(str::asprintf(_("%s audio stream %d: Cannot handle audio with sample format %s."),
                    _url.c_str(), index + 1, av_get_sample_fmt_name(audio_codec_ctx->sample_fmt)));
    }
}

void media_object::set_subtitle_box_template(int index)
{
    AVStream *subtitle_stream = _ffmpeg->format_ctx->streams[_ffmpeg->subtitle_streams[index]];
    //AVCodecContext *subtitle_codec_ctx = _ffmpeg->subtitle_codec_ctxs[index];
    subtitle_box &subtitle_box_template = _ffmpeg->subtitle_box_templates[index];

    AVDictionaryEntry *tag = av_dict_get(subtitle_stream->metadata, "language", NULL, AV_DICT_IGNORE_SUFFIX);
    if (tag)
    {
        subtitle_box_template.language = tag->value;
    }
}

void media_object::open(const std::string &url, const device_request &dev_request)
{
    assert(!_ffmpeg);

    _url = url;
    _is_device = dev_request.is_device();
    _ffmpeg = new struct ffmpeg_stuff;
    _ffmpeg->format_ctx = NULL;
    _ffmpeg->have_active_audio_stream = false;
    _ffmpeg->pos = 0;
    _ffmpeg->reader = new read_thread(_url, _is_device, _ffmpeg);
    int e;

    /* Set format and parameters for device input */
    AVInputFormat *iformat = NULL;
    AVDictionary *iparams = NULL;
    switch (dev_request.device)
    {
    case device_request::firewire:
        iformat = av_find_input_format("libdc1394");
        break;
    case device_request::x11:
        iformat = av_find_input_format("x11grab");
        break;
    case device_request::sys_default:
#if (defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__
        iformat = av_find_input_format("vfwcap");
#elif defined __FreeBSD__ || defined __NetBSD__ || defined __OpenBSD__ || defined __APPLE__
        iformat = av_find_input_format("bktr");
#else
        iformat = av_find_input_format("video4linux2");
#endif
        break;
    case device_request::no_device:
        /* Force the format for a few file types that are unknown to older
         * versions of FFmpeg and to Libav. Note: this may be removed in
         * the future if all relevant FFmpeg/Libav versions recognize these
         * files automatically.
         * This fixes the problems for MPO and JPS images listed here:
         * http://lists.nongnu.org/archive/html/bino-list/2012-03/msg00026.html
         * This does not fix the PNS problem, because the format is "image2" but
         * we also need to tell FFmpeg about the codec (PNG), which does not
         * seem to be possible.
         */
        {
            std::string extension = get_extension(_url);
            if (extension == "mpo" || extension == "jps")
                iformat = av_find_input_format("mjpeg");
        }
        break;
    }
    if (_is_device && !iformat)
    {
        throw exc(str::asprintf(_("No support available for %s device."),
                dev_request.device == device_request::firewire ? _("Firewire")
                : dev_request.device == device_request::x11 ? _("X11")
                : _("default")));
    }
    if (_is_device && dev_request.width != 0 && dev_request.height != 0)
    {
        av_dict_set(&iparams, "video_size", str::asprintf("%dx%d",
                    dev_request.width, dev_request.height).c_str(), 0);
    }
    if (_is_device && dev_request.frame_rate_num != 0 && dev_request.frame_rate_den != 0)
    {
        av_dict_set(&iparams, "framerate", str::asprintf("%d/%d",
                    dev_request.frame_rate_num, dev_request.frame_rate_den).c_str(), 0);
    }
    if (_is_device && dev_request.request_mjpeg)
    {
        av_dict_set(&iparams, "input_format", "mjpeg", 0);
    }

    /* Open the input */
    _ffmpeg->format_ctx = NULL;
    if ((e = avformat_open_input(&_ffmpeg->format_ctx, _url.c_str(), iformat, &iparams)) != 0)
    {
        av_dict_free(&iparams);
        throw exc(str::asprintf(_("%s: %s"),
                    _url.c_str(), my_av_strerror(e).c_str()));
    }
    av_dict_free(&iparams);
    if (_is_device)
    {
        // For a camera device, do not read ahead multiple packets, to avoid a startup delay.
        _ffmpeg->format_ctx->max_analyze_duration = 0;
    }
    if ((e = avformat_find_stream_info(_ffmpeg->format_ctx, NULL)) < 0)
    {
        throw exc(str::asprintf(_("%s: Cannot read stream info: %s"),
                    _url.c_str(), my_av_strerror(e).c_str()));
    }
    av_dump_format(_ffmpeg->format_ctx, 0, _url.c_str(), 0);

    /* Metadata */
    AVDictionaryEntry *tag = NULL;
    while ((tag = av_dict_get(_ffmpeg->format_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
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
        AVCodecContext *codec_ctx = _ffmpeg->format_ctx->streams[i]->codec;
        AVCodec *codec = (codec_ctx->codec_id == CODEC_ID_TEXT
                ? NULL : avcodec_find_decoder(codec_ctx->codec_id));
        // XXX: Sometimes the reported width and height for a video stream change after avcodec_open(),
        // but the original values seem to be correct. This seems to happen mostly with 1920x1080 video
        // that later is reported as 1920x1088, which results in a gray bar displayed at the bottom of
        // the frame. FFplay is also affected. As a workaround, we keep the original values here and use
        // them later in set_video_frame_template().
        int width_before_avcodec_open = codec_ctx->width;
        int height_before_avcodec_open = codec_ctx->height;
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            // Activate multithreaded decoding. This must be done before opening the codec; see
            // http://lists.gnu.org/archive/html/bino-list/2011-08/msg00019.html
            codec_ctx->thread_count = video_decoding_threads();
            // Set CODEC_FLAG_EMU_EDGE in the same situations in which ffplay sets it.
            // I don't know what exactly this does, but it is necessary to fix the problem
            // described in this thread: http://lists.nongnu.org/archive/html/bino-list/2012-02/msg00039.html
            if (codec_ctx->lowres || (codec && (codec->capabilities & CODEC_CAP_DR1)))
                codec_ctx->flags |= CODEC_FLAG_EMU_EDGE;
        }
        // Find and open the codec. CODEC_ID_TEXT is a special case: it has no decoder since it is unencoded raw data.
        if (codec_ctx->codec_id != CODEC_ID_TEXT && (!codec || (e = avcodec_open2(codec_ctx, codec, NULL)) < 0))
        {
            msg::wrn(_("%s stream %d: Cannot open %s: %s"), _url.c_str(), i,
                    codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ? _("video codec")
                    : codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO ? _("audio codec")
                    : codec_ctx->codec_type == AVMEDIA_TYPE_SUBTITLE ? _("subtitle codec")
                    : _("data"),
                    codec ? my_av_strerror(e).c_str() : _("codec not supported"));
        }
        else if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            _ffmpeg->video_streams.push_back(i);
            int j = _ffmpeg->video_streams.size() - 1;
            msg::dbg(_url + " stream " + str::from(i) + " is video stream " + str::from(j) + ".");
            _ffmpeg->video_codec_ctxs.push_back(codec_ctx);
            if (_ffmpeg->video_codec_ctxs[j]->width < 1 || _ffmpeg->video_codec_ctxs[j]->height < 1)
            {
                throw exc(str::asprintf(_("%s video stream %d: Invalid frame size."),
                            _url.c_str(), j + 1));
            }
            _ffmpeg->video_codecs.push_back(codec);
            // Determine frame template.
            _ffmpeg->video_frame_templates.push_back(video_frame());
            set_video_frame_template(j, width_before_avcodec_open, height_before_avcodec_open);
            // Allocate things required for decoding
            _ffmpeg->video_packets.push_back(AVPacket());
            av_init_packet(&(_ffmpeg->video_packets[j]));
            _ffmpeg->video_decode_threads.push_back(video_decode_thread(_url, _ffmpeg, j));
            _ffmpeg->video_frames.push_back(avcodec_alloc_frame());
            _ffmpeg->video_buffered_frames.push_back(avcodec_alloc_frame());
            enum PixelFormat frame_fmt = (_ffmpeg->video_frame_templates[j].layout == video_frame::bgra32
                    ? PIX_FMT_BGRA : _ffmpeg->video_codec_ctxs[j]->pix_fmt);
            int frame_bufsize = (avpicture_get_size(frame_fmt,
                        _ffmpeg->video_codec_ctxs[j]->width, _ffmpeg->video_codec_ctxs[j]->height));
            _ffmpeg->video_buffers.push_back(static_cast<uint8_t *>(av_malloc(frame_bufsize)));
            avpicture_fill(reinterpret_cast<AVPicture *>(_ffmpeg->video_buffered_frames[j]), _ffmpeg->video_buffers[j],
                    frame_fmt, _ffmpeg->video_codec_ctxs[j]->width, _ffmpeg->video_codec_ctxs[j]->height);
            if (!_ffmpeg->video_frames[j] || !_ffmpeg->video_buffered_frames[j] || !_ffmpeg->video_buffers[j])
            {
                throw exc(HERE + ": " + strerror(ENOMEM));
            }
            if (_ffmpeg->video_frame_templates[j].layout == video_frame::bgra32)
            {
                // Initialize things needed for software pixel format conversion
                int sws_bufsize = avpicture_get_size(PIX_FMT_BGRA,
                        _ffmpeg->video_codec_ctxs[j]->width, _ffmpeg->video_codec_ctxs[j]->height);
                _ffmpeg->video_sws_frames.push_back(avcodec_alloc_frame());
                _ffmpeg->video_sws_buffers.push_back(static_cast<uint8_t *>(av_malloc(sws_bufsize)));
                if (!_ffmpeg->video_sws_frames[j] || !_ffmpeg->video_sws_buffers[j])
                {
                    throw exc(HERE + ": " + strerror(ENOMEM));
                }
                avpicture_fill(reinterpret_cast<AVPicture *>(_ffmpeg->video_sws_frames[j]), _ffmpeg->video_sws_buffers[j],
                        PIX_FMT_BGRA, _ffmpeg->video_codec_ctxs[j]->width, _ffmpeg->video_codec_ctxs[j]->height);
                // Call sws_getCachedContext(NULL, ...) instead of sws_getContext(...) just to avoid a deprecation warning.
                _ffmpeg->video_sws_ctxs.push_back(sws_getCachedContext(NULL,
                            _ffmpeg->video_codec_ctxs[j]->width, _ffmpeg->video_codec_ctxs[j]->height, _ffmpeg->video_codec_ctxs[j]->pix_fmt,
                            _ffmpeg->video_codec_ctxs[j]->width, _ffmpeg->video_codec_ctxs[j]->height, PIX_FMT_BGRA,
                            SWS_POINT, NULL, NULL, NULL));
                if (!_ffmpeg->video_sws_ctxs[j])
                {
                    throw exc(str::asprintf(_("%s video stream %d: Cannot initialize conversion context."),
                                _url.c_str(), j + 1));
                }
            }
            else
            {
                _ffmpeg->video_sws_frames.push_back(NULL);
                _ffmpeg->video_sws_buffers.push_back(NULL);
                _ffmpeg->video_sws_ctxs.push_back(NULL);
            }
            _ffmpeg->video_last_timestamps.push_back(std::numeric_limits<int64_t>::min());
        }
        else if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            _ffmpeg->audio_streams.push_back(i);
            int j = _ffmpeg->audio_streams.size() - 1;
            msg::dbg(_url + " stream " + str::from(i) + " is audio stream " + str::from(j) + ".");
            _ffmpeg->audio_codec_ctxs.push_back(codec_ctx);
            _ffmpeg->audio_codecs.push_back(codec);
            _ffmpeg->audio_blob_templates.push_back(audio_blob());
            set_audio_blob_template(j);
            _ffmpeg->audio_decode_threads.push_back(audio_decode_thread(_url, _ffmpeg, j));
            // Manage audio_tmpbufs with av_malloc/av_free, to guarantee correct alignment.
            // Not doing this results in hard to debug crashes on some systems.
            _ffmpeg->audio_tmpbufs.push_back(static_cast<unsigned char*>(av_malloc(audio_tmpbuf_size)));
            if (!_ffmpeg->audio_tmpbufs[j])
            {
                throw exc(HERE + ": " + strerror(ENOMEM));
            }
            _ffmpeg->audio_blobs.push_back(blob());
            _ffmpeg->audio_buffers.push_back(std::vector<unsigned char>());
            _ffmpeg->audio_last_timestamps.push_back(std::numeric_limits<int64_t>::min());
        }
        else if (codec_ctx->codec_type == AVMEDIA_TYPE_SUBTITLE)
        {
            _ffmpeg->subtitle_streams.push_back(i);
            int j = _ffmpeg->subtitle_streams.size() - 1;
            msg::dbg(_url + " stream " + str::from(i) + " is subtitle stream " + str::from(j) + ".");
            _ffmpeg->subtitle_codec_ctxs.push_back(codec_ctx);
            // CODEC_ID_TEXT does not have any decoder; it is just UTF-8 text in the packet data.
            _ffmpeg->subtitle_codecs.push_back(
                    _ffmpeg->subtitle_codec_ctxs[j]->codec_id == CODEC_ID_TEXT ? NULL : codec);
            _ffmpeg->subtitle_box_templates.push_back(subtitle_box());
            set_subtitle_box_template(j);
            _ffmpeg->subtitle_decode_threads.push_back(subtitle_decode_thread(_url, _ffmpeg, j));
            _ffmpeg->subtitle_box_buffers.push_back(std::deque<subtitle_box>());
            _ffmpeg->subtitle_last_timestamps.push_back(std::numeric_limits<int64_t>::min());
        }
        else
        {
            msg::dbg(_url + " stream " + str::from(i) + " contains neither video nor audio nor subtitles.");
        }
    }
    _ffmpeg->video_packet_queues.resize(video_streams());
    _ffmpeg->audio_packet_queues.resize(audio_streams());
    _ffmpeg->subtitle_packet_queues.resize(subtitle_streams());
    _ffmpeg->video_packet_queue_mutexes.resize(video_streams());
    _ffmpeg->audio_packet_queue_mutexes.resize(audio_streams());
    _ffmpeg->subtitle_packet_queue_mutexes.resize(subtitle_streams());

    msg::inf(_url + ":");
    for (int i = 0; i < video_streams(); i++)
    {
        msg::inf(4, _("Video stream %d: %s / %s, %g seconds"), i,
                video_frame_template(i).format_info().c_str(),
                video_frame_template(i).format_name().c_str(),
                video_duration(i) / 1e6f);
        msg::inf(8, _("Using up to %d threads for decoding."),
                _ffmpeg->video_codec_ctxs.at(i)->thread_count);
    }
    for (int i = 0; i < audio_streams(); i++)
    {
        msg::inf(4, _("Audio stream %d: %s / %s, %g seconds"), i,
                audio_blob_template(i).format_info().c_str(),
                audio_blob_template(i).format_name().c_str(),
                audio_duration(i) / 1e6f);
    }
    for (int i = 0; i < subtitle_streams(); i++)
    {
        msg::inf(4, _("Subtitle stream %d: %s / %s, %g seconds"), i,
                subtitle_box_template(i).format_info().c_str(),
                subtitle_box_template(i).format_name().c_str(),
                subtitle_duration(i) / 1e6f);
    }
    if (video_streams() == 0 && audio_streams() == 0 && subtitle_streams() == 0)
    {
        msg::inf(4, _("No usable streams."));
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

int media_object::video_streams() const
{
    return _ffmpeg->video_streams.size();
}

int media_object::audio_streams() const
{
    return _ffmpeg->audio_streams.size();
}

int media_object::subtitle_streams() const
{
    return _ffmpeg->subtitle_streams.size();
}

void media_object::video_stream_set_active(int index, bool active)
{
    assert(index >= 0);
    assert(index < video_streams());
    // Stop decoder threads
    for (size_t i = 0; i < _ffmpeg->video_streams.size(); i++)
    {
        _ffmpeg->video_decode_threads[i].finish();
    }
    for (size_t i = 0; i < _ffmpeg->audio_streams.size(); i++)
    {
        _ffmpeg->audio_decode_threads[i].finish();
    }
    for (size_t i = 0; i < _ffmpeg->subtitle_streams.size(); i++)
    {
        _ffmpeg->subtitle_decode_threads[i].finish();
    }
    // Stop reading packets
    _ffmpeg->reader->finish();
    // Set status
    _ffmpeg->format_ctx->streams[_ffmpeg->video_streams.at(index)]->discard =
        (active ? AVDISCARD_DEFAULT : AVDISCARD_ALL);
    // Restart reader
    _ffmpeg->reader->start();
}

void media_object::audio_stream_set_active(int index, bool active)
{
    assert(index >= 0);
    assert(index < audio_streams());
    // Stop decoder threads
    for (size_t i = 0; i < _ffmpeg->video_streams.size(); i++)
    {
        _ffmpeg->video_decode_threads[i].finish();
    }
    for (size_t i = 0; i < _ffmpeg->audio_streams.size(); i++)
    {
        _ffmpeg->audio_decode_threads[i].finish();
    }
    for (size_t i = 0; i < _ffmpeg->subtitle_streams.size(); i++)
    {
        _ffmpeg->subtitle_decode_threads[i].finish();
    }
    // Stop reading packets
    _ffmpeg->reader->finish();
    // Set status
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
    // Restart reader
    _ffmpeg->reader->start();
}

void media_object::subtitle_stream_set_active(int index, bool active)
{
    assert(index >= 0);
    assert(index < subtitle_streams());
    // Stop decoder threads
    for (size_t i = 0; i < _ffmpeg->video_streams.size(); i++)
    {
        _ffmpeg->video_decode_threads[i].finish();
    }
    for (size_t i = 0; i < _ffmpeg->audio_streams.size(); i++)
    {
        _ffmpeg->audio_decode_threads[i].finish();
    }
    for (size_t i = 0; i < _ffmpeg->subtitle_streams.size(); i++)
    {
        _ffmpeg->subtitle_decode_threads[i].finish();
    }
    // Stop reading packets
    _ffmpeg->reader->finish();
    // Set status
    _ffmpeg->format_ctx->streams[_ffmpeg->subtitle_streams.at(index)]->discard =
        (active ? AVDISCARD_DEFAULT : AVDISCARD_ALL);
    // Restart reader
    _ffmpeg->reader->start();
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
    int n = _ffmpeg->format_ctx->streams[_ffmpeg->video_streams.at(index)]->r_frame_rate.num;
    if (n <= 0)
        n = 1;
    return n;
}

int media_object::video_frame_rate_denominator(int index) const
{
    assert(index >= 0);
    assert(index < video_streams());
    int d = _ffmpeg->format_ctx->streams[_ffmpeg->video_streams.at(index)]->r_frame_rate.den;
    if (d <= 0)
        d = 1;
    return d;
}

int64_t media_object::video_duration(int index) const
{
    assert(index >= 0);
    assert(index < video_streams());
    return stream_duration(
            _ffmpeg->format_ctx->streams[_ffmpeg->video_streams.at(index)],
            _ffmpeg->format_ctx);
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
    return stream_duration(
            _ffmpeg->format_ctx->streams[_ffmpeg->audio_streams.at(index)],
            _ffmpeg->format_ctx);
}

const subtitle_box &media_object::subtitle_box_template(int subtitle_stream) const
{
    assert(subtitle_stream >= 0);
    assert(subtitle_stream < subtitle_streams());
    return _ffmpeg->subtitle_box_templates.at(subtitle_stream);
}

int64_t media_object::subtitle_duration(int index) const
{
    assert(index >= 0);
    assert(index < subtitle_streams());
    return stream_duration(
            _ffmpeg->format_ctx->streams[_ffmpeg->subtitle_streams.at(index)],
            _ffmpeg->format_ctx);
}

read_thread::read_thread(const std::string &url, bool is_device, struct ffmpeg_stuff *ffmpeg) :
    _url(url), _is_device(is_device), _ffmpeg(ffmpeg), _eof(false)
{
}

void read_thread::run()
{
    while (!_eof)
    {
        // We need another packet if the number of queued packets for an active stream is below a threshold.
        // For files, we often want to read ahead to avoid i/o waits. For devices, we do not want to read
        // ahead to avoid latency.
        const size_t video_stream_low_threshold = (_is_device ? 1 : 2);         // Often, 1 packet results in one video frame
        const size_t audio_stream_low_threshold = (_is_device ? 1 : 5);         // Often, 3-4 packets are needed for one buffer fill
        const size_t subtitle_stream_low_threshold = (_is_device ? 1 : 1);      // Just a guess
        bool need_another_packet = false;
        for (size_t i = 0; !need_another_packet && i < _ffmpeg->video_streams.size(); i++)
        {
            if (_ffmpeg->format_ctx->streams[_ffmpeg->video_streams[i]]->discard == AVDISCARD_DEFAULT)
            {
                _ffmpeg->video_packet_queue_mutexes[i].lock();
                need_another_packet = _ffmpeg->video_packet_queues[i].size() < video_stream_low_threshold;
                _ffmpeg->video_packet_queue_mutexes[i].unlock();
            }
        }
        for (size_t i = 0; !need_another_packet && i < _ffmpeg->audio_streams.size(); i++)
        {
            if (_ffmpeg->format_ctx->streams[_ffmpeg->audio_streams[i]]->discard == AVDISCARD_DEFAULT)
            {
                _ffmpeg->audio_packet_queue_mutexes[i].lock();
                    need_another_packet = _ffmpeg->audio_packet_queues[i].size() < audio_stream_low_threshold;
                    _ffmpeg->audio_packet_queue_mutexes[i].unlock();
            }
        }
        for (size_t i = 0; !need_another_packet && i < _ffmpeg->subtitle_streams.size(); i++)
        {
            if (_ffmpeg->format_ctx->streams[_ffmpeg->subtitle_streams[i]]->discard == AVDISCARD_DEFAULT)
            {
                _ffmpeg->subtitle_packet_queue_mutexes[i].lock();
                need_another_packet = _ffmpeg->subtitle_packet_queues[i].size() < subtitle_stream_low_threshold;
                _ffmpeg->subtitle_packet_queue_mutexes[i].unlock();
            }
        }
        if (!need_another_packet)
        {
            msg::dbg(_url + ": No need to read more packets.");
            break;
        }
        // Read a packet.
        msg::dbg(_url + ": Reading a packet.");
        AVPacket packet;
        int e = av_read_frame(_ffmpeg->format_ctx, &packet);
        if (e < 0)
        {
            if (e == AVERROR_EOF)
            {
                msg::dbg(_url + ": EOF.");
                _eof = true;
                return;
            }
            else
            {
                throw exc(str::asprintf(_("%s: %s"), _url.c_str(), my_av_strerror(e).c_str()));
            }
        }
        // Put the packet in the right queue.
        bool packet_queued = false;
        for (size_t i = 0; i < _ffmpeg->video_streams.size() && !packet_queued; i++)
        {
            if (packet.stream_index == _ffmpeg->video_streams[i])
            {
                // We do not check for missing timestamps here, as we do with audio
                // packets, for the following reasons:
                // 1. The video decoder might fill in a timestamp for us
                // 2. We cannot drop video packets anyway, because of their
                //    interdependencies. We would mess up decoding.
                if (av_dup_packet(&packet) < 0)
                {
                    throw exc(str::asprintf(_("%s: Cannot duplicate packet."), _url.c_str()));
                }
                _ffmpeg->video_packet_queue_mutexes[i].lock();
                _ffmpeg->video_packet_queues[i].push_back(packet);
                _ffmpeg->video_packet_queue_mutexes[i].unlock();
                packet_queued = true;
                msg::dbg(_url + ": "
                        + str::from(_ffmpeg->video_packet_queues[i].size())
                        + " packets queued in video stream " + str::from(i) + ".");
            }
        }
        for (size_t i = 0; i < _ffmpeg->audio_streams.size() && !packet_queued; i++)
        {
            if (packet.stream_index == _ffmpeg->audio_streams[i])
            {
                _ffmpeg->audio_packet_queue_mutexes[i].lock();
                if (_ffmpeg->audio_packet_queues[i].empty()
                        && _ffmpeg->audio_last_timestamps[i] == std::numeric_limits<int64_t>::min()
                        && packet.dts == static_cast<int64_t>(AV_NOPTS_VALUE))
                {
                    // We have no packet in the queue and no last timestamp, probably
                    // because we just seeked. We *need* a packet with a timestamp.
                    msg::dbg(_url + ": audio stream " + str::from(i)
                            + ": dropping packet because it has no timestamp");
                }
                else
                {
                    if (av_dup_packet(&packet) < 0)
                    {
                        _ffmpeg->audio_packet_queue_mutexes[i].unlock();
                        throw exc(str::asprintf(_("%s: Cannot duplicate packet."), _url.c_str()));
                    }
                    _ffmpeg->audio_packet_queues[i].push_back(packet);
                    packet_queued = true;
                    msg::dbg(_url + ": "
                            + str::from(_ffmpeg->audio_packet_queues[i].size())
                            + " packets queued in audio stream " + str::from(i) + ".");
                }
                _ffmpeg->audio_packet_queue_mutexes[i].unlock();
            }
        }
        for (size_t i = 0; i < _ffmpeg->subtitle_streams.size() && !packet_queued; i++)
        {
            if (packet.stream_index == _ffmpeg->subtitle_streams[i])
            {
                _ffmpeg->subtitle_packet_queue_mutexes[i].lock();
                if (_ffmpeg->subtitle_packet_queues[i].empty()
                        && _ffmpeg->subtitle_last_timestamps[i] == std::numeric_limits<int64_t>::min()
                        && packet.dts == static_cast<int64_t>(AV_NOPTS_VALUE))
                {
                    // We have no packet in the queue and no last timestamp, probably
                    // because we just seeked. We want a packet with a timestamp.
                    msg::dbg(_url + ": subtitle stream " + str::from(i)
                            + ": dropping packet because it has no timestamp");
                }
                else
                {
                    if (av_dup_packet(&packet) < 0)
                    {
                        _ffmpeg->subtitle_packet_queue_mutexes[i].unlock();
                        throw exc(str::asprintf(_("%s: Cannot duplicate packet."), _url.c_str()));
                    }
                    _ffmpeg->subtitle_packet_queues[i].push_back(packet);
                    packet_queued = true;
                    msg::dbg(_url + ": "
                            + str::from(_ffmpeg->subtitle_packet_queues[i].size())
                            + " packets queued in subtitle stream " + str::from(i) + ".");
                }
                _ffmpeg->subtitle_packet_queue_mutexes[i].unlock();
            }
        }
        if (!packet_queued)
        {
            av_free_packet(&packet);
        }
    }
}

void read_thread::reset()
{
    exception() = exc();
    _eof = false;
}

video_decode_thread::video_decode_thread(const std::string &url, struct ffmpeg_stuff *ffmpeg, int video_stream) :
    _url(url), _ffmpeg(ffmpeg), _video_stream(video_stream), _frame(), _raw_frames(1)
{
}

int64_t video_decode_thread::handle_timestamp(int64_t timestamp)
{
    int64_t ts = timestamp_helper(_ffmpeg->video_last_timestamps[_video_stream], timestamp);
    if (!_ffmpeg->have_active_audio_stream || _ffmpeg->pos == std::numeric_limits<int64_t>::min())
    {
        _ffmpeg->pos = ts;
    }
    return ts;
}

void video_decode_thread::run()
{
    _frame = _ffmpeg->video_frame_templates[_video_stream];
    for (int raw_frame = 0; raw_frame < _raw_frames; raw_frame++)
    {
        int frame_finished = 0;
read_frame:
        do
        {
            bool empty;
            do
            {
                _ffmpeg->video_packet_queue_mutexes[_video_stream].lock();
                empty = _ffmpeg->video_packet_queues[_video_stream].empty();
                _ffmpeg->video_packet_queue_mutexes[_video_stream].unlock();
                if (empty)
                {
                    if (_ffmpeg->reader->eof())
                    {
                        if (raw_frame == 1)
                        {
                            _frame.data[1][0] = _frame.data[0][0];
                            _frame.data[1][1] = _frame.data[0][1];
                            _frame.data[1][2] = _frame.data[0][2];
                            _frame.line_size[1][0] = _frame.line_size[0][0];
                            _frame.line_size[1][1] = _frame.line_size[0][1];
                            _frame.line_size[1][2] = _frame.line_size[0][2];
                        }
                        else
                        {
                            _frame = video_frame();
                        }
                        return;
                    }
                    msg::dbg(_url + ": video stream " + str::from(_video_stream) + ": need to wait for packets...");
                    _ffmpeg->reader->start();
                    _ffmpeg->reader->finish();
                }
            }
            while (empty);
            av_free_packet(&(_ffmpeg->video_packets[_video_stream]));
            _ffmpeg->video_packet_queue_mutexes[_video_stream].lock();
            _ffmpeg->video_packets[_video_stream] = _ffmpeg->video_packet_queues[_video_stream].front();
            _ffmpeg->video_packet_queues[_video_stream].pop_front();
            _ffmpeg->video_packet_queue_mutexes[_video_stream].unlock();
            _ffmpeg->reader->start();       // Refill the packet queue
            avcodec_decode_video2(_ffmpeg->video_codec_ctxs[_video_stream],
                    _ffmpeg->video_frames[_video_stream], &frame_finished,
                    &(_ffmpeg->video_packets[_video_stream]));
        }
        while (!frame_finished);
        if (_ffmpeg->video_frames[_video_stream]->width != _ffmpeg->video_frame_templates[_video_stream].raw_width
                || _ffmpeg->video_frames[_video_stream]->height != _ffmpeg->video_frame_templates[_video_stream].raw_height)
        {
            msg::wrn(_("%s video stream %d: Dropping %dx%d frame"), _url.c_str(), _video_stream + 1, 
                    _ffmpeg->video_frames[_video_stream]->width, _ffmpeg->video_frames[_video_stream]->height);
            goto read_frame;
        }
        if (_frame.layout == video_frame::bgra32)
        {
            sws_scale(_ffmpeg->video_sws_ctxs[_video_stream],
                    _ffmpeg->video_frames[_video_stream]->data,
                    _ffmpeg->video_frames[_video_stream]->linesize,
                    0, _frame.raw_height,
                    _ffmpeg->video_sws_frames[_video_stream]->data,
                    _ffmpeg->video_sws_frames[_video_stream]->linesize);
            // TODO: Handle sws_scale errors. How?
            _frame.data[raw_frame][0] = _ffmpeg->video_sws_frames[_video_stream]->data[0];
            _frame.line_size[raw_frame][0] = _ffmpeg->video_sws_frames[_video_stream]->linesize[0];
        }
        else
        {
            const AVFrame *src_frame = _ffmpeg->video_frames[_video_stream];
            if (_raw_frames == 2 && raw_frame == 0)
            {
                // We need to buffer the data because FFmpeg will clubber it when decoding the next frame.
                av_picture_copy(reinterpret_cast<AVPicture *>(_ffmpeg->video_buffered_frames[_video_stream]),
                        reinterpret_cast<AVPicture *>(_ffmpeg->video_frames[_video_stream]),
                        static_cast<enum PixelFormat>(_ffmpeg->video_codec_ctxs[_video_stream]->pix_fmt),
                        _ffmpeg->video_codec_ctxs[_video_stream]->width,
                        _ffmpeg->video_codec_ctxs[_video_stream]->height);
                src_frame = _ffmpeg->video_buffered_frames[_video_stream];
            }
            _frame.data[raw_frame][0] = src_frame->data[0];
            _frame.data[raw_frame][1] = src_frame->data[1];
            _frame.data[raw_frame][2] = src_frame->data[2];
            _frame.line_size[raw_frame][0] = src_frame->linesize[0];
            _frame.line_size[raw_frame][1] = src_frame->linesize[1];
            _frame.line_size[raw_frame][2] = src_frame->linesize[2];
        }

        if (_ffmpeg->video_packets[_video_stream].dts != static_cast<int64_t>(AV_NOPTS_VALUE))
        {
            _frame.presentation_time = handle_timestamp(_ffmpeg->video_packets[_video_stream].dts * 1000000
                    * _ffmpeg->format_ctx->streams[_ffmpeg->video_streams[_video_stream]]->time_base.num
                    / _ffmpeg->format_ctx->streams[_ffmpeg->video_streams[_video_stream]]->time_base.den);
        }
        else if (_ffmpeg->video_last_timestamps[_video_stream] != std::numeric_limits<int64_t>::min())
        {
            msg::dbg(_url + ": video stream " + str::from(_video_stream)
                    + ": no timestamp available, using a questionable guess");
            _frame.presentation_time = _ffmpeg->video_last_timestamps[_video_stream];
        }
        else
        {
            msg::dbg(_url + ": video stream " + str::from(_video_stream)
                    + ": no timestamp available, using a bad guess");
            _frame.presentation_time = _ffmpeg->pos;
        }
    }
}

void media_object::start_video_frame_read(int video_stream, int raw_frames)
{
    assert(video_stream >= 0);
    assert(video_stream < video_streams());
    assert(raw_frames == 1 || raw_frames == 2);
    _ffmpeg->video_decode_threads[video_stream].set_raw_frames(raw_frames);
    _ffmpeg->video_decode_threads[video_stream].start();
}

video_frame media_object::finish_video_frame_read(int video_stream)
{
    assert(video_stream >= 0);
    assert(video_stream < video_streams());
    _ffmpeg->video_decode_threads[video_stream].finish();
    return _ffmpeg->video_decode_threads[video_stream].frame();
}

audio_decode_thread::audio_decode_thread(const std::string &url, struct ffmpeg_stuff *ffmpeg, int audio_stream) :
    _url(url), _ffmpeg(ffmpeg), _audio_stream(audio_stream), _blob()
{
}

int64_t audio_decode_thread::handle_timestamp(int64_t timestamp)
{
    int64_t ts = timestamp_helper(_ffmpeg->audio_last_timestamps[_audio_stream], timestamp);
    _ffmpeg->pos = ts;
    return ts;
}

void audio_decode_thread::run()
{
    size_t size = _ffmpeg->audio_blobs[_audio_stream].size();
    void *buffer = _ffmpeg->audio_blobs[_audio_stream].ptr();
    int64_t timestamp = std::numeric_limits<int64_t>::min();
    size_t i = 0;
    while (i < size)
    {
        if (_ffmpeg->audio_buffers[_audio_stream].size() > 0)
        {
            // Use available decoded audio data
            size_t remaining = std::min(size - i, _ffmpeg->audio_buffers[_audio_stream].size());
            memcpy(buffer, &(_ffmpeg->audio_buffers[_audio_stream][0]), remaining);
            _ffmpeg->audio_buffers[_audio_stream].erase(
                    _ffmpeg->audio_buffers[_audio_stream].begin(),
                    _ffmpeg->audio_buffers[_audio_stream].begin() + remaining);
            buffer = reinterpret_cast<unsigned char *>(buffer) + remaining;
            i += remaining;
        }
        if (_ffmpeg->audio_buffers[_audio_stream].size() == 0)
        {
            // Read more audio data
            AVPacket packet, tmppacket;
            bool empty;
            do
            {
                _ffmpeg->audio_packet_queue_mutexes[_audio_stream].lock();
                empty = _ffmpeg->audio_packet_queues[_audio_stream].empty();
                _ffmpeg->audio_packet_queue_mutexes[_audio_stream].unlock();
                if (empty)
                {
                    if (_ffmpeg->reader->eof())
                    {
                        _blob = audio_blob();
                        return;
                    }
                    msg::dbg(_url + ": audio stream " + str::from(_audio_stream) + ": need to wait for packets...");
                    _ffmpeg->reader->start();
                    _ffmpeg->reader->finish();
                }
            }
            while (empty);
            _ffmpeg->audio_packet_queue_mutexes[_audio_stream].lock();
            packet = _ffmpeg->audio_packet_queues[_audio_stream].front();
            _ffmpeg->audio_packet_queues[_audio_stream].pop_front();
            _ffmpeg->audio_packet_queue_mutexes[_audio_stream].unlock();
            _ffmpeg->reader->start();   // Refill the packet queue
            if (timestamp == std::numeric_limits<int64_t>::min() && packet.dts != static_cast<int64_t>(AV_NOPTS_VALUE))
            {
                timestamp = packet.dts * 1000000
                    * _ffmpeg->format_ctx->streams[_ffmpeg->audio_streams[_audio_stream]]->time_base.num
                    / _ffmpeg->format_ctx->streams[_ffmpeg->audio_streams[_audio_stream]]->time_base.den;
            }

            // Decode audio data
            AVFrame audioframe = { { 0 } };
            tmppacket = packet;
            while (tmppacket.size > 0)
            {
                int got_frame = 0;
                int len = avcodec_decode_audio4(_ffmpeg->audio_codec_ctxs[_audio_stream],
                        &audioframe, &got_frame, &tmppacket);
                if (len < 0)
                {
                    tmppacket.size = 0;
                    break;
                }
                tmppacket.data += len;
                tmppacket.size -= len;
                if (!got_frame)
                {
                    continue;
                }
                int plane_size;
                int tmpbuf_size = av_samples_get_buffer_size(&plane_size,
                        _ffmpeg->audio_codec_ctxs[_audio_stream]->channels,
                        audioframe.nb_samples,
                        _ffmpeg->audio_codec_ctxs[_audio_stream]->sample_fmt, 1);
                if (av_sample_fmt_is_planar(_ffmpeg->audio_codec_ctxs[_audio_stream]->sample_fmt)
                        && _ffmpeg->audio_codec_ctxs[_audio_stream]->channels > 1)
                {
                    int dummy;
                    int sample_size = av_samples_get_buffer_size(&dummy, 1, 1,
                            _ffmpeg->audio_codec_ctxs[_audio_stream]->sample_fmt, 1);
                    uint8_t *out = reinterpret_cast<uint8_t *>(&(_ffmpeg->audio_tmpbufs[_audio_stream][0]));
                    for (int s = 0; s < audioframe.nb_samples; s++)
                    {
                        for (int c = 0; c < _ffmpeg->audio_codec_ctxs[_audio_stream]->channels; c++)
                        {
                            std::memcpy(out, audioframe.extended_data[c] + s * sample_size, sample_size);
                            out += sample_size;
                        }
                    }
                }
                else
                {
                    std::memcpy(&(_ffmpeg->audio_tmpbufs[_audio_stream][0]), audioframe.extended_data[0], plane_size);
                }
                // Put it in the decoded audio data buffer
                if (_ffmpeg->audio_codec_ctxs[_audio_stream]->sample_fmt == AV_SAMPLE_FMT_S32)
                {
                    // we need to convert this to AV_SAMPLE_FMT_FLT
                    assert(sizeof(int32_t) == sizeof(float));
                    assert(tmpbuf_size % sizeof(int32_t) == 0);
                    void *tmpbuf_v = static_cast<void *>(&(_ffmpeg->audio_tmpbufs[_audio_stream][0]));
                    int32_t *tmpbuf_i32 = static_cast<int32_t *>(tmpbuf_v);
                    float *tmpbuf_flt = static_cast<float *>(tmpbuf_v);
                    const float posdiv = +static_cast<float>(std::numeric_limits<int32_t>::max());
                    const float negdiv = -static_cast<float>(std::numeric_limits<int32_t>::min());
                    for (size_t j = 0; j < tmpbuf_size / sizeof(int32_t); j++)
                    {
                        int32_t sample_i32 = tmpbuf_i32[j];
                        float sample_flt = sample_i32 / (sample_i32 >= 0 ? posdiv : negdiv);
                        tmpbuf_flt[j] = sample_flt;
                    }
                }
                size_t old_size = _ffmpeg->audio_buffers[_audio_stream].size();
                _ffmpeg->audio_buffers[_audio_stream].resize(old_size + tmpbuf_size);
                memcpy(&(_ffmpeg->audio_buffers[_audio_stream][old_size]), _ffmpeg->audio_tmpbufs[_audio_stream], tmpbuf_size);
            }

            av_free_packet(&packet);
        }
    }
    if (timestamp == std::numeric_limits<int64_t>::min())
    {
        timestamp = _ffmpeg->audio_last_timestamps[_audio_stream];
    }
    if (timestamp == std::numeric_limits<int64_t>::min())
    {
        msg::dbg(_url + ": audio stream " + str::from(_audio_stream)
                + ": no timestamp available, using a bad guess");
        timestamp = _ffmpeg->pos;
    }

    _blob = _ffmpeg->audio_blob_templates[_audio_stream];
    _blob.data = _ffmpeg->audio_blobs[_audio_stream].ptr();
    _blob.size = _ffmpeg->audio_blobs[_audio_stream].size();
    _blob.presentation_time = handle_timestamp(timestamp);
}

void media_object::start_audio_blob_read(int audio_stream, size_t size)
{
    assert(audio_stream >= 0);
    assert(audio_stream < audio_streams());
    _ffmpeg->audio_blobs[audio_stream].resize(size);
    _ffmpeg->audio_decode_threads[audio_stream].start();
}

audio_blob media_object::finish_audio_blob_read(int audio_stream)
{
    assert(audio_stream >= 0);
    assert(audio_stream < audio_streams());
    _ffmpeg->audio_decode_threads[audio_stream].finish();
    return _ffmpeg->audio_decode_threads[audio_stream].blob();
}

subtitle_decode_thread::subtitle_decode_thread(const std::string &url, struct ffmpeg_stuff *ffmpeg, int subtitle_stream) :
    _url(url), _ffmpeg(ffmpeg), _subtitle_stream(subtitle_stream), _box()
{
}

int64_t subtitle_decode_thread::handle_timestamp(int64_t timestamp)
{
    int64_t ts = timestamp_helper(_ffmpeg->subtitle_last_timestamps[_subtitle_stream], timestamp);
    _ffmpeg->pos = ts;
    return ts;
}

void subtitle_decode_thread::run()
{
    if (_ffmpeg->subtitle_box_buffers[_subtitle_stream].empty())
    {
        // Read more subtitle data
        AVPacket packet, tmppacket;
        bool empty;
        do
        {
            _ffmpeg->subtitle_packet_queue_mutexes[_subtitle_stream].lock();
            empty = _ffmpeg->subtitle_packet_queues[_subtitle_stream].empty();
            _ffmpeg->subtitle_packet_queue_mutexes[_subtitle_stream].unlock();
            if (empty)
            {
                if (_ffmpeg->reader->eof())
                {
                    _box = subtitle_box();
                    return;
                }
                msg::dbg(_url + ": subtitle stream " + str::from(_subtitle_stream) + ": need to wait for packets...");
                _ffmpeg->reader->start();
                _ffmpeg->reader->finish();
            }
        }
        while (empty);
        _ffmpeg->subtitle_packet_queue_mutexes[_subtitle_stream].lock();
        packet = _ffmpeg->subtitle_packet_queues[_subtitle_stream].front();
        _ffmpeg->subtitle_packet_queues[_subtitle_stream].pop_front();
        _ffmpeg->subtitle_packet_queue_mutexes[_subtitle_stream].unlock();
        _ffmpeg->reader->start();   // Refill the packet queue

        // Decode subtitle data
        int64_t timestamp = packet.pts * 1000000
            * _ffmpeg->format_ctx->streams[_ffmpeg->subtitle_streams[_subtitle_stream]]->time_base.num
            / _ffmpeg->format_ctx->streams[_ffmpeg->subtitle_streams[_subtitle_stream]]->time_base.den;
        AVSubtitle subtitle;
        int got_subtitle;
        tmppacket = packet;

        // CODEC_ID_TEXT does not have any decoder; it is just UTF-8 text in the packet data.
        if (_ffmpeg->subtitle_codec_ctxs[_subtitle_stream]->codec_id == CODEC_ID_TEXT)
        {
            int64_t duration = packet.convergence_duration * 1000000
                * _ffmpeg->format_ctx->streams[_ffmpeg->subtitle_streams[_subtitle_stream]]->time_base.num
                / _ffmpeg->format_ctx->streams[_ffmpeg->subtitle_streams[_subtitle_stream]]->time_base.den;

            // Put it in the subtitle buffer
            subtitle_box box = _ffmpeg->subtitle_box_templates[_subtitle_stream];
            box.presentation_start_time = timestamp;
            box.presentation_stop_time = timestamp + duration;

            box.format = subtitle_box::text;
            box.str = reinterpret_cast<const char *>(packet.data);

            _ffmpeg->subtitle_box_buffers[_subtitle_stream].push_back(box);

            tmppacket.size = 0;
        }

        while (tmppacket.size > 0)
        {
            int len = avcodec_decode_subtitle2(_ffmpeg->subtitle_codec_ctxs[_subtitle_stream],
                    &subtitle, &got_subtitle, &tmppacket);
            if (len < 0)
            {
                tmppacket.size = 0;
                break;
            }
            tmppacket.data += len;
            tmppacket.size -= len;
            if (!got_subtitle)
            {
                continue;
            }
            // Put it in the subtitle buffer
            subtitle_box box = _ffmpeg->subtitle_box_templates[_subtitle_stream];
            box.presentation_start_time = timestamp + subtitle.start_display_time * 1000;
            box.presentation_stop_time = box.presentation_start_time + subtitle.end_display_time * 1000;
            for (unsigned int i = 0; i < subtitle.num_rects; i++)
            {
                AVSubtitleRect *rect = subtitle.rects[i];
                switch (rect->type)
                {
                case SUBTITLE_BITMAP:
                    box.format = subtitle_box::image;
                    box.images.push_back(subtitle_box::image_t());
                    box.images.back().w = rect->w;
                    box.images.back().h = rect->h;
                    box.images.back().x = rect->x;
                    box.images.back().y = rect->y;
                    box.images.back().palette.resize(4 * rect->nb_colors);
                    std::memcpy(&(box.images.back().palette[0]), rect->pict.data[1],
                            box.images.back().palette.size());
                    box.images.back().linesize = rect->pict.linesize[0];
                    box.images.back().data.resize(box.images.back().h * box.images.back().linesize);
                    std::memcpy(&(box.images.back().data[0]), rect->pict.data[0],
                            box.images.back().data.size() * sizeof(uint8_t));
                    break;
                case SUBTITLE_TEXT:
                    box.format = subtitle_box::text;
                    if (!box.str.empty())
                    {
                        box.str += '\n';
                    }
                    box.str += rect->text;
                    break;
                case SUBTITLE_ASS:
                    box.format = subtitle_box::ass;
                    box.style = std::string(reinterpret_cast<const char *>(
                                _ffmpeg->subtitle_codec_ctxs[_subtitle_stream]->subtitle_header),
                            _ffmpeg->subtitle_codec_ctxs[_subtitle_stream]->subtitle_header_size);
                    if (!box.str.empty())
                    {
                        box.str += '\n';
                    }
                    box.str += rect->ass;
                    break;
                case SUBTITLE_NONE:
                    // Should never happen, but make sure we have a valid subtitle box anyway.
                    box.format = subtitle_box::text;
                    box.str = ' ';
                    break;
                }
            }
            _ffmpeg->subtitle_box_buffers[_subtitle_stream].push_back(box);
            avsubtitle_free(&subtitle);
        }

        av_free_packet(&packet);
    }
    if (_ffmpeg->subtitle_box_buffers[_subtitle_stream].empty())
    {
        _box = subtitle_box();
        return;
    }
    _box = _ffmpeg->subtitle_box_buffers[_subtitle_stream].front();
    _ffmpeg->subtitle_box_buffers[_subtitle_stream].pop_front();
}

void media_object::start_subtitle_box_read(int subtitle_stream)
{
    assert(subtitle_stream >= 0);
    assert(subtitle_stream < subtitle_streams());
    _ffmpeg->subtitle_decode_threads[subtitle_stream].start();
}

subtitle_box media_object::finish_subtitle_box_read(int subtitle_stream)
{
    assert(subtitle_stream >= 0);
    assert(subtitle_stream < subtitle_streams());
    _ffmpeg->subtitle_decode_threads[subtitle_stream].finish();
    return _ffmpeg->subtitle_decode_threads[subtitle_stream].box();
}

int64_t media_object::tell()
{
    return _ffmpeg->pos;
}

void media_object::seek(int64_t dest_pos)
{
    msg::dbg(_url + ": Seeking from " + str::from(_ffmpeg->pos / 1e6f) + " to " + str::from(dest_pos / 1e6f) + ".");

    // Stop decoder threads
    for (size_t i = 0; i < _ffmpeg->video_streams.size(); i++)
    {
        _ffmpeg->video_decode_threads[i].finish();
    }
    for (size_t i = 0; i < _ffmpeg->audio_streams.size(); i++)
    {
        _ffmpeg->audio_decode_threads[i].finish();
    }
    for (size_t i = 0; i < _ffmpeg->subtitle_streams.size(); i++)
    {
        _ffmpeg->subtitle_decode_threads[i].finish();
    }
    // Stop reading packets
    _ffmpeg->reader->finish();
    // Throw away all queued packets and buffered data
    for (size_t i = 0; i < _ffmpeg->video_streams.size(); i++)
    {
        avcodec_flush_buffers(_ffmpeg->format_ctx->streams[_ffmpeg->video_streams[i]]->codec);
        for (size_t j = 0; j < _ffmpeg->video_packet_queues[i].size(); j++)
        {
            av_free_packet(&_ffmpeg->video_packet_queues[i][j]);
        }
        _ffmpeg->video_packet_queues[i].clear();
    }
    for (size_t i = 0; i < _ffmpeg->audio_streams.size(); i++)
    {
        avcodec_flush_buffers(_ffmpeg->format_ctx->streams[_ffmpeg->audio_streams[i]]->codec);
        _ffmpeg->audio_buffers[i].clear();
        for (size_t j = 0; j < _ffmpeg->audio_packet_queues[i].size(); j++)
        {
            av_free_packet(&_ffmpeg->audio_packet_queues[i][j]);
        }
        _ffmpeg->audio_packet_queues[i].clear();
    }
    for (size_t i = 0; i < _ffmpeg->subtitle_streams.size(); i++)
    {
        if (_ffmpeg->format_ctx->streams[_ffmpeg->subtitle_streams[i]]->codec->codec_id != CODEC_ID_TEXT)
        {
            // CODEC_ID_TEXT has no decoder, so we cannot flush its buffers
            avcodec_flush_buffers(_ffmpeg->format_ctx->streams[_ffmpeg->subtitle_streams[i]]->codec);
        }
        _ffmpeg->subtitle_box_buffers[i].clear();
        for (size_t j = 0; j < _ffmpeg->subtitle_packet_queues[i].size(); j++)
        {
            av_free_packet(&_ffmpeg->subtitle_packet_queues[i][j]);
        }
        _ffmpeg->subtitle_packet_queues[i].clear();
    }
    // The next read request must update the position
    for (size_t i = 0; i < _ffmpeg->video_streams.size(); i++)
    {
        _ffmpeg->video_last_timestamps[i] = std::numeric_limits<int64_t>::min();
    }
    for (size_t i = 0; i < _ffmpeg->audio_streams.size(); i++)
    {
        _ffmpeg->audio_last_timestamps[i] = std::numeric_limits<int64_t>::min();
    }
    for (size_t i = 0; i < _ffmpeg->subtitle_streams.size(); i++)
    {
        _ffmpeg->subtitle_last_timestamps[i] = std::numeric_limits<int64_t>::min();
    }
    _ffmpeg->pos = std::numeric_limits<int64_t>::min();
    // Seek
    int e = av_seek_frame(_ffmpeg->format_ctx, -1,
            dest_pos * AV_TIME_BASE / 1000000,
            dest_pos < _ffmpeg->pos ?  AVSEEK_FLAG_BACKWARD : 0);
    if (e < 0)
    {
        msg::err(_("%s: Seeking failed."), _url.c_str());
    }
    // Restart packet reading
    _ffmpeg->reader->reset();
    _ffmpeg->reader->start();
}

void media_object::close()
{
    if (_ffmpeg)
    {
        try
        {
            // Stop decoder threads
            for (size_t i = 0; i < _ffmpeg->video_decode_threads.size(); i++)
            {
                _ffmpeg->video_decode_threads[i].finish();
            }
            for (size_t i = 0; i < _ffmpeg->audio_decode_threads.size(); i++)
            {
                _ffmpeg->audio_decode_threads[i].finish();
            }
            for (size_t i = 0; i < _ffmpeg->subtitle_decode_threads.size(); i++)
            {
                _ffmpeg->subtitle_decode_threads[i].finish();
            }
            // Stop reading packets
            _ffmpeg->reader->finish();
        }
        catch (...)
        {
        }
        if (_ffmpeg->format_ctx)
        {
            for (size_t i = 0; i < _ffmpeg->video_frames.size(); i++)
            {
                av_free(_ffmpeg->video_frames[i]);
            }
            for (size_t i = 0; i < _ffmpeg->video_buffered_frames.size(); i++)
            {
                av_free(_ffmpeg->video_buffered_frames[i]);
            }
            for (size_t i = 0; i < _ffmpeg->video_buffers.size(); i++)
            {
                av_free(_ffmpeg->video_buffers[i]);
            }
            for (size_t i = 0; i < _ffmpeg->video_sws_frames.size(); i++)
            {
                av_free(_ffmpeg->video_sws_frames[i]);
            }
            for (size_t i = 0; i < _ffmpeg->video_sws_buffers.size(); i++)
            {
                av_free(_ffmpeg->video_sws_buffers[i]);
            }
            for (size_t i = 0; i < _ffmpeg->video_codec_ctxs.size(); i++)
            {
                if (i < _ffmpeg->video_codecs.size() && _ffmpeg->video_codecs[i])
                {
                    avcodec_close(_ffmpeg->video_codec_ctxs[i]);
                }
            }
            for (size_t i = 0; i < _ffmpeg->video_sws_ctxs.size(); i++)
            {
                sws_freeContext(_ffmpeg->video_sws_ctxs[i]);
            }
            for (size_t i = 0; i < _ffmpeg->video_packet_queues.size(); i++)
            {
                if (_ffmpeg->video_packet_queues[i].size() > 0)
                {
                    msg::dbg(_url + ": " + str::from(_ffmpeg->video_packet_queues[i].size())
                            + " unprocessed packets in video stream " + str::from(i));
                }
                for (size_t j = 0; j < _ffmpeg->video_packet_queues[i].size(); j++)
                {
                    av_free_packet(&_ffmpeg->video_packet_queues[i][j]);
                }
            }
            for (size_t i = 0; i < _ffmpeg->video_packets.size(); i++)
            {
                av_free_packet(&(_ffmpeg->video_packets[i]));
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
                            + " unprocessed packets in audio stream " + str::from(i));
                }
                for (size_t j = 0; j < _ffmpeg->audio_packet_queues[i].size(); j++)
                {
                    av_free_packet(&_ffmpeg->audio_packet_queues[i][j]);
                }
            }
            for (size_t i = 0; i < _ffmpeg->audio_tmpbufs.size(); i++)
            {
                av_free(_ffmpeg->audio_tmpbufs[i]);
            }
            for (size_t i = 0; i < _ffmpeg->subtitle_codec_ctxs.size(); i++)
            {
                if (i < _ffmpeg->subtitle_codecs.size() && _ffmpeg->subtitle_codecs[i])
                {
                    avcodec_close(_ffmpeg->subtitle_codec_ctxs[i]);
                }
            }
            for (size_t i = 0; i < _ffmpeg->subtitle_packet_queues.size(); i++)
            {
                if (_ffmpeg->subtitle_packet_queues[i].size() > 0)
                {
                    msg::dbg(_url + ": " + str::from(_ffmpeg->subtitle_packet_queues[i].size())
                            + " unprocessed packets in subtitle stream " + str::from(i));
                }
                for (size_t j = 0; j < _ffmpeg->subtitle_packet_queues[i].size(); j++)
                {
                    av_free_packet(&_ffmpeg->subtitle_packet_queues[i][j]);
                }
            }
            avformat_close_input(&_ffmpeg->format_ctx);
        }
        delete _ffmpeg->reader;
        delete _ffmpeg;
        _ffmpeg = NULL;
    }
    _url = "";
    _is_device = false;
    _tag_names.clear();
    _tag_values.clear();
}
