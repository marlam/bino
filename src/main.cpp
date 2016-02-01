/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013, 2014, 2015, 2016
 * Martin Lambers <marlam@marlam.de>
 * Stefan Eilemann <eile@eyescale.ch>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
 * Joe <cuchac@email.cz>
 * Binocle <http://binocle.com> (author: Olivier Letz <oletz@binocle.com>)
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

#include "base/dbg.h"
#include "base/msg.h"
#include "base/str.h"
#include "base/opt.h"

#include "base/gettext.h"
#define _(string) gettext(string)

#include "dispatch.h"
#include "audio_output.h"
#if HAVE_LIBEQUALIZER
# include "player_equalizer.h"
#endif
#include "command_file.h"
#if HAVE_LIBLIRCCLIENT
# include "lirc.h"
#endif
#include "lib_versions.h"

#if HAVE_LIBXNVCTRL
#include "NVCtrl.h"
#endif // HAVE_LIBXNVCTRL


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

#if QT_VERSION < 0x050000
static void qt_msg_handler(QtMsgType type, const char *msg)
#else
static void qt_msg_handler(QtMsgType type, const QMessageLogContext&, const QString& msg)
#endif
{
#if QT_VERSION < 0x050000
    std::string s = msg;
#else
    std::string s = qPrintable(msg);
#endif
    switch (type)
    {
    case QtDebugMsg:
        msg::dbg(str::sanitize(s));
        break;
    case QtWarningMsg:
        msg::wrn(str::sanitize(s));
        break;
    case QtCriticalMsg:
        msg::err(str::sanitize(s));
        break;
    case QtFatalMsg:
    default:
        msg::err(str::sanitize(s));
        std::exit(1);
    }
}

// Handle a log file that may be set via the --log-file option; see below
static FILE *logfile = NULL;
static void close_log_file(void)
{
    if (logfile)
    {
        (void)std::fclose(logfile);
    }
}

