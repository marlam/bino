/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010  Martin Lambers <marlam@marlam.de>
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

    enum video_frame_format
    {
        frame_format_yuv420p,    // 3 planes for Y, U, V, with one U and V value per 4 Y values
        frame_format_bgra32      // 1 plane: BGRABGRABGRA...
    };

    static std::string video_frame_format_name(enum video_frame_format f);

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
    virtual enum video_frame_format video_preferred_frame_format(int video_stream) const throw () = 0;

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
    /* Decode the video frame from the internal buffer and transform it into a fixed format.
     * A pointer to each resulting plane data will be returned in 'data'. The total size in
     * bytes of one frame line will be returned in 'line_size' for each plane. This may be
     * larger than the size of one line for alignment reasons. */
    virtual void get_video_frame(int video_stream, enum video_frame_format fmt,
            uint8_t *data[3], size_t line_size[3]) = 0;
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
