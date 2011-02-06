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

#ifndef MEDIA_OBJECT_H
#define MEDIA_OBJECT_H

#include <string>
#include <vector>

#include "media_data.h"


class media_object
{
private:
    std::string _url;
    struct ffmpeg_stuff *_ffmpeg;
    std::vector<std::string> _tag_names;
    std::vector<std::string> _tag_values;

    void set_video_frame_template(int video_stream);
    void set_audio_blob_template(int audio_stream);

    bool read();
    int64_t handle_timestamp(int64_t &last_timestamp, int64_t timestamp);
    int64_t handle_video_timestamp(int video_stream, int64_t timestamp);
    int64_t handle_audio_timestamp(int audio_stream, int64_t timestamp);

public:

    /* Constructor, Destructor */
    media_object();
    ~media_object();

    /*
     * Initialization
     */

    /* Open a file (or URL) to decode. */
    void open(const std::string &url);

    /* Get metadata */
    const std::string &url() const;
    size_t tags() const;
    const std::string &tag_name(size_t i) const;
    const std::string &tag_value(size_t i) const;
    const std::string &tag_value(const std::string &tag_name) const;

    /* Get the number of video and audio streams in the file. */
    int video_streams() const;
    int audio_streams() const;

    /* Activate a video or audio stream for usage. Inactive streams will not be accessible. */
    void video_stream_set_active(int video_stream, bool active);
    void audio_stream_set_active(int audio_stream, bool active);

    /* Get information about video streams.
     * Note that this is only a hint; the properties of actual video frames may differ! */
    const video_frame &video_frame_template(int video_stream) const;
    int video_frame_rate_numerator(int video_stream) const;
    int video_frame_rate_denominator(int video_stream) const;
    int64_t video_duration(int video_stream) const;

    /* Get information about audio streams.
     * Note that this is only a hint; the properties of actual audio blobs may differ! */
    const audio_blob &audio_blob_template(int audio_stream) const;
    int64_t audio_duration(int video_stream) const;

    /*
     * Access video and audio data
     */

    /* Start to read a video frame asynchronously (in a separate thread). */
    void start_video_frame_read(int video_stream);
    /* Wait for the video frame reading to finish, and return the frame.
     * An invalid frame means that EOF was reached. */
    video_frame finish_video_frame_read(int video_stream);

    /* Start to read the given amount of audio data asynchronously (in a separate thread). */
    void start_audio_blob_read(int audio_stream, size_t size);
    /* Wait for the audio data reading to finish, and return the blob.
     * An invalid blob means that EOF was reached. */
    audio_blob finish_audio_blob_read(int audio_stream);

    /* Seek to the given position in microseconds. This affects all streams.
     * Make sure that the position is not out of range!
     * The real position after seeking is only revealed after reading the next video frame
     * or audio blob. This position may differ from the requested position for various
     * reasons (seeking is only possible to keyframes, seeking is not supported by the
     * stream, ...) */
    void seek(int64_t pos);

    /*
     * Cleanup
     */

    /* When done, close the file (or URL) and clean up. */
    void close();
};

#endif
