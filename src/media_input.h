/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2018
 * Martin Lambers <marlam@marlam.de>
 * Joe <cuchac@email.cz>
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

#ifndef MEDIA_INPUT_H
#define MEDIA_INPUT_H

#include <vector>

#include "media_data.h"
#include "media_object.h"


class media_input
{
private:
    bool _is_device;                            // Whether this is a device (e.g. a camera)
    std::string _id;                            // ID of this input: URL0[/URL1[/URL2[...]]]
    std::vector<media_object> _media_objects;   // The media objects that are combined into one input
    std::vector<std::string> _tag_names;        // Meta data: tag names
    std::vector<std::string> _tag_values;       // Meta data: tag values

    std::vector<std::string> _video_stream_names;       // Descriptions of available video streams
    std::vector<std::string> _audio_stream_names;       // Descriptions of available audio streams
    std::vector<std::string> _subtitle_stream_names;    // Descriptions of available subtitle streams

    bool _supports_stereo_layout_separate;      // Does this input support the stereo layout 'separate_streams'?
    int _active_video_stream;                   // The video stream that is currently active.
    int _active_audio_stream;                   // The audio stream that is currently active.
    int _active_subtitle_stream;                // The subtitle stream that is currently active.
    bool _have_active_video_read;               // Whether a video frame read was started.
    bool _have_active_audio_read;               // Whether a audio blob read was started.
    bool _have_active_subtitle_read;            // Whether a subtitle box read was started.
    size_t _last_audio_data_size;               // Size of last audio blob read

    int64_t _initial_skip;                      // Initial portion of input to skip, in microseconds.
    int64_t _duration;                          // Total combined duration of input.

    bool _finished_first_frame_read;            // Whether we have finished the first frame read from this input.

    video_frame _video_frame;                   // Video frame template for currently active video stream.
    audio_blob _audio_blob;                     // Audio blob template for currently active audio stream.
    subtitle_box _subtitle_box;                 // Subtitle box template for currently active subtitle stream.

    // Find the media object and its stream index for a given video or audio stream number.
    void get_video_stream(int stream, int &media_object, int &media_object_video_stream) const;
    void get_audio_stream(int stream, int &media_object, int &media_object_audio_stream) const;
    void get_subtitle_stream(int stream, int &media_object, int &media_object_subtitle_stream) const;

public:

    /* Constructor, Destructor */

    media_input();
    ~media_input();

    /* Open this input by combining the media objects at the given URLS.
     * A device can only have a single URL. */

    void open(const std::vector<std::string> &urls, const device_request &dev_request = device_request());

    /* Get information */

    // The number of URLs (= the number of media objects)
    size_t urls() const;
    // Get the URL with the given index
    const std::string &url(size_t i) const;

    // Identifier.
    const std::string &id() const;

    // Metadata
    bool is_device() const;
    size_t tags() const;
    const std::string &tag_name(size_t i) const;
    const std::string &tag_value(size_t i) const;
    const std::string &tag_value(const std::string &tag_name) const;

    // Number of video streams in this input.
    int video_streams() const
    {
        return _video_stream_names.size();
    }

    // Number of audio streams in this input.
    int audio_streams() const
    {
        return _audio_stream_names.size();
    }

    // Number of subtitle streams in this input.
    int subtitle_streams() const
    {
        return _subtitle_stream_names.size();
    }

    // Name of the given video stream.
    const std::string &video_stream_name(int video_stream) const
    {
        return _video_stream_names[video_stream];
    }

    // Name of the given audio stream.
    const std::string &audio_stream_name(int audio_stream) const
    {
        return _audio_stream_names[audio_stream];
    }

    // Name of the given subtitle stream.
    const std::string &subtitle_stream_name(int subtitle_stream) const
    {
        return _subtitle_stream_names[subtitle_stream];
    }

    // Initial portion of the input to skip.
    int64_t initial_skip() const
    {
        return _initial_skip;
    }

    // Total combined duration of this input.
    int64_t duration() const
    {
        return _duration;
    }

    // Information about the active video stream, in the form of a video frame
    // that contains all properties but no actual data.
    const video_frame &video_frame_template() const;
    // Video rate information. This is only informal, as videos do not need to have
    // a constant frame rate. Usually, the presentation time of a frame should be used.
    int video_frame_rate_numerator() const;
    int video_frame_rate_denominator() const;
    int64_t video_frame_duration() const;       // derived from frame rate

    // Information about the active audio stream, in the form of an audio blob
    // that contains all properties but no actual data.
    const audio_blob &audio_blob_template() const;

    // Information about the active subtitle stream, in the form of a subtitle box
    // that contains all properties but no actual data.
    const subtitle_box &subtitle_box_template() const;

    /*
     * Access media data
     */

    /* Set the active media streams.
     * For subtitle streams, -1 selects no subtitle stream. */
    int selected_video_stream() const
    {
        return _active_video_stream;
    }
    void select_video_stream(int video_stream);
    int selected_audio_stream() const
    {
        return _active_audio_stream;
    }
    void select_audio_stream(int audio_stream);
    int selected_subtitle_stream() const
    {
        return _active_subtitle_stream;
    }
    void select_subtitle_stream(int subtitle_stream);

    /* Check whether a stereo layout is supported by this input. */
    bool stereo_layout_is_supported(parameters::stereo_layout_t layout, bool swap) const;
    /* Set the stereo layout. It must be supported by the input. */
    void set_stereo_layout(parameters::stereo_layout_t layout, bool swap);

    /* Start to read a video frame from the active stream asynchronously
     * (in a separate thread). */
    void start_video_frame_read();
    /* Wait for the video frame reading to finish, and return the frame.
     * An invalid frame means that EOF was reached. */
    video_frame finish_video_frame_read();

    /* Start to read the given amount of audio data from the active stream asynchronously
     * (in a separate thread). */
    void start_audio_blob_read(size_t size);
    /* Wait for the audio data reading to finish, and return the blob.
     * An invalid blob means that EOF was reached. */
    audio_blob finish_audio_blob_read();

    /* Start to read a subtitle box from the active stream asynchronously
     * (in a separate thread). */
    void start_subtitle_box_read();
    /* Wait for the subtitle data reading to finish, and return the box.
     * An invalid box means that EOF was reached. */
    subtitle_box finish_subtitle_box_read();

    /* Return the last position in microseconds, of the last packet that was read in an
     * active stream. If the position is unkown, the minimum possible value is returned. */
    int64_t tell();

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

    /* When done, close the input and clean up. */
    void close();
};

#endif
