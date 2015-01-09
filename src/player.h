/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2015
 * Martin Lambers <marlam@marlam.de>
 * Alexey Osipov <lion-simba@pridelands.ru>
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

#ifndef PLAYER_H
#define PLAYER_H

#include <vector>
#include <string>

#include "base/msg.h"
#include "base/ser.h"

#include "dispatch.h"


/*
 * The player class.
 */

class player
{
private:
    /* Current state */

    // Benchmark mode
    int _frames_shown;                          // Frames shown since last reset
    int64_t _fps_mark_time;                     // Time when _frames_shown was reset to zero

    // The play state
    bool _running;                              // Are we running?
    bool _first_frame;                          // Did we already process the first video frame?
    bool _need_frame_now;                       // Do we need another video frame right now?
    bool _need_frame_soon;                      // Do we need another video frame soon?
    bool _drop_next_frame;                      // Do we need to drop the next video frame (to catch up)?
    bool _previous_frame_dropped;               // Did we drop the previous video frame?
    bool _in_pause;                             // Are we in pause mode?
    bool _recently_seeked;                      // We did not yet display a video frame after the last seek.

    // Requests made by controller commands
    bool _quit_request;                         // Request to quit
    bool _pause_request;                        // Request to go into pause mode
    bool _step_request;                         // Request for single-frame stepping mode
    int64_t _seek_request;                      // Request to seek relative to the current position
    float _set_pos_request;                     // Request to seek to the absolute position (normalized 0..1)

    /* Timing information. All times are in microseconds.
     * The master time is the audio time if audio output is available,
     * or external system time if there is no audio. */

    int64_t _pause_start;                       // Start of the pause mode (if active)
    int64_t _start_pos;                         // Initial input position
    int64_t _current_pos;                       // Current input position
    int64_t _video_pos;                         // Presentation time of current video frame
    int64_t _audio_pos;                         // Presentation time of current audio blob
    int64_t _master_time_start;                 // Master time offset
    int64_t _master_time_current;               // Current master time
    int64_t _master_time_pos;                   // Input position at master time start

    /* Helper functions */

    // Normalize an input position to [0,1]
    float normalize_pos(int64_t pos) const;

    // Set the current subtitle from the next subtitle
    void set_current_subtitle_box();

    // Reset the play state
    void reset_playstate();

    // Stop playback
    void stop_playback();

protected:
    // The current video frame and subtitle
    video_frame _video_frame;
    subtitle_box _current_subtitle_box;
    subtitle_box _next_subtitle_box;

public:
    /* Constructor/destructor.
     * Only a single player instance can exist. The constructor throws an
     * exception if and only if it detects that this instance already exists. */
    player();
    virtual ~player();

    /* Open a player. */
    virtual void open();

    /* Close the player and clean up. */
    virtual void close();

    // Execute one step and indicate required actions. Returns the number of microseconds
    // that the caller may sleep before starting the next step.
    int64_t step(bool *more_steps, int64_t *seek_to, bool *prep_frame, bool *drop_frame, bool *display_frame);

    // Execute one step and immediately take required actions. Return true if more steps are required.
    bool run_step();

    /* Dispatch interface. TODO: remove this */
    void quit_request();
    void set_pause(bool p);
    void set_step(bool s);
    void seek(int64_t offset);
    void set_pos(float pos);

    int set_video_stream(int s);
    int set_audio_stream(int s);
    int set_subtitle_stream(int s);
    bool set_fullscreen(bool fs);
    void center();
    
    void set_stereo_layout(parameters::stereo_layout_t stereo_layout);
    void set_stereo_layout_swap(bool swap);

    float get_pos() const;

    /* Equalizer interface. */
    const video_frame& get_video_frame() const
    {
        return _video_frame;
    }
    const subtitle_box& get_subtitle_box() const
    {
        return _current_subtitle_box;
    }
};

#endif