static std::string lengthen(const std::string& s, int display_width)
{
    int spaces = display_width - static_cast<int>(str::display_width(s));
    if (spaces > 0)
        return s + std::string(spaces, ' ');
    else
        return s;
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
    QCoreApplication::setAttribute(Qt::AA_X11InitThreads);
#ifdef Q_WS_X11
    // This only works with Qt4; Qt5 ignores the 'have_display' flag.
    const char *display = getenv("DISPLAY");
    bool have_display = (display && display[0] != '\0');
#else
    bool have_display = true;
#endif
#if QT_VERSION < 0x050000
    qInstallMsgHandler(qt_msg_handler);
#else
    qInstallMessageHandler(qt_msg_handler);
#endif
    QApplication *qt_app = new QApplication(argc, argv, have_display);
#if QT_VERSION < 0x050000
    // Make Qt4 behave like Qt5: always interpret all C strings as UTF-8.
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
#endif
    // Qt resets locale information in the QApplication constructor. Repair that.
    setlocale(LC_ALL, "");
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
    opt::val<int> audio_delay("audio-delay", 'D', opt::optional, -100000, +100000, 0);
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
    std::vector<std::string> device_formats;
    device_formats.push_back("default");
    device_formats.push_back("mjpeg");
    opt::val<std::string> device_format("device-format", '\0', opt::optional, device_formats, "");
    options.push_back(&device_format);
    opt::val<std::string> read_commands("read-commands", '\0', opt::optional);
    options.push_back(&read_commands);
    opt::val<std::string> lirc_config("lirc-config", '\0', opt::optional);
    options.push_back(&lirc_config);
    std::vector<std::string> input_modes;
    opt::val<int> quality("quality", '\0', opt::optional, 0, 4, 4);
    options.push_back(&quality);
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
    opt::val<std::string> video_output_mode("output", 'o', opt::optional);
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
    opt::flag fullscreen_3d_ready_sync("fullscreen-3dr-sync", '\0', opt::optional);
    options.push_back(&fullscreen_3d_ready_sync);
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
    opt::val<int> subtitle_shadow("subtitle-shadow", '\0', opt::optional, -1, 1);
    options.push_back(&subtitle_shadow);
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
#if HAVE_LIBXNVCTRL
    opt::val<int> sdi_output_format("sdi-output-format", '\0', opt::optional,
            NV_CTRL_GVIO_VIDEO_FORMAT_487I_59_94_SMPTE259_NTSC,
            NV_CTRL_GVIO_VIDEO_FORMAT_2048I_47_96_SMPTE372);
    options.push_back(&sdi_output_format);
#endif // HAVE_LIBXNVCTRL
    // Accept some Equalizer options. These are passed to Equalizer for interpretation.
    opt::flag eq_help("eq-help", '\0', opt::optional);
    options.push_back(&eq_help);
    opt::val<std::string> eq_layout("eq-layout", '\0', opt::optional);
    options.push_back(&eq_layout);
    opt::val<std::string> eq_gpufilter("eq-gpufilter", '\0', opt::optional);
    options.push_back(&eq_gpufilter);
    opt::val<std::string> eq_modelunit("eq-modelunit", '\0', opt::optional);
    options.push_back(&eq_modelunit);
    opt::val<std::string> eq_logfile("eq-logfile", '\0', opt::optional);
    options.push_back(&eq_logfile);
    opt::val<std::string> eq_server("eq-server", '\0', opt::optional);
    options.push_back(&eq_server);
    opt::val<std::string> eq_config("eq-config", '\0', opt::optional);
    options.push_back(&eq_config);
    opt::val<std::string> eq_config_flags("eq-config-flags", '\0', opt::optional);
    options.push_back(&eq_config_flags);
    opt::val<std::string> eq_config_prefixes("eq-config-prefixes", '\0', opt::optional);
    options.push_back(&eq_config_prefixes);
    opt::val<std::string> eq_render_client("eq-render-client", '\0', opt::optional);
    options.push_back(&eq_render_client);
    opt::val<std::string> eq_listen("eq-listen", '\0', opt::optional);
    options.push_back(&eq_listen);
    opt::val<std::string> co_listen("co-listen", '\0', opt::optional);
    options.push_back(&co_listen);

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
        logfile = std::fopen(log_file.value().c_str(), "a");
        if (!logfile)
        {
            msg::err(_("%s: %s"), log_file.value().c_str(), std::strerror(errno));
            return 1;
        }
        std::atexit(close_log_file);
        msg::set_file(logfile);
        msg::set_columns(80);
    }

    if (version.value())
    {
        if (msg::file() == stderr)
            msg::set_file(stdout);
        msg::req(_("%s version %s"), PACKAGE_NAME, VERSION);
        msg::req(4, _("Copyright (C) 2016 the Bino developers."));
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
                + "  --help                   " + _("Print help") + '\n'
                + "  --version                " + _("Print version") + '\n'
                + "  -n|--no-gui              " + _("Do not use the GUI, just show a plain window") + '\n'
                + "  --log-file=FILE          " + _("Append all log messages to the given file") + '\n'
                + "  -L|--log-level=LEVEL     " + _("Set log level (debug/info/warning/error/quiet)") + '\n'
                + "  --list-audio-devices     " + _("Print a list of known audio devices and exit") + '\n'
                + "  -A|--audio-device=N      " + _("Use audio device number N (N=0 is the default)") + '\n'
                + "  -D|--audio-delay=D       " + _("Delay audio by D milliseconds (default 0)") + '\n'
                + "  -V|--audio-volume=V      " + _("Set audio volume (0 to 1, default 1)") + '\n'
                + "  -m|--audio-mute          " + _("Mute audio") + '\n'
                + "  --device-type=TYPE       " + _("Type of input device: default, firewire, x11") + '\n'
                + "  --device-frame-size=WxH  " + _("Request frame size WxH from input device") + '\n'
                + "  --device-frame-rate=N/D  " + _("Request frame rate N/D from input device") + '\n'
                + "  --device-format=FORMAT   " + _("Request device format 'default' or 'mjpeg'") + '\n'
                + "  --read-commands=FILE     " + _("Read commands from a file") + '\n'
                + "  --lirc-config=FILE       " + _("Use the given LIRC configuration file") + '\n'
                + "                           " + _("This option can be used more than once") + '\n'
                + "  --quality=Q              " + _("Output quality (0=fastest to 4=best/default)") + '\n'
                + "  -v|--video=STREAM        " + _("Select video stream (1-n, depending on input)") + '\n'
                + "  -a|--audio=STREAM        " + _("Select audio stream (1-n, depending on input)") + '\n'
                + "  -s|--subtitle=STREAM     " + _("Select subtitle stream (0-n, dep. on input)") + '\n'
                + "  -i|--input=TYPE          " + _("Select input type (default autodetect):") + '\n'
                + "    mono                     " + _("2D") + '\n'
                + "    separate-left-right      " + _("Separate streams, left first") + '\n'
                + "    separate-right-left      " + _("Separate streams, right first") + '\n'
                + "    alternating-left-right   " + _("Alternating, left first") + '\n'
                + "    alternating-right-left   " + _("Alternating, right first") + '\n'
                + "    top-bottom               " + _("Top/bottom") + '\n'
                + "    top-bottom-half          " + _("Top/bottom, half height") + '\n'
                + "    bottom-top               " + _("Bottom/top") + '\n'
                + "    bottom-top-half          " + _("Bottom/top, half height") + '\n'
                + "    left-right               " + _("Left/right") + '\n'
                + "    left-right-half          " + _("Left/right, half width") + '\n'
                + "    right-left               " + _("Right/left") + '\n'
                + "    right-left-half          " + _("Right/left, half width") + '\n'
                + "    even-odd-rows            " + _("Even/odd rows") + '\n'
                + "    odd-even-rows            " + _("Odd/even rows") + '\n'
                + "  -o|--output=TYPE         " + _("Select output type:") + '\n'
                + "    stereo                   " + _("OpenGL stereo") + '\n'
                + "    alternating              " + _("Left/right alternating") + '\n'
                + "    mono-left                " + _("Left view") + '\n'
                + "    mono-right               " + _("Right view") + '\n'
                + "    top-bottom               " + _("Top/bottom") + '\n'
                + "    top-bottom-half          " + _("Top/bottom, half height") + '\n'
                + "    left-right               " + _("Left/right") + '\n'
                + "    left-right-half          " + _("Left/right, half width") + '\n'
                + "    even-odd-rows            " + _("Even/odd rows") + '\n'
                + "    even-odd-columns         " + _("Even/odd columns") + '\n'
                + "    checkerboard             " + _("Checkerboard pattern") + '\n'
                + "    hdmi-frame-pack          " + _("HDMI frame packing mode") + '\n'
                + "    red-cyan-monochrome      " + _("Red/cyan glasses, monochrome") + '\n'
                + "    red-cyan-half-color      " + _("Red/cyan glasses, half color") + '\n'
                + "    red-cyan-full-color      " + _("Red/cyan glasses, full color") + '\n'
                + "    red-cyan-dubois          " + _("Red/cyan glasses, Dubois") + '\n'
                + "    green-magenta-monochrome " + _("Green/magenta glasses, monochrome") + '\n'
                + "    green-magenta-half-color " + _("Green/magenta glasses, half color") + '\n'
                + "    green-magenta-full-color " + _("Green/magenta glasses, full color") + '\n'
                + "    green-magenta-dubois     " + _("Green/magenta glasses, Dubois") + '\n'
                + "    amber-blue-monochrome    " + _("Amber/blue glasses, monochrome") + '\n'
                + "    amber-blue-half-color    " + _("Amber/blue glasses, half color") + '\n'
                + "    amber-blue-full-color    " + _("Amber/blue glasses, full color") + '\n'
                + "    amber-blue-dubois        " + _("Amber/blue glasses, Dubois") + '\n'
                + "    red-green-monochrome     " + _("Red/green glasses, monochrome") + '\n'
                + "    red-blue-monochrome      " + _("Red/blue glasses, monochrome") + '\n'
                + "    equalizer                " + _("Multi-display via Equalizer (2D setup)") + '\n'
                + "    equalizer-3d             " + _("Multi-display via Equalizer (3D setup)") + '\n'
                + "  -S|--swap-eyes           " + _("Swap left/right view") + '\n'
                + "  -f|--fullscreen          " + _("Fullscreen") + '\n'
                + "  --fullscreen-screens=    " + _("Use the listed screens in fullscreen mode") + '\n'
                + "     [S0[,S1[,...]]]       " + _("Screen numbers start with 1") + '\n'
                + "                           " + _("An empty list defaults to the primary screen") + '\n'
                + "  --fullscreen-flip-left   " + _("Flip left view vertically when fullscreen") + '\n'
                + "  --fullscreen-flop-left   " + _("Flop left view horizontally when fullscreen") + '\n'
                + "  --fullscreen-flip-right  " + _("Flip right view vertically when fullscreen") + '\n'
                + "  --fullscreen-flop-right  " + _("Flop right view horizontally when fullscreen") + '\n'
                + "  --fullscreen-3dr-sync    " + _("Use DLP 3-D Ready Sync when fullscreen") + '\n'
                + "  -z|--zoom=Z              " + _("Set zoom for wide videos (0=off to 1=full)") + '\n'
                + "  -C|--crop=W:H            " + _("Crop video to given aspect ratio (0:0=off)") + '\n'
                + "  -c|--center              " + _("Center window on screen") + '\n'
                + "  --subtitle-encoding=ENC  " + _("Set subtitle encoding") + '\n'
                + "  --subtitle-font=FONT     " + _("Set subtitle font name") + '\n'
                + "  --subtitle-size=N        " + _("Set subtitle font size") + '\n'
                + "  --subtitle-scale=S       " + _("Set subtitle scale factor") + '\n'
                + "  --subtitle-color=COLOR   " + _("Set subtitle color, in [AA]RRGGBB format") + '\n'
                + "  --subtitle-shadow=-1|0|1 " + _("Set subtitle shadow, -1=default, 0=off, 1=on") + '\n'
                + "  --subtitle-parallax=VAL  " + _("Subtitle parallax adjustment (-1 to +1)") + '\n'
                + "  -P|--parallax=VAL        " + _("Parallax adjustment (-1 to +1)") + '\n'
                + "  --crosstalk=VAL          " + _("Crosstalk leak level (0 to 1)") + '\n'
                + "                           " + _("Comma-separated values for R,G,B") + '\n'
                + "  --ghostbust=VAL          " + _("Amount of ghostbusting to apply (0 to 1)") + '\n'
                + "  -b|--benchmark           " + _("Benchmark mode (no audio, show fps)") + '\n'
                + "  --swap-interval=D        " + _("Frame rate divisor for display refresh rate") + '\n'
                + "                           " + _("Default is 0 for benchmark mode, 1 otherwise") + '\n'
                + "  -l|--loop                " + _("Loop the input media") + '\n'
                + "  --sdi-output-format=F    " + _("Set SDI output format") + '\n'
                + '\n'
                + _("Interactive control:") + '\n'
                + "  " + lengthen(_("ESC"), 25) + _("Leave fullscreen mode, or quit") + '\n'
                + "  q                        " + _("Quit") + '\n'
                + "  p / " + lengthen(_("SPACE"), 21) + _("Pause / unpause") + '\n'
                + "  f                        " + _("Toggle fullscreen") + '\n'
                + "  c                        " + _("Center window") + '\n'
                + "  e / F7                   " + _("Swap left/right eye") + '\n'
                + "  v                        " + _("Cycle through available video streams") + '\n'
                + "  a                        " + _("Cycle through available audio streams") + '\n'
                + "  s                        " + _("Cycle through available subtitle streams") + '\n'
                + "  1, 2                     " + _("Adjust contrast") + '\n'
                + "  3, 4                     " + _("Adjust brightness") + '\n'
                + "  5, 6                     " + _("Adjust hue") + '\n'
                + "  7, 8                     " + _("Adjust saturation") + '\n'
                + "  [, ]                     " + _("Adjust parallax") + '\n'
                + "  (, )                     " + _("Adjust ghostbusting") + '\n'
                + "  <, >                     " + _("Adjust zoom for wide videos") + '\n'
                + "  /, *                     " + _("Adjust audio volume") + '\n'
                + "  m                        " + _("Toggle audio mute") + '\n'
                + "  .                        " + _("Step a single video frame forward") + '\n'
                + "  " + lengthen(_("left, right"), 25)        + _("Seek 10 seconds backward / forward") + '\n'
                + "  " + lengthen(_("down, up"), 25)           + _("Seek 1 minute backward / forward") + '\n'
                + "  " + lengthen(_("page down, page up"), 25) + _("Seek 10 minutes backward / forward") + '\n'
                + "  " + lengthen(_("Mouse click"), 25)        + _("Seek according to horizontal click position") + '\n'
                + "  " + lengthen(_("Media keys"), 25)         + _("Media keys should work as expected"));
    }
    if (version.value() || help.value())
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

    /* List audio devices and exit, if requested */
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
        return 0;
    }

    /* Set session parameters */
    if (dispatch_benchmark)
        msg::inf(_("Benchmark mode: audio and time synchronization disabled."));
    if (audio_device.is_set())
        controller::send_cmd(command::set_audio_device, audio_device.value() - 1);
    if (quality.is_set())
        controller::send_cmd(command::set_quality, quality.value());
    if (video_output_mode.is_set()) {
        parameters::stereo_mode_t stereo_mode;
        if (video_output_mode.value() == "equalizer") {
            controller::send_cmd(command::set_stereo_mode, static_cast<int>(parameters::mode_mono_left));
        } else if (video_output_mode.value() == "equalizer-3d") {
            controller::send_cmd(command::set_stereo_mode, static_cast<int>(parameters::mode_mono_left));
        } else if (parameters::parse_stereo_mode(video_output_mode.value(), &stereo_mode)) {
            controller::send_cmd(command::set_stereo_mode, static_cast<int>(stereo_mode));
        } else {
            msg::err(_("Unknown mode %s."), video_output_mode.value().c_str());
            return 1;
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
    if (fullscreen_3d_ready_sync.is_set())
        controller::send_cmd(command::set_fullscreen_3d_ready_sync, fullscreen_3d_ready_sync.value());
    if (zoom.is_set())
        controller::send_cmd(command::set_zoom, zoom.value());
    if (loop.is_set())
        controller::send_cmd(command::set_loop_mode, loop.value() ? parameters::loop_current : parameters::no_loop);
    if (audio_delay.is_set())
        controller::send_cmd(command::set_audio_delay, static_cast<int64_t>(audio_delay.value() * 1000));
    if (subtitle_encoding.is_set()) {
        std::ostringstream v;
        s11n::save(v, subtitle_encoding.value());
        controller::send_cmd(command::set_subtitle_encoding, v.str());
    }
    if (subtitle_font.is_set()) {
        std::ostringstream v;
        s11n::save(v, subtitle_font.value());
        controller::send_cmd(command::set_subtitle_font, v.str());
    }
    if (subtitle_size.is_set())
        controller::send_cmd(command::set_subtitle_size, subtitle_size.value());
    if (subtitle_scale.is_set())
        controller::send_cmd(command::set_subtitle_scale, subtitle_scale.value());
    if (subtitle_color.is_set())
        controller::send_cmd(command::set_subtitle_color, static_cast<uint64_t>(subtitle_color.value()));
    if (subtitle_shadow.is_set())
        controller::send_cmd(command::set_subtitle_shadow, subtitle_shadow.value());
#if HAVE_LIBXNVCTRL
    if (sdi_output_format.is_set())
        controller::send_cmd(command::set_sdi_output_format, sdi_output_format.value());
#endif // HAVE_LIBXNVCTRL

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
            (arguments.size() >= 1 && arguments[0].substr(0, 5) == "/dev/"
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
    if (device_format.value() == "mjpeg")
        input_data.dev_request.request_mjpeg = true;
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
    std::vector<command_file*> command_files;
    try {
        for (size_t i = 0; i < read_commands.values().size(); i++) {
            command_files.push_back(new command_file(read_commands.values()[i]));
            command_files[i]->init();
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
        global_dispatch.init(input_data);
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

    for (size_t i = 0; i < command_files.size(); i++)
        delete command_files[i];
    delete qt_app;

    return retval;
}
