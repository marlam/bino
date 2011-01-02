/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
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

#ifndef DECODER_H
#define DECODER_H

#include <string>
#include <vector>
#include <stdint.h>


class decoder
{
protected:
    std::vector<std::string> _tag_names;
    std::vector<std::string> _tag_values;

public:

    /* The video frame format */

    enum video_layout
    {
        video_layout_bgra32           = 0,       // Single plane: BGRABGRABGRA....
        video_layout_yuv444p          = 1,       // Three planes, all with the same size
        video_layout_yuv422p          = 2,       // Three planes, U and V with half width: one U/V pair for 2x1 Y values
        video_layout_yuv420p          = 3        // Three planes, U and V with half width and half height: one U/V pair for 2x2 Y values
    };

    enum video_color_space
    {
        video_color_space_srgb        = 0 << 2,  // SRGB color space
        video_color_space_yuv601      = 1 << 2,  // YUV according to ITU.BT-601
        video_color_space_yuv709      = 2 << 2,  // YUV according to ITU.BT-709
    };

    enum video_value_range
    {
        video_value_range_8bit_full   = 0 << 4,  // 0-255 for all components
        video_value_range_8bit_mpeg   = 1 << 4   // 16-235 for Y, 16-240 for U and V
    };

    enum video_chroma_location
    {
        video_chroma_location_center  = 0 << 5,  // U/V at center of the corresponding Y locations
        video_chroma_location_left    = 1 << 5,  // U/V vertically at the center, horizontally at the left Y locations
        video_chroma_location_topleft = 2 << 5   // U/V at the corresponding top left Y location
    };

    static int video_format(
            enum video_layout l,
            enum video_color_space cs,
            enum video_value_range vr,
            enum video_chroma_location cl)
    {
        return (l | cs | vr | cl);
    }

    static enum video_layout video_format_layout(int video_format)
    {
        return static_cast<enum video_layout>(video_format & 0x03);
    }

    static enum video_color_space video_format_color_space(int video_format)
    {
        return static_cast<enum video_color_space>(video_format & 0x0c);
    }

    static enum video_value_range video_format_value_range(int video_format)
    {
        return static_cast<enum video_value_range>(video_format & 0x10);
    }

    static enum video_chroma_location video_format_chroma_location(int video_format)
    {
        return static_cast<enum video_chroma_location>(video_format & 0x60);
    }

    static int video_format_planes(int video_format)
    {
        return (video_format_layout(video_format) == video_layout_bgra32 ? 1 : 3);
    }

    static std::string video_format_name(int video_format);

    /* The audio sample format */

    enum audio_sample_format
    {
        audio_sample_u8,
        audio_sample_s16,
        audio_sample_f32,
        audio_sample_d64
    };

    static std::string audio_sample_format_name(enum audio_sample_format f);
    static int audio_sample_format_bits(enum audio_sample_format f);

    /* Constructor, Destructor */

    decoder() throw ();
    virtual ~decoder();

    /*
     * Initialization
     */

    /* Open a file (or URL) to decode. */
    virtual void open(const std::string &filename) = 0;

    /* Get the number of video and audio streams in the file. */
    virtual int video_streams() const throw () = 0;
    virtual int audio_streams() const throw () = 0;

    /* Activate a video or audio stream for usage. Inactive streams will not be accessible. */
    virtual void activate_video_stream(int video_stream) = 0;
    virtual void activate_audio_stream(int audio_stream) = 0;

    /* Get information about video streams. */
    virtual int video_width(int video_stream) const throw () = 0;                       // pixels
    virtual int video_height(int video_stream) const throw () = 0;                      // pixels
    virtual int video_aspect_ratio_numerator(int video_stream) const throw () = 0;      // aspect ratio of a frame (not of a pixel!)
    virtual int video_aspect_ratio_denominator(int video_stream) const throw () = 0;    // aspect ratio of a frame (not of a pixel!)
    virtual int video_frame_rate_numerator(int video_stream) const throw () = 0;        // frames per second
    virtual int video_frame_rate_denominator(int video_stream) const throw () = 0;      // frames per second
    virtual int64_t video_duration(int video_stream) const throw () = 0;                // microseconds
    virtual int video_format(int video_stream) const throw () = 0;

    /* Get information about audio streams. */
    virtual int audio_rate(int audio_stream) const throw () = 0;                // samples per second
    virtual int audio_channels(int audio_stream) const throw () = 0;            // 1 (mono), 2 (stereo), 4 (quad), 6 (5:1), 7 (6:1), or 8 (7:1)
    virtual enum audio_sample_format audio_sample_format(int audio_stream) const throw () = 0;  // one of the formats deinfed above
    virtual int64_t audio_duration(int video_stream) const throw () = 0;        // microseconds

    /* Get metadata */
    virtual const char *file_name() const = 0;          // the file name, or URL
    size_t tags() const;                                // number of metadata tags
    const char *tag_name(size_t i) const;               // get name of given tag
    const char *tag_value(size_t i) const;              // get value of given tag
    const char *tag_value(const char *tag_name) const;  // get value of tag with the given name (returns NULL if no such tag exists)

    /*
     * Access video and audio data
     */

    /* Read a video frame into an internal buffer, and return its presentation time stamp
     * in microseconds (between 0 and video_duration(video_stream)).
     * A negative time stamp means that the end of the stream was reached.
     * After reading a frame, you may call get_video_frame(), and you must call release_video_frame(). */
    virtual int64_t read_video_frame(int video_stream) = 0;
    /* Decode the video frame from the internal buffer into the frame format of the given stream.
     * A pointer to each resulting plane data will be returned in 'data'. The total size in
     * bytes of one frame line will be returned in 'line_size' for each plane. This may be
     * larger than the size of one line for alignment reasons. */
    virtual void get_video_frame(int video_stream, uint8_t *data[3], size_t line_size[3]) = 0;
    /* Release the video frame from the internal buffer. Can be called after get_video_frame(),
     * but also directly after read_video_frame(), e.g. in case video playback is too slow and
     * you need to skip frames. */
    virtual void release_video_frame(int video_stream) = 0;

    /* Read the given amount of audio data and stored it in the given buffer.
     * Return the presentation time stamp in microseconds. Beware: this is only useful
     * when starting audio playback; after that, the exact audio time should be measured
     * by the audio output because it depends on software and hardware buffering.
     * A negative time stamp means that the end of the stream was reached. */
    virtual int64_t read_audio_data(int audio_stream, void *buffer, size_t size) = 0;

    /* Seek to the given position in microseconds. Make sure that the position is not out
     * of range. Seeking affects all active video and audio streams.
     * The real position after seeking is only revealed after reading the next video frame
     * or audio data block. This position may differ from the requested position for various
     * reasons (seeking is only possible to keyframes, seeking is not supported by the
     * stream, ...) */
    virtual void seek(int64_t pos) = 0;

    /*
     * Cleanup
     */

    /* When done, close the file (or URL) and clean up. */
    virtual void close() = 0;
};

#endif
