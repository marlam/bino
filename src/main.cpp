/*
 * This file is part of bino, a program to play stereoscopic videos.
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

#include "config.h"

#include <cstring>

#include "debug.h"
#include "msg.h"
#include "opt.h"

#include "decoder.h"
#include "decoder_ffmpeg.h"
#include "input.h"
#include "controller.h"
#include "player.h"
#include "audio_output_openal.h"
#include "video_output_opengl.h"


int main(int argc, char *argv[])
{
    /* Initialization */
    char *program_name = strrchr(argv[0], '/');
    program_name = program_name ? program_name + 1 : argv[0];
    msg::set_level(msg::INF);
    msg::set_program_name(program_name);
    msg::set_columns_from_env();
    debug::init_crashhandler();

    /* Command line handling */
    std::vector<opt::option *> options;
    opt::info help("help", '\0', opt::optional);
    options.push_back(&help);
    opt::info version("version", '\0', opt::optional);
    options.push_back(&version);
    std::vector<std::string> log_levels;
    log_levels.push_back("debug");
    log_levels.push_back("info");
    log_levels.push_back("warning");
    log_levels.push_back("error");
    log_levels.push_back("quiet");
    opt::val<std::string> log_level("log-level", 'l', opt::optional, log_levels, "info");
    options.push_back(&log_level);
    std::vector<std::string> input_modes;
    input_modes.push_back("mono");
    input_modes.push_back("separate");
    input_modes.push_back("top-bottom");
    input_modes.push_back("top-bottom-half");
    input_modes.push_back("left-right");
    input_modes.push_back("left-right-half");
    input_modes.push_back("even-odd-rows");
    opt::val<std::string> input_mode("input", 'i', opt::optional, input_modes, "");
    options.push_back(&input_mode);
    std::vector<std::string> video_output_modes;
    video_output_modes.push_back("mono-left");
    video_output_modes.push_back("mono-right");
    video_output_modes.push_back("top-bottom");
    video_output_modes.push_back("top-bottom-half");
    video_output_modes.push_back("left-right");
    video_output_modes.push_back("left-right-half");
    video_output_modes.push_back("even-odd-rows");
    video_output_modes.push_back("even-odd-columns");
    video_output_modes.push_back("checkerboard");
    video_output_modes.push_back("anaglyph");
    video_output_modes.push_back("anaglyph-monochrome");
    video_output_modes.push_back("anaglyph-full-color");
    video_output_modes.push_back("anaglyph-half-color");
    video_output_modes.push_back("anaglyph-dubois");
    video_output_modes.push_back("stereo");
    opt::val<std::string> video_output_mode("output", 'o', opt::optional, video_output_modes, "");
    options.push_back(&video_output_mode);
    opt::flag fullscreen("fullscreen", 'f', opt::optional);
    options.push_back(&fullscreen);
    opt::flag center("center", 'c', opt::optional);
    options.push_back(&center);
    opt::flag swap_eyes("swap-eyes", 's', opt::optional);
    options.push_back(&swap_eyes);
    std::vector<std::string> arguments;
    if (!opt::parse(argc, argv, options, 1, 3, arguments))
    {
        return 1;
    }
    if (version.value())
    {
        msg::req_txt("%s version %s on %s\n"
                "Copyright (C) 2010  Martin Lambers <marlam@marlam.de>.\n"
                "This is free software. You may redistribute copies of it under the terms of "
                "the GNU General Public License.\n"
                "There is NO WARRANTY, to the extent permitted by law.",
                PACKAGE_NAME, VERSION, PLATFORM);
    }
    if (help.value())
    {
        msg::req_txt(
                "Usage:\n"
                "  %s [option...] file0 [file1] [file2]\n"
                "\n"
                "Options:\n"
                "  --help               Print help\n"
                "  --version            Print version\n"
                "  -l|--log-level=LEVEL Set log level (debug, info, warning, error, quiet)\n"
                "  -i|--input=TYPE      Select input type (default autodetect):\n"
                "    mono                 Single view\n"
                "    separate             Left/right view in separate streams\n"
                "    top-bottom           Left view top, right view bottom\n"
                "    top-bottom-half      Left view top, right view bottom, half height\n"
                "    left-right           Left view left, right view right\n"
                "    left-right-half      Left view left, right view right, half width\n"
                "    even-odd-rows        Left view even rows, right view odd rows\n"
                "  -o|--output=TYPE     Select output type (default stereo, anaglyph,\n"
                "                         or mono-left, depending on input and display):\n"
                "    mono-left            Only left view\n"
                "    mono-right           Only right view\n"
                "    top-bottom           Left view top, right view bottom\n"
                "    top-bottom-half      Left view top, right view bottom, half height\n"
                "    left-right           Left view left, right view right\n"
                "    left-right-half      Left view left, right view right, half width\n"
                "    even-odd-rows        Left view even rows, right view odd rows\n"
                "    even-odd-columns     Left view even columns, right view odd columns\n"
                "    checkerboard         Left and right view in checkerboard pattern\n"
                "    anaglyph             Red/cyan anaglyph, default method (Dubois)\n"
                "    anaglyph-monochrome  Red/cyan anaglyph, monochrome method\n"
                "    anaglyph-full-color  Red/cyan anaglyph, full color method\n"
                "    anaglyph-half-color  Red/cyan anaglyph, half color method\n"
                "    anaglyph-dubois      Red/cyan anaglyph, Dubois method\n"
                "    stereo               OpenGL quad-buffered stereo\n"
                "  -f|--fullscreen      Fullscreen\n"
                "  -c|--center          Center window on screen\n"
                "  -s|--swap-eyes       Swap left/right view\n"        
                "\n"
                "Keyboard control:\n"
                "  q or ESC             Quit\n"
                "  p or SPACE           Pause / unpause\n"
                "  f                    Toggle fullscreen\n"
                "  s                    Swap left/right view\n"
                "  1, 2                 Adjust contrast\n"
                "  3, 4                 Adjust brightness\n"
                "  5, 6                 Adjust hue\n"
                "  7, 8                 Adjust saturation\n"
                "  left, right          Seek 10 seconds backward / forward\n"
                "  up, down             Seek 1 minute backward / forward\n"
                "  page up, page down   Seek 10 minutes backward / forward",
                program_name);
    }
    if (version.value() || help.value())
    {
        return 0;
    }

    if (log_level.value() == "debug")
    {
        msg::set_level(msg::DBG);
    }
    else if (log_level.value() == "info")
    {
        msg::set_level(msg::INF);
    }
    else if (log_level.value() == "warning")
    {
        msg::set_level(msg::WRN);
    }
    else if (log_level.value() == "error")
    {
        msg::set_level(msg::ERR);
    }
    else if (log_level.value() == "quiet")
    {
        msg::set_level(msg::REQ);
    }

    std::vector<decoder *> decoders;
    input *input = NULL;
    audio_output *audio_output = NULL;
    video_output *video_output = NULL;
    player *player = NULL;
    int retval = 0;
    try
    {
        /* Create decoders for input files */
        int video_streams = 0;
        int audio_streams = 0;
        for (size_t i = 0; i < arguments.size(); i++)
        {
            decoders.push_back(new decoder_ffmpeg);
            decoders[i]->open(arguments[i].c_str());
            video_streams += decoders[i]->video_streams();
            audio_streams += decoders[i]->audio_streams();
        }
        if (video_streams == 0)
        {
            throw exc("no video streams found");
        }
        if (video_streams > 2)
        {
            throw exc("cannot handle more than 2 video streams");
        }
        if (audio_streams > 1)
        {
            throw exc("cannot handle more than 1 audio stream");
        }

        /* Create input */
        int video_decoder[2] = { -1, -1 };
        int video_stream[2] = { -1, -1 };
        int audio_decoder = -1;
        int audio_stream = -1;
        for (size_t i = 0; i < decoders.size(); i++)
        {
            for (int j = 0; j < decoders[i]->video_streams(); j++)
            {
                if (video_decoder[0] != -1 && video_decoder[1] != -1)
                {
                    continue;
                }
                else if (video_decoder[0] != -1)
                {
                    video_decoder[1] = i;
                    video_stream[1] = j;
                }
                else
                {
                    video_decoder[0] = i;
                    video_stream[0] = j;
                }
            }
            for (int j = 0; j < decoders[i]->audio_streams(); j++)
            {
                if (audio_decoder != -1)
                {
                    continue;
                }
                else
                {
                    audio_decoder = i;
                    audio_stream = j;
                }
            }
        }
        enum input::mode in_mode;
        if (input_mode.value() == "")
        {
            in_mode = input::automatic;
        }
        else if (input_mode.value() == "separate")
        {
            in_mode = input::separate;
        }
        else if (input_mode.value() == "top-bottom")
        {
            in_mode = input::top_bottom;
        }
        else if (input_mode.value() == "top-bottom-half")
        {
            in_mode = input::top_bottom_half;
        }
        else if (input_mode.value() == "left-right")
        {
            in_mode = input::left_right;
        }
        else if (input_mode.value() == "left-right-half")
        {
            in_mode = input::left_right_half;
        }
        else if (input_mode.value() == "even-odd-rows")
        {
            in_mode = input::even_odd_rows;
        }
        else
        {
            in_mode = input::mono;
        }
        input = new class input();
        input->open(decoders,
                video_decoder[0], video_stream[0],
                video_decoder[1], video_stream[1],
                audio_decoder, audio_stream,
                in_mode);

        /* Create audio output */
        if (audio_stream != -1)
        {
            audio_output = new audio_output_openal();
            audio_output->open(input->audio_rate(), input->audio_channels(), input->audio_bits());
        }

        /* Create video output */
        video_output = new video_output_opengl();
        video_output::mode video_mode;
        if (video_output_mode.value() == "")
        {
            if (input->mode() == input::mono)
            {
                video_mode = video_output::mono_left;
            }
            else if (video_output->supports_stereo())
            {
                video_mode = video_output::stereo;
            }
            else
            {
                video_mode = video_output::anaglyph_red_cyan_dubois;
            }
        }
        else if (video_output_mode.value() == "mono-left")
        {
            video_mode = video_output::mono_left;
        }
        else if (video_output_mode.value() == "mono-right")
        {
            video_mode = video_output::mono_right;
        }
        else if (video_output_mode.value() == "top-bottom")
        {
            video_mode = video_output::top_bottom;
        }
        else if (video_output_mode.value() == "top-bottom-half")
        {
            video_mode = video_output::top_bottom_half;
        }
        else if (video_output_mode.value() == "left-right")
        {
            video_mode = video_output::left_right;
        }
        else if (video_output_mode.value() == "left-right-half")
        {
            video_mode = video_output::left_right_half;
        }
        else if (video_output_mode.value() == "even-odd-rows")
        {
            video_mode = video_output::even_odd_rows;
        }
        else if (video_output_mode.value() == "even-odd-columns")
        {
            video_mode = video_output::even_odd_columns;
        }
        else if (video_output_mode.value() == "checkerboard")
        {
            video_mode = video_output::checkerboard;
        }
        else if (video_output_mode.value() == "anaglyph")
        {
            video_mode = video_output::anaglyph_red_cyan_dubois;
        }
        else if (video_output_mode.value() == "anaglyph-monochrome")
        {
            video_mode = video_output::anaglyph_red_cyan_monochrome;
        }
        else if (video_output_mode.value() == "anaglyph-full-color")
        {
            video_mode = video_output::anaglyph_red_cyan_full_color;
        }
        else if (video_output_mode.value() == "anaglyph-half-color")
        {
            video_mode = video_output::anaglyph_red_cyan_half_color;
        }
        else if (video_output_mode.value() == "anaglyph-dubois")
        {
            video_mode = video_output::anaglyph_red_cyan_dubois;
        }
        else
        {
            video_mode = video_output::stereo;
        }
        struct video_output::state video_state;
        video_state.contrast = 0.0f;
        video_state.brightness = 0.0f;
        video_state.hue = 0.0f;
        video_state.saturation = 0.0f;
        video_state.fullscreen = fullscreen.value();
        video_state.swap_eyes = swap_eyes.value();
        unsigned int video_flags = 0;
        if (center.value())
        {
            video_flags |= video_output::center;
        }
        video_output->open(
                input->video_preferred_frame_format(),
                input->video_width(), input->video_height(), input->video_aspect_ratio(),
                video_mode, video_state, video_flags, -1, -1);
        video_output->process_events();

        /* Play */
        player = new class player();
        player->open(input, NULL, audio_output, video_output);
        player->run();
    }
    catch (std::exception &e)
    {
        msg::err("%s", e.what());
        retval = 1;
    }

    /* Cleanup */
    if (player)
    {
        try { player->close(); } catch (...) {}
        delete player;
    }
    if (audio_output)
    {
        try { audio_output->close(); } catch (...) {}
        delete audio_output;
    }
    if (video_output)
    {
        try { video_output->close(); } catch (...) {}
        delete video_output;
    }
    if (input)
    {
        try { input->close(); } catch (...) {}
        delete input;
    }
    for (size_t i = 0; i < decoders.size(); i++)
    {
        try { decoders[i]->close(); } catch (...) {}
        delete decoders[i];
    }

    return retval;
}
