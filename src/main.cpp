/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
 * Martin Lambers <marlam@marlam.de>
 * Stefan Eilemann <eile@eyescale.ch>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
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

#include "config.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <limits>
#include <locale.h>

#if (defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__
# include <windows.h>
#endif

#ifdef __APPLE__
# include <mach-o/dyld.h> // for _NSGetExecutablePath()
#endif

#include <QCoreApplication>
#include <QApplication>
#include <QtGlobal>
#include <QTextCodec>
#ifdef Q_WS_X11
# include <X11/Xlib.h>
#endif

#include "gettext.h"
#define _(string) gettext(string)

#include "dbg.h"
#include "msg.h"
#include "opt.h"

#include "dispatch.h"
#include "audio_output.h"
#if HAVE_LIBEQUALIZER
# include "player_equalizer.h"
#endif
#if HAVE_LIBLIRCCLIENT
# include "lirc.h"
#endif
#include "lib_versions.h"


/* Return the directory containing our locale data (translated messages). */
static const char *localedir()
{
#if (defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__
    /* Windows: D/../locale, where D contains the program binary. If something
     * goes wrong, we fall back to "..\\locale", which at least works if the
     * current directory contains the program binary. */
    static const char *rel_locale = "..\\locale";
    static char buffer[MAX_PATH + 10];  // leave space to append rel_locale
    DWORD v = GetModuleFileName(NULL, buffer, MAX_PATH);
    if (v == 0 || v >= MAX_PATH)
    {
        return rel_locale;
    }
    char *backslash = strrchr(buffer, '\\');
    if (!backslash)
    {
        return rel_locale;
    }
    strcpy(backslash + 1, "..\\locale");
    return buffer;
#else
# ifdef __APPLE__
    try
    {
        static char buffer[PATH_MAX];
        uint32_t buffersize = sizeof(buffer);
        int32_t check = _NSGetExecutablePath(buffer, &buffersize);
        if (check != 0)
        {
            throw 0;
        }
        char *pch = std::strrchr(buffer, '/');
        if (pch == NULL)
        {
            throw 0;
        }
        *pch = 0;
        pch = std::strrchr(buffer, '/');
        if (pch == NULL)
        {
            throw 0;
        }
        // check that I'm an application
        if (std::strncmp(pch, "/MacOS", 6) != 0)
        {
            throw 0;
        }
        const char *subdir = "/Resources/locale";
        size_t subdirlen = std::strlen(subdir) + 1;
        // check that there's enough room left
        if (sizeof(buffer) - (buffer-pch) <= subdirlen)
        {
            throw 0;
        }
        std::strncpy(pch, subdir, subdirlen);
        return buffer;
    }
    catch (...)
    {
    }
# endif
    /* GNU/Linux and others: fixed directory defined by LOCALEDIR */
    return LOCALEDIR;
#endif
}

static void qt_msg_handler(QtMsgType type, const char *msg)
{
    switch (type)
    {
    case QtDebugMsg:
        msg::dbg("%s", msg);
        break;
    case QtWarningMsg:
        msg::wrn("%s", msg);
        break;
    case QtCriticalMsg:
        msg::err("%s", msg);
        break;
    case QtFatalMsg:
        msg::err("%s", msg);
        std::exit(1);
    }
}

// Handle a log file that may be set via the --log-file option; see below
static FILE *logf = NULL;
static void close_log_file(void)
{
    if (logf)
    {
        (void)std::fclose(logf);
    }
}


int main(int argc, char *argv[])
{
    /* Initialization: gettext */
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, localedir());
    textdomain(PACKAGE);

    /* Initialization: messages */
    char *program_name = strrchr(argv[0], '/');
    program_name = program_name ? program_name + 1 : argv[0];
    msg::set_level(msg::INF);
    msg::set_program_name(program_name);
    msg::set_columns_from_env();
    dbg::init_crashhandler();

    /* Initialization: Qt */
#ifdef Q_WS_X11
    const char *display = getenv("DISPLAY");
    bool have_display = (display && display[0] != '\0');
    if (have_display) {
        XInitThreads();
    }
#else
    bool have_display = true;
