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

#include <vector>
#include <deque>
#include <limits>
#include <cerrno>
#include <cstring>

#if HAVE_SYSCONF
#  include <unistd.h>
#else
#  include <windows.h>
#endif

#include "debug.h"
#include "blob.h"
#include "exc.h"
#include "msg.h"
#include "str.h"

#include "decoder_ffmpeg.h"


/* Hide the FFmpeg stuff so that their header files cannot cause problems
 * in other source files. */
struct internal_stuff
{
    AVFormatContext *format_ctx;
    int64_t pos;

    std::vector<int> video_streams;
    std::vector<AVCodecContext *> video_codec_ctxs;
    std::vector<struct SwsContext *> img_conv_ctxs;
    std::vector<AVCodec *> video_codecs;
    std::vector<std::deque<AVPacket> > video_packet_queues;
    std::vector<AVPacket> packets;
    std::vector<bool> video_flush_flags;
    std::vector<AVFrame *> frames;
    std::vector<AVFrame *> out_frames;
    std::vector<uint8_t *> buffers;
    std::vector<int64_t> video_pos_offsets;

    std::vector<int> audio_streams;
    std::vector<AVCodecContext *> audio_codec_ctxs;
    std::vector<enum decoder::audio_sample_format> audio_sample_formats;
    std::vector<AVCodec *> audio_codecs;
    std::vector<std::deque<AVPacket> > audio_packet_queues;
    std::vector<bool> audio_flush_flags;
    std::vector<std::vector<unsigned char> > audio_buffers;
    std::vector<int64_t> audio_last_timestamps;
    std::vector<int64_t> audio_pos_offsets;
};

static std::string my_av_strerror(int err)
{
    blob b(1024);
    av_strerror(err, b.ptr<char>(), b.size());
    return std::string(b.ptr<const char>());
}

static int decoding_threads()
{
    // Use one decoding thread per processor.
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
        else if (n > 64)
        {
            n = 64;
        }
    }
    return n;
}

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
        line.resize(0);
    }
}

decoder_ffmpeg::decoder_ffmpeg() throw ()
    : decoder(), _stuff(NULL)
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

decoder_ffmpeg::~decoder_ffmpeg()
{
    if (_stuff)
    {
        close();
    }
}

