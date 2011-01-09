/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
 * Stefan Eilemann <eile@eyescale.ch>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
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

#include "player.h"
#include "player_qt.h"
#if HAVE_LIBEQUALIZER
# include "player_equalizer.h"
#endif
#include "lib_versions.h"


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
    opt::flag show_gui("gui", 'g', opt::optional);
    options.push_back(&show_gui);
    std::vector<std::string> log_levels;
    log_levels.push_back("debug");
    log_levels.push_back("info");
    log_levels.push_back("warning");
    log_levels.push_back("error");
    log_levels.push_back("quiet");
    opt::val<std::string> log_level("log-level", 'l', opt::optional, log_levels, "");
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
    opt::val<int> audio("audio", 'a', opt::optional, 1, 999, 1);
    options.push_back(&audio);
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
    video_output_modes.push_back("equalizer");
    video_output_modes.push_back("equalizer-3d");
    opt::val<std::string> video_output_mode("output", 'o', opt::optional, video_output_modes, "");
    options.push_back(&video_output_mode);
    opt::flag fullscreen("fullscreen", 'f', opt::optional);
    options.push_back(&fullscreen);
    opt::flag center("center", 'c', opt::optional);
    options.push_back(&center);
    opt::flag swap_eyes("swap-eyes", 's', opt::optional);
    options.push_back(&swap_eyes);
    opt::flag benchmark("benchmark", 'b', opt::optional);
    options.push_back(&benchmark);
    opt::val<float> parallax("parallax", 'P', opt::optional, -1.0f, +1.0f, 0.0f);
    options.push_back(&parallax);
    opt::tuple<float> crosstalk("crosstalk", 'C', opt::optional, 0.0f, 100.0f, std::vector<float>(3, 0.0f));
    options.push_back(&crosstalk);
    opt::val<float> ghostbust("ghostbust", 'G', opt::optional, 0.0f, 100.0f);
    options.push_back(&ghostbust);
    // Accept some Equalizer options. These are passed to Equalizer for interpretation.
    opt::val<std::string> eq_server("eq-server", '\0', opt::optional);
    options.push_back(&eq_server);
    opt::val<std::string> eq_config("eq-config", '\0', opt::optional);
    options.push_back(&eq_config);
    opt::val<std::string> eq_listen("eq-listen", '\0', opt::optional);
    options.push_back(&eq_listen);
    opt::val<std::string> eq_logfile("eq-logfile", '\0', opt::optional);
    options.push_back(&eq_logfile);
    opt::val<std::string> eq_render_client("eq-render-client", '\0', opt::optional);
    options.push_back(&eq_render_client);

    std::vector<std::string> arguments;
#ifdef __APPLE__
    // strip -psn* option when launching from the Finder on mac
    if (argc > 1 && strncmp("-psn", argv[1], 4) == 0)
    {
        argc--;
        argv++;
    }
#endif
    if (!opt::parse(argc, argv, options, 0, 3, arguments))
    {
        return 1;
    }
    if (version.value())
    {
        msg::req("%s version %s", PACKAGE_NAME, VERSION);
        msg::req("    Copyright (C) 2011 the Bino developers.");
        msg::req("    This is free software. You may redistribute copies of it");
        msg::req("    under the terms of the GNU General Public License.");
        msg::req("    There is NO WARRANTY, to the extent permitted by law.");
        msg::req("Platform:");
        msg::req("    %s", PLATFORM);
        msg::req("Libraries used:");
        std::vector<std::string> v = lib_versions();
        for (size_t i = 0; i < v.size(); i++)
        {
            msg::req("    %s", v[i].c_str());
        }
    }
    if (help.value())
    {
        msg::req_txt(
                "Usage:\n"
                "  %s [option...] [file0] [file1] [file2]\n"
                "\n"
                "Options:\n"
                "  --help               Print help\n"
                "  --version            Print version\n"
                "  -g|--gui             Show the GUI (default if no files are given)\n"
                "  -l|--log-level=LEVEL Set log level (debug, info, warning, error, quiet)\n"
                "  -a|--audio=STREAM    Select audio stream (1-n, depending on the input)\n"
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
                "    equalizer            Multi-display OpenGL via Equalizer (2D setup)\n"
                "    equalizer-3d         Multi-display OpenGL via Equalizer (3D setup)\n"
                "  -f|--fullscreen      Fullscreen\n"
                "  -c|--center          Center window on screen\n"
                "  -s|--swap-eyes       Swap left/right view\n"
                "  -P|--parallax=VAL    Parallax adjustment (-1 to +1)\n"
                "  -C|--crosstalk=VAL   Crosstalk leak level in %% (0 to 100). VAL may\n"
                "                       be one value or comma-separated R,G,B values.\n"
                "  -G|--ghostbust=VAL   Amount of ghostbusting to apply, in %% (0 to 100).\n"
                "  -b|--benchmark       Benchmark mode (no audio, no timesync, show fps)\n"
                "\n"
                "Interactive control:\n"
                "  q or ESC             Quit\n"
                "  p or SPACE           Pause / unpause\n"
                "  f                    Toggle fullscreen\n"
                "  c                    Center window\n"
                "  s                    Swap left/right view\n"
                "  1, 2                 Adjust contrast\n"
                "  3, 4                 Adjust brightness\n"
                "  5, 6                 Adjust hue\n"
                "  7, 8                 Adjust saturation\n"
                "  <, >                 Adjust parallax\n"
                "  (, )                 Adjust ghostbusting\n"
                "  left, right          Seek 10 seconds backward / forward\n"
                "  up, down             Seek 1 minute backward / forward\n"
                "  page up, page down   Seek 10 minutes backward / forward\n"
                "  Mouse click          Seek according to the horizontal click position",
                program_name);
    }
    if (version.value() || help.value())
    {
        return 0;
    }
    if (crosstalk.value().size() != 1 && crosstalk.value().size() != 3)
    {
        msg::err("Invalid crosstalk levels");
        return 1;
    }