#endif
    qInstallMsgHandler(qt_msg_handler);
    QApplication *qt_app = new QApplication(argc, argv, have_display);
    QTextCodec::setCodecForCStrings(QTextCodec::codecForLocale()); // necessary for i18n via gettext
    QCoreApplication::setOrganizationName("Bino");
    QCoreApplication::setOrganizationDomain("bino3d.org");
    QCoreApplication::setApplicationName(PACKAGE_NAME);

    /* Command line handling */
    std::vector<opt::option *> options;
    opt::info help("help", '\0', opt::optional);
    options.push_back(&help);
    opt::info version("version", '\0', opt::optional);
    options.push_back(&version);
    opt::flag no_gui("no-gui", 'n', opt::optional);
    options.push_back(&no_gui);
    opt::val<std::string> log_file("log-file", '\0', opt::optional);
    options.push_back(&log_file);
    std::vector<std::string> log_levels;
    log_levels.push_back("debug");
    log_levels.push_back("info");
    log_levels.push_back("warning");
    log_levels.push_back("error");
    log_levels.push_back("quiet");
    opt::val<std::string> log_level("log-level", 'L', opt::optional, log_levels, "");
    options.push_back(&log_level);
    opt::info list_audio_devices("list-audio-devices", '\0', opt::optional);
    options.push_back(&list_audio_devices);
    opt::val<int> audio_device("audio-device", 'A', opt::optional, 0, 999, 0);
    options.push_back(&audio_device);
    opt::val<int> audio_delay("audio-delay", 'D', opt::optional, -10000, +10000, 0);
    options.push_back(&audio_delay);
    opt::val<float> audio_volume("audio-volume", 'V', opt::optional, 0.0f, 1.0f, 1.0f);
    options.push_back(&audio_volume);
    opt::flag audio_mute("audio-mute", 'm', opt::optional);
    options.push_back(&audio_mute);
    std::vector<std::string> device_types;
    device_types.push_back("default");
    device_types.push_back("firewire");
    device_types.push_back("x11");
    opt::val<std::string> device_type("device-type", '\0', opt::optional, device_types, "");
    options.push_back(&device_type);
    opt::tuple<int> device_frame_size("device-frame-size", '\0', opt::optional,
            1, std::numeric_limits<int>::max(), std::vector<int>(2, 0), 2, "x");
    options.push_back(&device_frame_size);
    opt::tuple<int> device_frame_rate("device-frame-rate", '\0', opt::optional,
            1, std::numeric_limits<int>::max(), std::vector<int>(2, 0), 2, "/");
    options.push_back(&device_frame_rate);
    opt::val<std::string> lirc_config("lirc-config", '\0', opt::optional);
    options.push_back(&lirc_config);
    std::vector<std::string> input_modes;
    input_modes.push_back("mono");
    input_modes.push_back("separate-left-right");
    input_modes.push_back("separate-right-left");
    input_modes.push_back("alternating-left-right");
    input_modes.push_back("alternating-right-left");
    input_modes.push_back("top-bottom");
    input_modes.push_back("top-bottom-half");
    input_modes.push_back("bottom-top");
    input_modes.push_back("bottom-top-half");
    input_modes.push_back("left-right");
    input_modes.push_back("left-right-half");
    input_modes.push_back("right-left");
    input_modes.push_back("right-left-half");
    input_modes.push_back("even-odd-rows");
    input_modes.push_back("odd-even-rows");
    opt::val<std::string> input_mode("input", 'i', opt::optional, input_modes, "");
    options.push_back(&input_mode);
    opt::val<int> video("video", 'v', opt::optional, 1, 999, 1);
    options.push_back(&video);
    opt::val<int> audio("audio", 'a', opt::optional, 1, 999, 1);
    options.push_back(&audio);
    opt::val<int> subtitle("subtitle", 's', opt::optional, 0, 999, 0);
    options.push_back(&subtitle);
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
    video_output_modes.push_back("hdmi-frame-pack");
    video_output_modes.push_back("red-cyan-monochrome");
    video_output_modes.push_back("red-cyan-half-color");
    video_output_modes.push_back("red-cyan-full-color");
    video_output_modes.push_back("red-cyan-dubois");
    video_output_modes.push_back("green-magenta-monochrome");
    video_output_modes.push_back("green-magenta-half-color");
    video_output_modes.push_back("green-magenta-full-color");
    video_output_modes.push_back("green-magenta-dubois");
    video_output_modes.push_back("amber-blue-monochrome");
    video_output_modes.push_back("amber-blue-half-color");
    video_output_modes.push_back("amber-blue-full-color");
    video_output_modes.push_back("amber-blue-dubois");
    video_output_modes.push_back("red-green-monochrome");
    video_output_modes.push_back("red-blue-monochrome");
    video_output_modes.push_back("stereo");
    video_output_modes.push_back("equalizer");
    video_output_modes.push_back("equalizer-3d");
    opt::val<std::string> video_output_mode("output", 'o', opt::optional, video_output_modes, "");
    options.push_back(&video_output_mode);
    opt::flag swap_eyes("swap-eyes", 'S', opt::optional);
    options.push_back(&swap_eyes);
    opt::flag fullscreen("fullscreen", 'f', opt::optional);
    options.push_back(&fullscreen);
    opt::tuple<int> fullscreen_screens("fullscreen-screens", '\0', opt::optional, 1, 16);
    options.push_back(&fullscreen_screens);
    opt::flag fullscreen_flip_left("fullscreen-flip-left", '\0', opt::optional);
    options.push_back(&fullscreen_flip_left);
    opt::flag fullscreen_flop_left("fullscreen-flop-left", '\0', opt::optional);
    options.push_back(&fullscreen_flop_left);
    opt::flag fullscreen_flip_right("fullscreen-flip-right", '\0', opt::optional);
    options.push_back(&fullscreen_flip_right);
    opt::flag fullscreen_flop_right("fullscreen-flop-right", '\0', opt::optional);
    options.push_back(&fullscreen_flop_right);
    opt::val<float> zoom("zoom", 'z', opt::optional, 0.0f, 1.0f);
    options.push_back(&zoom);
    opt::tuple<float> crop_aspect_ratio("crop", 'C', opt::optional, 0.0f, 100.0f, std::vector<float>(), 2, ":");
    options.push_back(&crop_aspect_ratio);
    opt::flag center("center", 'c', opt::optional);
    options.push_back(&center);
    opt::val<std::string> subtitle_encoding("subtitle-encoding", '\0', opt::optional);
    options.push_back(&subtitle_encoding);
    opt::val<std::string> subtitle_font("subtitle-font", '\0', opt::optional);
    options.push_back(&subtitle_font);
    opt::val<int> subtitle_size("subtitle-size", '\0', opt::optional, 1, 999);
    options.push_back(&subtitle_size);
    opt::val<float> subtitle_scale("subtitle-scale", '\0', opt::optional, 0.0f, false, std::numeric_limits<float>::max(), true);
    options.push_back(&subtitle_scale);
    opt::color subtitle_color("subtitle-color", '\0', opt::optional);
    options.push_back(&subtitle_color);
    opt::val<float> subtitle_parallax("subtitle-parallax", '\0', opt::optional, -1.0f, +1.0f);
    options.push_back(&subtitle_parallax);
    opt::val<float> parallax("parallax", 'P', opt::optional, -1.0f, +1.0f);
    options.push_back(&parallax);
    opt::tuple<float> crosstalk("crosstalk", '\0', opt::optional, 0.0f, 1.0f, std::vector<float>(), 3);
    options.push_back(&crosstalk);
    opt::val<float> ghostbust("ghostbust", '\0', opt::optional, 0.0f, 1.0f);
    options.push_back(&ghostbust);
    opt::flag benchmark("benchmark", 'b', opt::optional);
    options.push_back(&benchmark);
    opt::val<int> swap_interval("swap-interval", '\0', opt::optional, 0, 999);
    options.push_back(&swap_interval);
    opt::flag loop("loop", 'l', opt::optional);
    options.push_back(&loop);
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
    if (!opt::parse(argc, argv, options, 0, -1, arguments))
    {
        return 1;
    }
    if (!log_file.value().empty())
    {
        logf = std::fopen(log_file.value().c_str(), "a");
        if (!logf)
        {
            msg::err(_("%s: %s"), log_file.value().c_str(), std::strerror(errno));
            return 1;
        }
        std::atexit(close_log_file);
        msg::set_file(logf);
        msg::set_columns(80);
    }

    if (version.value())
    {
        if (msg::file() == stderr)
            msg::set_file(stdout);
        msg::req(_("%s version %s"), PACKAGE_NAME, VERSION);
        msg::req(4, _("Copyright (C) 2012 the Bino developers."));
        msg::req_txt(4, _("This is free software. You may redistribute copies of it "
                    "under the terms of the GNU General Public License. "
                    "There is NO WARRANTY, to the extent permitted by law."));
        msg::req(_("Platform:"));
        msg::req(4, "%s", PLATFORM);
        msg::req(_("Libraries used:"));
        std::vector<std::string> v = lib_versions(false);
        for (size_t i = 0; i < v.size(); i++)
        {
            msg::req(4, "%s", v[i].c_str());
        }
    }
    if (help.value())
    {
        if (msg::file() == stderr)
            msg::set_file(stdout);
        /* TRANSLATORS: This is the --help text. Please keep the line length limited.
         * The --help output should look good with a line width of 80 characters. */
        msg::req_txt(std::string(_("Usage:")) + '\n'
                + "  " + program_name + ' ' + "[option...] [file...]" + "\n\n"
                + _("Options:") + '\n'
                + "  --help                   " + _("Print help.") + '\n'
                + "  --version                " + _("Print version.") + '\n'
                + "  -n|--no-gui              " + _("Do not use the GUI, just show a plain window.") + '\n'
                + "  --log-file=FILE          " + _("Append all log messages to the given file.") + '\n'
                + "  -L|--log-level=LEVEL     " + _("Set log level (debug/info/warning/error/quiet).") + '\n'
                + "  --list-audio-devices     " + _("Print a list of known audio devices and exit.") + '\n'
                + "  -A|--audio-device=N      " + _("Use audio device number N (N=0 is the default).") + '\n'
                + "  -D|--audio-delay=D       " + _("Delay audio by D milliseconds. Default is 0.") + '\n'
                + "  -V|--audio-volume=V      " + _("Set audio volume (0 to 1). Default is 1.") + '\n'
                + "  -m|--audio-mute          " + _("Mute audio.") + '\n'
                + "  --device-type=TYPE       " + _("Type of input device: default, firewire, x11.") + '\n'
                + "  --device-frame-size=WxH  " + _("Request frame size WxH from input device.") + '\n'
                + "  --device-frame-rate=N/D  " + _("Request frame rate N/D from input device.") + '\n'
                + "  --lirc-config=FILE       " + _("Use the given LIRC configuration file.") + '\n'
                + "                           " + _("This option can be used more than once.") + '\n'
                + "  -v|--video=STREAM        " + _("Select video stream (1-n, depending on input).") + '\n'
                + "  -a|--audio=STREAM        " + _("Select audio stream (1-n, depending on input).") + '\n'
                + "  -s|--subtitle=STREAM     " + _("Select subtitle stream (0-n, dep. on input).") + '\n'
                + "  -i|--input=TYPE          " + _("Select input type (default autodetect):") + '\n'
                + "    mono                     " + _("Single view.") + '\n'
                + "    separate-left-right      " + _("Left/right separate streams, left first.") + '\n'
                + "    separate-right-left      " + _("Left/right separate streams, right first.") + '\n'
                + "    alternating-left-right   " + _("Left/right alternating, left first.") + '\n'
                + "    alternating-right-left   " + _("Left/right alternating, right first.") + '\n'
                + "    top-bottom               " + _("Left top, right bottom.") + '\n'
                + "    top-bottom-half          " + _("Left top, right bottom, half height.") + '\n'
                + "    bottom-top               " + _("Left bottom, right top.") + '\n'
                + "    bottom-top-half          " + _("Left bottom, right top, half height.") + '\n'
                + "    left-right               " + _("Left left, right right.") + '\n'
                + "    left-right-half          " + _("Left left, right right, half width.") + '\n'
                + "    right-left               " + _("Left right, right left.") + '\n'
                + "    right-left-half          " + _("Left right, right left, half width.") + '\n'
                + "    even-odd-rows            " + _("Left even rows, right odd rows.") + '\n'
                + "    odd-even-rows            " + _("Left odd rows, right even rows.") + '\n'
                + "  -o|--output=TYPE         " + _("Select output type:") + '\n'
                + "    mono-left                " + _("Only left.") + '\n'
                + "    mono-right               " + _("Only right.") + '\n'
                + "    top-bottom               " + _("Left top, right bottom.") + '\n'
                + "    top-bottom-half          " + _("Left top, right bottom, half height.") + '\n'
                + "    left-right               " + _("Left left, right right.") + '\n'
                + "    left-right-half          " + _("Left left, right right, half width.") + '\n'
                + "    even-odd-rows            " + _("Left even rows, right odd rows.") + '\n'
                + "    even-odd-columns         " + _("Left even columns, right odd columns.") + '\n'
                + "    checkerboard             " + _("Left and right in checkerboard pattern.") + '\n'
                + "    hdmi-frame-pack          " + _("HDMI frame packing mode.") + '\n'
                + "    red-cyan-monochrome      " + _("Red/cyan anaglyph, monochrome method.") + '\n'
                + "    red-cyan-half-color      " + _("Red/cyan anaglyph, half color method.") + '\n'
                + "    red-cyan-full-color      " + _("Red/cyan anaglyph, full color method.") + '\n'
                + "    red-cyan-dubois          " + _("Red/cyan anaglyph, Dubois method.") + '\n'
                + "    green-magenta-monochrome " + _("Green/magenta anaglyph, monochrome method.") + '\n'
                + "    green-magenta-half-color " + _("Green/magenta anaglyph, half color method.") + '\n'
                + "    green-magenta-full-color " + _("Green/magenta anaglyph, full color method.") + '\n'
                + "    green-magenta-dubois     " + _("Green/magenta anaglyph, Dubois method.") + '\n'
                + "    amber-blue-monochrome    " + _("Amber/blue anaglyph, monochrome method.") + '\n'
                + "    amber-blue-half-color    " + _("Amber/blue anaglyph, half color method.") + '\n'
                + "    amber-blue-full-color    " + _("Amber/blue anaglyph, full color method.") + '\n'
                + "    amber-blue-dubois        " + _("Amber/blue anaglyph, Dubois method.") + '\n'
                + "    red-green-monochrome     " + _("Red/green anaglyph, monochrome method.") + '\n'
                + "    red-blue-monochrome      " + _("Red/blue anaglyph, monochrome method.") + '\n'
                + "    stereo                   " + _("OpenGL quad-buffered stereo.") + '\n'
                + "    equalizer                " + _("Multi-display via Equalizer (2D setup).") + '\n'
                + "    equalizer-3d             " + _("Multi-display via Equalizer (3D setup).") + '\n'
                + "  -S|--swap-eyes           " + _("Swap left/right view.") + '\n'
                + "  -f|--fullscreen          " + _("Fullscreen.") + '\n'
                + "  --fullscreen-screens=    " + _("Use the listed screens in fullscreen mode.") + '\n'
                + "     [S0[,S1[,...]]]       " + _("Screen numbers start with 1.") + '\n'
                + "                           " + _("An empty list defaults to the primary screen.") + '\n'
                + "  --fullscreen-flip-left   " + _("Flip left view vertically when fullscreen.") + '\n'
                + "  --fullscreen-flop-left   " + _("Flop left view horizontally when fullscreen.") + '\n'
                + "  --fullscreen-flip-right  " + _("Flip right view vertically when fullscreen.") + '\n'
                + "  --fullscreen-flop-right  " + _("Flop right view horizontally when fullscreen.") + '\n'
                + "  -z|--zoom=Z              " + _("Set zoom for wide videos (0=off to 1=full).") + '\n'
                + "  -C|--crop=W:H            " + _("Crop video to given aspect ratio (0:0=off).") + '\n'
                + "  -c|--center              " + _("Center window on screen.") + '\n'
                + "  --subtitle-encoding=ENC  " + _("Set subtitle encoding.") + '\n'
                + "  --subtitle-font=FONT     " + _("Set subtitle font name.") + '\n'
                + "  --subtitle-size=N        " + _("Set subtitle font size.") + '\n'
                + "  --subtitle-scale=S       " + _("Set subtitle scale factor.") + '\n'
                + "  --subtitle-color=COLOR   " + _("Set subtitle color, in [AA]RRGGBB format.") + '\n'
                + "  --subtitle-parallax=VAL  " + _("Subtitle parallax adjustment (-1 to +1).") + '\n'
                + "  -P|--parallax=VAL        " + _("Parallax adjustment (-1 to +1).") + '\n'
                + "  --crosstalk=VAL          " + _("Crosstalk leak level (0 to 1).") + '\n'
                + "                           " + _("Comma-separated values for R,G,B.") + '\n'
                + "  --ghostbust=VAL          " + _("Amount of ghostbusting to apply (0 to 1).") + '\n'
                + "  -b|--benchmark           " + _("Benchmark mode (no audio, show fps).") + '\n'
                + "  --swap-interval=D        " + _("Frame rate divisor for display refresh rate.") + '\n'
                + "                           " + _("Default is 0 for benchmark mode, 1 otherwise.") + '\n'
                + "  -l|--loop                " + _("Loop the input media.") + '\n'
                + '\n'
                + _("Interactive control:") + '\n'
                + "  ESC                      " + _("Leave fullscreen mode, or quit.") + '\n'
                + "  q                        " + _("Quit.") + '\n'
                + "  p or SPACE               " + _("Pause / unpause.") + '\n'
                + "  f                        " + _("Toggle fullscreen.") + '\n'
                + "  c                        " + _("Center window.") + '\n'
                + "  e                        " + _("Swap left/right eye.") + '\n'
                + "  v                        " + _("Cycle through available video streams.") + '\n'
                + "  a                        " + _("Cycle through available audio streams.") + '\n'
                + "  s                        " + _("Cycle through available subtitle streams.") + '\n'
                + "  1, 2                     " + _("Adjust contrast.") + '\n'
                + "  3, 4                     " + _("Adjust brightness.") + '\n'
                + "  5, 6                     " + _("Adjust hue.") + '\n'
                + "  7, 8                     " + _("Adjust saturation.") + '\n'
                + "  [, ]                     " + _("Adjust parallax.") + '\n'
                + "  (, )                     " + _("Adjust ghostbusting.") + '\n'
                + "  <, >                     " + _("Adjust zoom for wide videos.") + '\n'
                + "  /, *                     " + _("Adjust audio volume.") + '\n'
                + "  m                        " + _("Toggle audio mute.") + '\n'
                + "  left, right              " + _("Seek 10 seconds backward / forward.") + '\n'
                + "  down, up                 " + _("Seek 1 minute backward / forward.") + '\n'
                + "  page down, page up       " + _("Seek 10 minutes backward / forward.") + '\n'
                + "  Mouse click              " + _("Seek according to horizontal click position.") + '\n'
                + "  Media keys               " + _("Media keys should work as expected."));
    }
    if (list_audio_devices.value())
    {
        audio_output ao;
        if (ao.devices() == 0)
        {
            msg::req(_("No audio devices known."));
        }
        else
        {
            msg::req("%d audio devices available:", ao.devices());
            for (int i = 0; i < ao.devices(); i++)
            {
                msg::req(4, "%d: %s", i + 1, ao.device_name(i).c_str());
            }
        }
    }
    if (version.value() || help.value() || list_audio_devices.value())
    {
        return 0;
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

    /* Find invariant parameters, required for dispatch initialization */
    bool dispatch_equalizer = false;
    bool dispatch_equalizer_3d = false;
    bool dispatch_gui = !no_gui.value();
    if (audio_device.is_set())
        controller::send_cmd(command::set_audio_device, audio_device.value() - 1);
    if (video_output_mode.value() == "equalizer") {
        dispatch_equalizer = true;
        dispatch_equalizer_3d = false;
        dispatch_gui = false;
    } else if (video_output_mode.value() == "equalizer-3d") {
        dispatch_equalizer = true;
        dispatch_equalizer_3d = true;
        dispatch_gui = false;
    }
    msg::level_t dispatch_log_level = msg::level();
    if (log_level.value() == "")
        dispatch_log_level = msg::INF;
    else if (log_level.value() == "debug")
        dispatch_log_level = msg::DBG;
    else if (log_level.value() == "info")
        dispatch_log_level = msg::INF;
    else if (log_level.value() == "warning")
        dispatch_log_level = msg::WRN;
    else if (log_level.value() == "error")
        dispatch_log_level = msg::ERR;
    else if (log_level.value() == "quiet")
        dispatch_log_level = msg::REQ;
    bool dispatch_benchmark = benchmark.value();
    int dispatch_swap_interval = dispatch_benchmark ? 0 : 1;
    if (swap_interval.is_set())
        dispatch_swap_interval = swap_interval.value();

    /* Create the central dispatch */
    dispatch global_dispatch(&argc, argv,
            dispatch_equalizer, dispatch_equalizer_3d, false,
            dispatch_gui, have_display, dispatch_log_level,
            dispatch_benchmark, dispatch_swap_interval);
    if (dispatch_benchmark)
        msg::inf(_("Benchmark mode: audio and time synchronization disabled."));

    /* Set session parameters */
    if (audio_device.is_set())
        controller::send_cmd(command::set_audio_device, audio_device.value() - 1);
    if (video_output_mode.is_set()) {
        if (video_output_mode.value() == "equalizer") {
            controller::send_cmd(command::set_stereo_mode, static_cast<int>(parameters::mode_mono_left));
            controller::send_cmd(command::set_stereo_mode_swap, false);
        } else if (video_output_mode.value() == "equalizer-3d") {
            controller::send_cmd(command::set_stereo_mode, static_cast<int>(parameters::mode_mono_left));
            controller::send_cmd(command::set_stereo_mode_swap, false);
        } else {
            parameters::stereo_mode_t stereo_mode;
            bool stereo_mode_swap;
            parameters::stereo_mode_from_string(video_output_mode.value(), stereo_mode, stereo_mode_swap);
            controller::send_cmd(command::set_stereo_mode, static_cast<int>(stereo_mode));
            controller::send_cmd(command::set_stereo_mode_swap, stereo_mode_swap);
        }
    }
    if (swap_eyes.is_set())
        controller::send_cmd(command::set_stereo_mode_swap, swap_eyes.value());
    if (crosstalk.is_set()) {
        std::ostringstream v;
        s11n::save(v, crosstalk.value()[0]);
        s11n::save(v, crosstalk.value()[1]);
        s11n::save(v, crosstalk.value()[2]);
        controller::send_cmd(command::set_crosstalk, v.str());
    }
    if (fullscreen_screens.is_set()) {
        int fs = 0;
        for (size_t i = 0; i < fullscreen_screens.value().size(); i++)
            fs |= (1 << (fullscreen_screens.value()[i] - 1));
        controller::send_cmd(command::set_fullscreen_screens, fs);
    }
    if (fullscreen_flip_left.is_set())
        controller::send_cmd(command::set_fullscreen_flip_left, fullscreen_flip_left.value());
    if (fullscreen_flop_left.is_set())
        controller::send_cmd(command::set_fullscreen_flop_left, fullscreen_flop_left.value());
    if (fullscreen_flip_right.is_set())
        controller::send_cmd(command::set_fullscreen_flip_right, fullscreen_flip_right.value());
    if (fullscreen_flop_right.is_set())
        controller::send_cmd(command::set_fullscreen_flop_right, fullscreen_flop_right.value());
    if (zoom.is_set())
        controller::send_cmd(command::set_zoom, zoom.value());
    if (loop.is_set())
        controller::send_cmd(command::set_loop_mode, loop.value() ? parameters::loop_current : parameters::no_loop);
    if (audio_delay.is_set())
        controller::send_cmd(command::set_audio_delay, audio_delay.value() * 1000);
    if (subtitle_encoding.is_set())
        controller::send_cmd(command::set_subtitle_encoding, subtitle_encoding.value());
    if (subtitle_font.is_set())
        controller::send_cmd(command::set_subtitle_font, subtitle_font.value());
    if (subtitle_size.is_set())
        controller::send_cmd(command::set_subtitle_size, subtitle_size.value());
    if (subtitle_scale.is_set())
        controller::send_cmd(command::set_subtitle_scale, subtitle_scale.value());
    if (subtitle_color.is_set())
        controller::send_cmd(command::set_subtitle_color, static_cast<uint64_t>(subtitle_color.value()));

    /* Set volatile parameters */
    if (fullscreen.is_set() && fullscreen.value())
        controller::send_cmd(command::toggle_fullscreen);
    if (center.is_set())
        controller::send_cmd(command::center);
    if (audio_volume.is_set())
        controller::send_cmd(command::set_audio_volume, audio_volume.value());
    if (audio_mute.is_set() && audio_mute.value())
        controller::send_cmd(command::toggle_audio_mute);

    /* Gather initial player data: input URLs and per-video parameters */
    open_input_data input_data;
    if (device_type.value() == "") {
        input_data.dev_request.device =
            (arguments.size() == 1 && arguments[0].substr(0, 5) == "/dev/"
             ? device_request::sys_default : device_request::no_device);
    } else {
        input_data.dev_request.device =
            (device_type.value() == "firewire" ? device_request::firewire
             : device_type.value() == "x11" ? device_request::x11
             : device_request::sys_default);
    }
    input_data.dev_request.width = device_frame_size.value()[0];
    input_data.dev_request.height = device_frame_size.value()[1];
    input_data.dev_request.frame_rate_num = device_frame_rate.value()[0];
    input_data.dev_request.frame_rate_den = device_frame_rate.value()[1];
    input_data.urls = arguments;
    if (video.is_set() > 0)
        input_data.params.set_video_stream(video.value() - 1);
    if (audio.is_set() > 0)
        input_data.params.set_audio_stream(audio.value() - 1);
    if (subtitle.is_set() > 0)
        input_data.params.set_subtitle_stream(subtitle.value() - 1);
    if (input_mode.is_set()) {
        parameters::stereo_layout_t stereo_layout;
        bool stereo_layout_swap;
        parameters::stereo_layout_from_string(input_mode.value(), stereo_layout, stereo_layout_swap);
        input_data.params.set_stereo_layout(stereo_layout);
        input_data.params.set_stereo_layout_swap(stereo_layout_swap);
    }
    if (crop_aspect_ratio.is_set()) {
        float crop_ar = 0.0f;
        if (crop_aspect_ratio.value()[0] > 0.0f && crop_aspect_ratio.value()[1] > 0.0f) {
            crop_ar = crop_aspect_ratio.value()[0] / crop_aspect_ratio.value()[1];
            crop_ar = std::min(std::max(crop_ar, 1.0f), 2.39f);
        }
        input_data.params.set_crop_aspect_ratio(crop_ar);
    }
    if (parallax.is_set())
        input_data.params.set_parallax(parallax.value());
    if (ghostbust.is_set())
        input_data.params.set_ghostbust(ghostbust.value());
    if (subtitle_parallax.is_set())
        input_data.params.set_subtitle_parallax(subtitle_parallax.value());

    int retval = 0;
    try {
        global_dispatch.init();
        if (input_data.urls.size() > 0) {
            std::ostringstream v;
            s11n::save(v, input_data);
            controller::send_cmd(command::open, v.str());
            controller::send_cmd(command::toggle_play);
        } else if (!dispatch_gui) {
            throw exc(_("No video to play."));
        }
#if HAVE_LIBLIRCCLIENT
        lircclient lirc(PACKAGE, lirc_config.values());
        try {
            lirc.init();
        }
        catch (std::exception& e) {
            msg::wrn("%s", e.what());
        }
#else
        if (lirc_config.is_set()) {
            msg::wrn(_("This version of Bino was compiled without support for LIRC."));
        }
#endif
        if (dispatch_equalizer) {
#if HAVE_LIBEQUALIZER
            player_equalizer::mainloop();
#else
            msg::err(_("This version of Bino was compiled without support for Equalizer."));
#endif
        } else {
            QApplication::exec();
        }
#if HAVE_LIBLIRCCLIENT
        lirc.deinit();
#endif
        global_dispatch.deinit();
    }
    catch (std::exception& e) {
        msg::err("%s", e.what());
        retval = 1;
    }

    delete qt_app;

    return retval;
}