void decoder_ffmpeg::open(const std::string &filename)
{
    int e;

    _filename = filename;
    _stuff = new stuff_t;

    if ((e = av_open_input_file(&_stuff->format_ctx, filename.c_str(), NULL, 0, NULL)) != 0)
    {
        throw exc(filename + ": " + my_av_strerror(e));
    }
    if ((e = av_find_stream_info(_stuff->format_ctx)) < 0)
    {
        throw exc(filename + ": cannot read stream info: " + my_av_strerror(e));
    }
    dump_format(_stuff->format_ctx, 0, filename.c_str(), 0);
    _stuff->pos = std::numeric_limits<int64_t>::min();

    for (unsigned int i = 0; i < _stuff->format_ctx->nb_streams
            && i < static_cast<unsigned int>(std::numeric_limits<int>::max()); i++)
    {
        _stuff->format_ctx->streams[i]->discard = AVDISCARD_ALL;        // ignore by default; user must activate streams
        if (_stuff->format_ctx->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
        {
            _stuff->video_streams.push_back(i);
            int j = _stuff->video_streams.size() - 1;
            msg::dbg(filename + " stream " + str::from(i) + " is video stream " + str::from(j));
            _stuff->video_codec_ctxs.push_back(_stuff->format_ctx->streams[i]->codec);
            if (_stuff->video_codec_ctxs[j]->width < 1 || _stuff->video_codec_ctxs[j]->height < 1)
            {
                throw exc(filename + " stream " + str::from(i) + ": invalid frame size");
            }
            _stuff->video_codec_ctxs[j]->thread_count = decoding_threads();
            if (avcodec_thread_init(_stuff->video_codec_ctxs[j], _stuff->video_codec_ctxs[j]->thread_count) != 0)
            {
                _stuff->video_codec_ctxs[j]->thread_count = 1;
            }
            _stuff->video_codecs.push_back(avcodec_find_decoder(_stuff->video_codec_ctxs[j]->codec_id));
            if (!_stuff->video_codecs[j])
            {
                throw exc(filename + " stream " + str::from(i) + ": unsupported video codec");
            }
            if ((e = avcodec_open(_stuff->video_codec_ctxs[j], _stuff->video_codecs[j])) < 0)
            {
                _stuff->video_codecs[j] = NULL;
                throw exc(filename + " stream " + str::from(i) + ": cannot open video codec: " + my_av_strerror(e));
            }
            int bufsize = avpicture_get_size(PIX_FMT_BGRA,
                    _stuff->video_codec_ctxs[j]->width, _stuff->video_codec_ctxs[j]->height);
            _stuff->packets.push_back(AVPacket());
            _stuff->video_flush_flags.push_back(false);
            _stuff->frames.push_back(avcodec_alloc_frame());
            _stuff->out_frames.push_back(avcodec_alloc_frame());
            _stuff->buffers.push_back(static_cast<uint8_t *>(av_malloc(bufsize)));
            if (!_stuff->frames[j] || !_stuff->out_frames[j] || !_stuff->buffers[j])
            {
                throw exc(HERE + ": " + strerror(ENOMEM));
            }
            avpicture_fill(reinterpret_cast<AVPicture *>(_stuff->out_frames[j]), _stuff->buffers[j],
                    PIX_FMT_BGRA, _stuff->video_codec_ctxs[j]->width, _stuff->video_codec_ctxs[j]->height);
            _stuff->img_conv_ctxs.push_back(sws_getContext(
                        _stuff->video_codec_ctxs[j]->width, _stuff->video_codec_ctxs[j]->height, _stuff->video_codec_ctxs[j]->pix_fmt,
                        _stuff->video_codec_ctxs[j]->width, _stuff->video_codec_ctxs[j]->height, PIX_FMT_BGRA,
                        SWS_FAST_BILINEAR, NULL, NULL, NULL));
            if (!_stuff->img_conv_ctxs[j])
            {
                throw exc(filename + " stream " + str::from(i) + ": cannot initialize conversion context");
            }
            _stuff->video_pos_offsets.push_back(std::numeric_limits<int64_t>::min());
        }
        else if (_stuff->format_ctx->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO)
        {
            _stuff->audio_streams.push_back(i);
            int j = _stuff->audio_streams.size() - 1;
            msg::dbg(filename + " stream " + str::from(i) + " is audio stream " + str::from(j));
            _stuff->audio_codec_ctxs.push_back(_stuff->format_ctx->streams[i]->codec);
            _stuff->audio_codecs.push_back(avcodec_find_decoder(_stuff->audio_codec_ctxs[j]->codec_id));
            if (!_stuff->audio_codecs[j])
            {
                throw exc(filename + " stream " + str::from(i) + ": unsupported audio codec");
            }
            if ((e = avcodec_open(_stuff->audio_codec_ctxs[j], _stuff->audio_codecs[j])) < 0)
            {
                _stuff->audio_codecs[j] = NULL;
                throw exc(filename + " stream " + str::from(i) + ": cannot open audio codec: " + my_av_strerror(e));
            }
            if (_stuff->audio_codec_ctxs[j]->sample_fmt == SAMPLE_FMT_U8)
            {
                _stuff->audio_sample_formats.push_back(audio_sample_u8);
            }
            else if (_stuff->audio_codec_ctxs[j]->sample_fmt == SAMPLE_FMT_S16)
            {
                _stuff->audio_sample_formats.push_back(audio_sample_s16);
            }
            else if (_stuff->audio_codec_ctxs[j]->sample_fmt == SAMPLE_FMT_FLT)
            {
                _stuff->audio_sample_formats.push_back(audio_sample_f32);
            }
            else if (_stuff->audio_codec_ctxs[j]->sample_fmt == SAMPLE_FMT_DBL)
            {
                _stuff->audio_sample_formats.push_back(audio_sample_d64);
            }
            else
            {
                throw exc(filename + " stream " + str::from(i)
                        + ": cannot handle audio sample format "
                        + str::from(static_cast<int>(_stuff->audio_codec_ctxs[j]->sample_fmt)));
                // XXX: Once widely available, use av_get_sample_fmt_name(_stuff->audio_codec_ctxs[j]->sample_fmt)
                // in this error message.
            }
            if (_stuff->audio_codec_ctxs[j]->channels < 1
                    || _stuff->audio_codec_ctxs[j]->channels > 8
                    || _stuff->audio_codec_ctxs[j]->channels == 3
                    || _stuff->audio_codec_ctxs[j]->channels == 5)
            {
                throw exc(filename + " stream " + str::from(i) + ": cannot handle audio with "
                        + str::from(_stuff->audio_codec_ctxs[j]->channels) + " channels");
            }
            _stuff->audio_flush_flags.push_back(false);
            _stuff->audio_buffers.push_back(std::vector<unsigned char>());
            _stuff->audio_last_timestamps.push_back(std::numeric_limits<int64_t>::min());
            _stuff->audio_pos_offsets.push_back(std::numeric_limits<int64_t>::min());
        }
        else
        {
            msg::dbg(filename + " stream " + str::from(i) + " contains neither video nor audio");
        }
    }
    _stuff->video_packet_queues.resize(video_streams());
    _stuff->audio_packet_queues.resize(audio_streams());

    /* Metadata */
    AVMetadataTag *tag = NULL;
    while ((tag = av_metadata_get(_stuff->format_ctx->metadata, "", tag, AV_METADATA_IGNORE_SUFFIX)))
    {
        _tag_names.push_back(tag->key);
        _tag_values.push_back(tag->value);
    }

    msg::inf(filename + ":");
    for (int i = 0; i < video_streams(); i++)
    {
        msg::inf("    video stream %d: %dx%d, format %s,",
                i, video_width(i), video_height(i),
                _stuff->video_codec_ctxs.at(i)->pix_fmt == PIX_FMT_YUV420P
                ? decoder::video_frame_format_name(decoder::frame_format_yuv420p).c_str()
                : str::asprintf("%d (converted to %s)", _stuff->video_codec_ctxs.at(i)->pix_fmt,
                    decoder::video_frame_format_name(decoder::frame_format_bgra32).c_str()).c_str());
        msg::inf("        aspect ratio %g:1, %g fps, %g seconds",
                static_cast<float>(video_aspect_ratio_numerator(i))
                / static_cast<float>(video_aspect_ratio_denominator(i)),
                static_cast<float>(video_frame_rate_numerator(i))
                / static_cast<float>(video_frame_rate_denominator(i)),
                video_duration(i) / 1e6f);
        msg::inf("        using up to %d threads for decoding", _stuff->video_codec_ctxs.at(i)->thread_count);
    }
    for (int i = 0; i < audio_streams(); i++)
    {
        msg::inf("    audio stream %d: %d channels, %d Hz, sample format %s", i,
                audio_channels(i), audio_rate(i), audio_sample_format_name(audio_sample_format(i)).c_str());
    }
    if (video_streams() == 0 && audio_streams() == 0)
    {
        msg::inf("    no usable streams");
    }
}

int decoder_ffmpeg::audio_streams() const throw ()
{
    return _stuff->audio_streams.size();
}

int decoder_ffmpeg::video_streams() const throw ()
{
    return _stuff->video_streams.size();
}

void decoder_ffmpeg::activate_video_stream(int index)
{
    _stuff->format_ctx->streams[_stuff->video_streams.at(index)]->discard = AVDISCARD_DEFAULT;
}

void decoder_ffmpeg::activate_audio_stream(int index)
{
    _stuff->format_ctx->streams[_stuff->audio_streams.at(index)]->discard = AVDISCARD_DEFAULT;
}

int decoder_ffmpeg::video_width(int index) const throw ()
{
    return _stuff->video_codec_ctxs.at(index)->width;
}

int decoder_ffmpeg::video_height(int index) const throw ()
{
    return _stuff->video_codec_ctxs.at(index)->height;
}

int decoder_ffmpeg::video_aspect_ratio_numerator(int index) const throw ()
{
    int n = 1;
    int sn = _stuff->format_ctx->streams[_stuff->video_streams.at(index)]->sample_aspect_ratio.num;
    int cn = _stuff->video_codec_ctxs.at(index)->sample_aspect_ratio.num;
    if (sn > 0)
    {
        n = sn;
    }
    else if (cn > 0)
    {
        n = cn;
    }
    return n * video_width(index);
}

int decoder_ffmpeg::video_aspect_ratio_denominator(int index) const throw ()
{
    int d = 1;
    int sn = _stuff->format_ctx->streams[_stuff->video_streams.at(index)]->sample_aspect_ratio.num;
    int sd = _stuff->format_ctx->streams[_stuff->video_streams.at(index)]->sample_aspect_ratio.den;
    int cn = _stuff->video_codec_ctxs.at(index)->sample_aspect_ratio.num;
    int cd = _stuff->video_codec_ctxs.at(index)->sample_aspect_ratio.den;
    if (sn > 0 && sd > 0)
    {
        d = sd;
    }
    else if (cn > 0 && cd > 0)
    {
        d = cd;
    }
    return d * video_height(index);
}

int decoder_ffmpeg::video_frame_rate_numerator(int index) const throw ()
{
    return _stuff->format_ctx->streams[_stuff->video_streams.at(index)]->r_frame_rate.num;
}

int decoder_ffmpeg::video_frame_rate_denominator(int index) const throw ()
{
    return _stuff->format_ctx->streams[_stuff->video_streams.at(index)]->r_frame_rate.den;
}

int64_t decoder_ffmpeg::video_duration(int index) const throw ()
{
    int64_t duration = _stuff->format_ctx->streams[_stuff->video_streams.at(index)]->duration;
    AVRational time_base = _stuff->format_ctx->streams[_stuff->video_streams.at(index)]->time_base;
    return duration * 1000000 * time_base.num / time_base.den;
}

enum decoder::video_frame_format decoder_ffmpeg::video_frame_format(int index) const throw ()
{
    return (_stuff->video_codec_ctxs.at(index)->pix_fmt == PIX_FMT_YUV420P
            ? decoder::frame_format_yuv420p
            : decoder::frame_format_bgra32);
}

int decoder_ffmpeg::audio_rate(int index) const throw ()
{
    return _stuff->audio_codec_ctxs.at(index)->sample_rate;
}

int decoder_ffmpeg::audio_channels(int index) const throw ()
{
    return _stuff->audio_codec_ctxs.at(index)->channels;
}

enum decoder::audio_sample_format decoder_ffmpeg::audio_sample_format(int index) const throw ()
{
    return _stuff->audio_sample_formats.at(index);
}

int64_t decoder_ffmpeg::audio_duration(int index) const throw ()
{
    int64_t duration = _stuff->format_ctx->streams[_stuff->audio_streams.at(index)]->duration;
    AVRational time_base = _stuff->format_ctx->streams[_stuff->audio_streams.at(index)]->time_base;
    return duration * 1000000 * time_base.num / time_base.den;
}

bool decoder_ffmpeg::read()
{
    msg::dbg(_filename + ": reading a packet");

    AVPacket packet;
    int e;
    if ((e = av_read_frame(_stuff->format_ctx, &packet)) < 0)
    {
        if (e == AVERROR_EOF)
        {
            msg::dbg(_filename + ": EOF");
            return false;
        }
        else
        {
            throw exc(_filename + ": " + my_av_strerror(e));
        }
    }
    bool packet_queued = false;
    for (size_t i = 0; i < _stuff->video_streams.size() && !packet_queued; i++)
    {
        if (packet.stream_index == _stuff->video_streams[i])
        {
            if (av_dup_packet(&packet) < 0)
            {
                msg::dbg(_filename + ": cannot duplicate packet");
                return false;
            }
            _stuff->video_packet_queues[i].push_back(packet);
            msg::dbg(_filename + ": "
                    + str::from(_stuff->video_packet_queues[i].size())
                    + " packets queued in video stream " + str::from(i));
            packet_queued = true;
        }
    }
    for (size_t i = 0; i < _stuff->audio_streams.size() && !packet_queued; i++)
    {
        if (packet.stream_index == _stuff->audio_streams[i])
        {
            if (av_dup_packet(&packet) < 0)
            {
                msg::dbg(_filename + ": cannot duplicate packet");
                return false;
            }
            _stuff->audio_packet_queues[i].push_back(packet);
            msg::dbg(_filename + ": "
                    + str::from(_stuff->audio_packet_queues[i].size())
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

int64_t decoder_ffmpeg::read_video_frame(int video_stream)
{
    if (_stuff->video_flush_flags[video_stream])
    {
        avcodec_flush_buffers(_stuff->format_ctx->streams[_stuff->video_streams[video_stream]]->codec);
        for (size_t j = 0; j < _stuff->video_packet_queues[video_stream].size(); j++)
        {
            av_free_packet(&_stuff->video_packet_queues[video_stream][j]);
        }
        _stuff->video_packet_queues[video_stream].resize(0);
        _stuff->video_flush_flags[video_stream] = false;
    }
    int frame_finished = 0;
    do
    {
        while (_stuff->video_packet_queues[video_stream].empty())
        {
            if (!read())
            {
                return -1;
            }
        }
        _stuff->packets[video_stream] = _stuff->video_packet_queues[video_stream].front();
        _stuff->video_packet_queues[video_stream].pop_front();
        avcodec_decode_video2(_stuff->video_codec_ctxs[video_stream],
                _stuff->frames[video_stream], &frame_finished,
                &(_stuff->packets[video_stream]));
        av_free_packet(&(_stuff->packets[video_stream]));
    }
    while (!frame_finished);

    int64_t timestamp = _stuff->packets[video_stream].dts * 1000000
        * _stuff->format_ctx->streams[_stuff->video_streams[video_stream]]->time_base.num 
        / _stuff->format_ctx->streams[_stuff->video_streams[video_stream]]->time_base.den;
    if (_stuff->video_pos_offsets[video_stream] == std::numeric_limits<int64_t>::min())
    {
        _stuff->video_pos_offsets[video_stream] = timestamp;
    }
    timestamp -= _stuff->video_pos_offsets[video_stream];
    if (timestamp > _stuff->pos)
    {
        _stuff->pos = timestamp;
    }
    return timestamp;
}

void decoder_ffmpeg::release_video_frame(int /* video_stream */)
{
    // TODO
    // This used to free the packet, but this is now done directly after decoding
    // in read_video_frame. Nevertheless, we keep this function for now, until it
    // is clear that we will never need it.
}

void decoder_ffmpeg::get_video_frame(int video_stream, enum video_frame_format fmt,
            uint8_t *data[3], size_t line_size[3])
{
    data[0] = NULL;
    data[1] = NULL;
    data[2] = NULL;
    line_size[0] = 0;
    line_size[1] = 0;
    line_size[2] = 0;
    if (fmt == decoder::frame_format_yuv420p)
    {
        if (video_frame_format(video_stream) == decoder::frame_format_yuv420p)
        {
            data[0] = _stuff->frames[video_stream]->data[0];
            data[1] = _stuff->frames[video_stream]->data[1];
            data[2] = _stuff->frames[video_stream]->data[2];
            line_size[0] = _stuff->frames[video_stream]->linesize[0];
            line_size[1] = _stuff->frames[video_stream]->linesize[1];
            line_size[2] = _stuff->frames[video_stream]->linesize[2];
        }
    }
    else
    {
        sws_scale(_stuff->img_conv_ctxs[video_stream],
                _stuff->frames[video_stream]->data,
                _stuff->frames[video_stream]->linesize,
                0, video_height(video_stream),
                _stuff->out_frames[video_stream]->data,
                _stuff->out_frames[video_stream]->linesize);
        // TODO: Handle sws_scale errors. How?
        data[0] = _stuff->out_frames[video_stream]->data[0];
        line_size[0] = _stuff->out_frames[video_stream]->linesize[0];
    }
}

int64_t decoder_ffmpeg::read_audio_data(int audio_stream, void *buffer, size_t size)
{
    if (_stuff->audio_flush_flags[audio_stream])
    {
        avcodec_flush_buffers(_stuff->format_ctx->streams[_stuff->audio_streams[audio_stream]]->codec);
        _stuff->audio_buffers[audio_stream].resize(0);
        for (size_t j = 0; j < _stuff->audio_packet_queues[audio_stream].size(); j++)
        {
            av_free_packet(&_stuff->audio_packet_queues[audio_stream][j]);
        }
        _stuff->audio_packet_queues[audio_stream].resize(0);
        _stuff->audio_flush_flags[audio_stream] = false;
    }

    memset(buffer, 0, size);

    int64_t timestamp = std::numeric_limits<int64_t>::min();
    size_t i = 0;
    while (i < size)
    {
        if (_stuff->audio_buffers[audio_stream].size() > 0)
        {
            // Use available decoded audio data
            size_t remaining = std::min(size - i, _stuff->audio_buffers[audio_stream].size());
            memcpy(buffer, &(_stuff->audio_buffers[audio_stream][0]), remaining);
            _stuff->audio_buffers[audio_stream].erase(
                    _stuff->audio_buffers[audio_stream].begin(),
                    _stuff->audio_buffers[audio_stream].begin() + remaining);
            buffer = reinterpret_cast<unsigned char *>(buffer) + remaining;
            i += remaining;
        }
        if (_stuff->audio_buffers[audio_stream].size() == 0)
        {
            // Read more audio data
            AVPacket packet, tmppacket;
            while (_stuff->audio_packet_queues[audio_stream].empty())
            {
                if (!read())
                {
                    return -1;
                }
            }
            packet = _stuff->audio_packet_queues[audio_stream].front();
            _stuff->audio_packet_queues[audio_stream].pop_front();
            if (timestamp == std::numeric_limits<int64_t>::min())
            {
                timestamp = packet.dts * 1000000
                    * _stuff->format_ctx->streams[_stuff->audio_streams[audio_stream]]->time_base.num 
                    / _stuff->format_ctx->streams[_stuff->audio_streams[audio_stream]]->time_base.den;
            }

            // Decode audio data
            tmppacket = packet;
            while (tmppacket.size > 0)
            {
                // Manage tmpbuf with av_malloc/av_free, to guarantee correct alignment.
                // Not doing this results in hard to debug crashes on some systems.
                int tmpbuf_size = (AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2;
                unsigned char *tmpbuf = static_cast<unsigned char *>(av_malloc(tmpbuf_size));
                int len = avcodec_decode_audio3(_stuff->audio_codec_ctxs[audio_stream],
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
                size_t old_size = _stuff->audio_buffers[audio_stream].size();
                _stuff->audio_buffers[audio_stream].resize(old_size + tmpbuf_size);
                memcpy(&(_stuff->audio_buffers[audio_stream][old_size]), tmpbuf, tmpbuf_size);
                av_free(tmpbuf);
            }
            
            av_free_packet(&packet);
        }
    }

    if (timestamp != std::numeric_limits<int64_t>::min())
    {
        if (_stuff->audio_pos_offsets[audio_stream] == std::numeric_limits<int64_t>::min())
        {
            _stuff->audio_pos_offsets[audio_stream] = timestamp;
        }
        timestamp -= _stuff->audio_pos_offsets[audio_stream];
        _stuff->audio_last_timestamps[audio_stream] = timestamp;
    }
    if (_stuff->audio_last_timestamps[audio_stream] > _stuff->pos)
    {
        _stuff->pos = _stuff->audio_last_timestamps[audio_stream];
    }
    return _stuff->audio_last_timestamps[audio_stream];
}

void decoder_ffmpeg::seek(int64_t dest_pos)
{
    msg::dbg(_filename + ": seeking from " + str::from(_stuff->pos / 1e6f) + " to " + str::from(dest_pos / 1e6f));

    int e = av_seek_frame(_stuff->format_ctx, -1,
            dest_pos * AV_TIME_BASE / 1000000,
            dest_pos < _stuff->pos ?  AVSEEK_FLAG_BACKWARD : 0);
    if (e < 0)
    {
        msg::err(_filename + ": seeking failed");
    }
    else
    {
        /* Set a flag to throw away all queued packets */
        for (size_t i = 0; i < _stuff->video_packet_queues.size(); i++)
        {
            _stuff->video_flush_flags[i] = true;
        }
        for (size_t i = 0; i < _stuff->audio_packet_queues.size(); i++)
        {
            _stuff->audio_flush_flags[i] = true;
        }
        /* The next read request must update the position */
        _stuff->pos = std::numeric_limits<int64_t>::min();
    }
}

void decoder_ffmpeg::close()
{
    for (size_t i = 0; i < _stuff->frames.size(); i++)
    {
        av_free(_stuff->frames[i]);
    }
    for (size_t i = 0; i < _stuff->out_frames.size(); i++)
    {
        av_free(_stuff->out_frames[i]);
    }
    for (size_t i = 0; i < _stuff->buffers.size(); i++)
    {
        av_free(_stuff->buffers[i]);
    }
    for (size_t i = 0; i < _stuff->video_codec_ctxs.size(); i++)
    {
        if (_stuff->video_codecs[i])
        {
            avcodec_close(_stuff->video_codec_ctxs[i]);
        }
    }
    for (size_t i = 0; i < _stuff->img_conv_ctxs.size(); i++)
    {
        sws_freeContext(_stuff->img_conv_ctxs[i]);
    }
    for (size_t i = 0; i < _stuff->video_packet_queues.size(); i++)
    {
        if (_stuff->video_packet_queues[i].size() > 0)
        {
            msg::dbg(_filename + ": " + str::from(_stuff->video_packet_queues[i].size())
                    + " unprocessed video packets in video stream " + str::from(i));
        }
        for (size_t j = 0; j < _stuff->video_packet_queues[i].size(); j++)
        {
            av_free_packet(&_stuff->video_packet_queues[i][j]);
        }
    }
    for (size_t i = 0; i < _stuff->audio_codec_ctxs.size(); i++)
    {
        if (_stuff->audio_codecs[i])
        {
            avcodec_close(_stuff->audio_codec_ctxs[i]);
        }
    }
    for (size_t i = 0; i < _stuff->audio_packet_queues.size(); i++)
    {
        if (_stuff->audio_packet_queues[i].size() > 0)
        {
            msg::dbg(_filename + ": " + str::from(_stuff->audio_packet_queues[i].size())
                    + " unprocessed audio packets in audio stream " + str::from(i));
        }
        for (size_t j = 0; j < _stuff->audio_packet_queues[i].size(); j++)
        {
            av_free_packet(&_stuff->audio_packet_queues[i][j]);
        }
    }
    if (_stuff->format_ctx)
    {
        av_close_input_file(_stuff->format_ctx);
    }
    delete _stuff;
    _stuff = NULL;
}

std::vector<std::string> ffmpeg_versions()
{
    std::vector<std::string> v;
    v.push_back(str::asprintf("FFmpeg libavformat %d.%d.%d / %d.%d.%d",
                LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO,
                avformat_version() >> 16, avformat_version() >> 8 & 0xff, avformat_version() & 0xff));
    v.push_back(str::asprintf("FFmpeg libavcodec %d.%d.%d / %d.%d.%d",
                LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO,
                avcodec_version() >> 16, avcodec_version() >> 8 & 0xff, avcodec_version() & 0xff));
    v.push_back(str::asprintf("FFmpeg libswscale %d.%d.%d / %d.%d.%d",
                LIBSWSCALE_VERSION_MAJOR, LIBSWSCALE_VERSION_MINOR, LIBSWSCALE_VERSION_MICRO,
                swscale_version() >> 16, swscale_version() >> 8 & 0xff, swscale_version() & 0xff));
    return v;
}