#if HAVE_LIBEQUALIZER
    if (arguments.size() > 0 && arguments[0] == "--eq-client")
    {
        try
        {
            player_equalizer *player = new player_equalizer(&argc, argv, true);
            delete player;
        }
        catch (std::exception &e)
        {
            msg::err("%s", e.what());
            return 1;
        }
        return 0;
    }
#endif

    bool equalizer = false;
    bool equalizer_flat_screen = true;
    player_init_data init_data;
    init_data.log_level = msg::level();
    if (log_level.value() == "")
    {
        init_data.log_level = msg::INF;
    }
    else if (log_level.value() == "debug")
    {
        init_data.log_level = msg::DBG;
    }
    else if (log_level.value() == "info")
    {
        init_data.log_level = msg::INF;
    }
    else if (log_level.value() == "warning")
    {
        init_data.log_level = msg::WRN;
    }
    else if (log_level.value() == "error")
    {
        init_data.log_level = msg::ERR;
    }
    else if (log_level.value() == "quiet")
    {
        init_data.log_level = msg::REQ;
    }
    init_data.filenames = arguments;
    init_data.audio_stream = audio.value() - 1;
    if (input_mode.value() == "")
    {
        init_data.input_mode = input::automatic;
    }
    else
    {
        bool ok;
        init_data.input_mode = input::mode_from_name(input_mode.value(), &ok);
        if (!ok)
        {
            msg::err("Invalid input mode name");
            debug::crash();
        }
    }
    if (video_output_mode.value() == "equalizer")
    {
        equalizer = true;
        init_data.video_mode = video_output::mono_left;
    }
    else if (video_output_mode.value() == "equalizer-3d")
    {
        equalizer = true;
        equalizer_flat_screen = false;
        init_data.video_mode = video_output::mono_left;
    }
    else if (video_output_mode.value() == "anaglyph")
    {
        init_data.video_mode = video_output::anaglyph_red_cyan_dubois;
    }
    else if (video_output_mode.value() == "")
    {
        init_data.video_mode = video_output::automatic;
    }
    else
    {
        bool ok;
        init_data.video_mode = video_output::mode_from_name(video_output_mode.value(), &ok);
        if (!ok)
        {
            msg::err("Invalid output mode name");
            debug::crash();
        }
    }
    init_data.video_state.parallax = parallax.value();
    init_data.video_state.crosstalk_r = crosstalk.value()[0] / 100.0f;
    init_data.video_state.crosstalk_g = crosstalk.value()[crosstalk.value().size() == 3 ? 1 : 0] / 100.0f;
    init_data.video_state.crosstalk_b = crosstalk.value()[crosstalk.value().size() == 3 ? 2 : 0] / 100.0f;
    init_data.video_state.ghostbust = ghostbust.value() / 100.0f;
    init_data.video_state.fullscreen = fullscreen.value();
    init_data.video_state.swap_eyes = swap_eyes.value();
    if (center.value())
    {
        init_data.video_flags |= video_output::center;
    }
    init_data.benchmark = benchmark.value();
    if (init_data.benchmark)
    {
        msg::inf("Benchmark mode: audio and time synchronization disabled");
    }

    int retval = 0;
    player *player = NULL;
    try
    {
        if (equalizer)
        {
#if HAVE_LIBEQUALIZER
            player = new class player_equalizer(&argc, argv, equalizer_flat_screen);
#else
            throw exc("This version of Bino was compiled without support for Equalizer");
#endif
        }
        else if (arguments.size() == 0 || show_gui.value())
        {
            if (log_level.value() == "")
            {
                init_data.log_level = msg::WRN;         // Be silent by default when the GUI is used
            }
            init_data.video_state.fullscreen = false;   // GUI overrides fullscreen setting
            init_data.video_flags = 0;                  // GUI overrides center flag
            player = new class player_qt();
        }
        else
        {
            player = new class player();
        }
        player->open(init_data);
        player->run();
    }
    catch (std::exception &e)
    {
        msg::err("%s", e.what());
        retval = 1;
    }
    if (player)
    {
        try { player->close(); } catch (...) {}
        delete player;
    }

    return retval;
}
