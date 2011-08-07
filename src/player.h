/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
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

#include "msg.h"
#include "s11n.h"

#include "media_data.h"
#include "media_input.h"
#include "audio_output.h"
#include "video_output.h"


/* The player_init_data contains everything that a player needs to start. */

class player_init_data : public s11n
{
public:
    msg::level_t log_level;                     // Level of log messages
    device_request dev_request;                 // Request for input device settings
    std::vector<std::string> urls;              // Input media objects
    int video_stream;                           // Selected video stream
    int audio_stream;                           // Selected audio stream
    int subtitle_stream;                        // Selected subtitle stream
    bool benchmark;                             // Benchmark mode?
    bool fullscreen;                            // Make video fullscreen?
    bool center;                                // Center video on screen?
    bool stereo_layout_override;                // Manual input layout override?
    video_frame::stereo_layout_t stereo_layout; //   Override layout
    bool stereo_layout_swap;                    //   Override layout swap
    bool stereo_mode_override;                  // Manual output mode override?
    parameters::stereo_mode_t stereo_mode;      //   Override mode
    bool stereo_mode_swap;                      //   Override mode swap
    parameters params;                          // Initial output parameters

public:
    player_init_data();
    ~player_init_data();

    // Serialization
    void save(std::ostream &os) const;
    void load(std::istream &is);
};

/*
 * The player class.
 *
 * A player handles the input and determines the next actions.
 * It receives commands from a controller, reacts on them, and then sends
 * notifications to the controllers to inform them about any state changes.
 */

class player
{
public:
    enum type
    {
        master,         // Only the master player receives commands from controllers
        slave           // Slave players do not receive commands
    };

private:

    /* Input and output objects */

    media_input *_media_input;                  // The media input
    audio_output *_audio_output;                // Audio output
    video_output *_video_output;                // Video output

    /* Current state */

    // The current output parameters
    parameters _params;

    // Benchmark mode
    bool _benchmark;                            // Is benchmark mode active?
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

    // Requests made by controller commands
    bool _quit_request;                         // Request to quit
    bool _pause_request;                        // Request to go into pause mode
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
    float normalize_pos(int64_t pos);

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

    // Create and destroy video and audio output (overridable by subclasses)
    virtual video_output *create_video_output();
    virtual void destroy_video_output(video_output *vo);
    virtual audio_output *create_audio_output();
    virtual void destroy_audio_output(audio_output *ao);

    // Make this player the master player
    void make_master();

    // Execute one step and indicate required actions. Returns the number of microseconds
    // that the caller may sleep before starting the next step.
    int64_t step(bool *more_steps, int64_t *seek_to, bool *prep_frame, bool *drop_frame, bool *display_frame);

    // Execute one step and immediately take required actions. Return true if more steps are required.
    bool run_step();

    // Get the media input for potential changes
    media_input &get_media_input_nonconst()
    {
        return *_media_input;
    }

public:
    /* Constructor/destructor.
     * Only a single player instance can exist. The constructor throws an
     * exception if and only if it detects that this instance already exists. */
    player(type t = master);
    virtual ~player();

    /* Open a player. */
    virtual void open(const player_init_data &init_data);

    /* Get information about input and output parameters */
    bool has_media_input() const
    {
        return _media_input;
    }
    const media_input &get_media_input() const
    {
        return *_media_input;
    }
    const parameters &get_parameters() const
    {
        return _params;
    }

    /* Run the player. It will take care of all interaction. This function
     * returns when the user quits the player. */
    virtual void run();

    /* Close the player and clean up. */
    virtual void close();

    /* Receive a command from a controller. */
    virtual void receive_cmd(const command &cmd);
};

#endif
