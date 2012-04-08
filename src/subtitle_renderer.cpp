/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2011, 2012
 * Martin Lambers <marlam@marlam.de>
 * Joe <cuchac@email.cz>
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

#include <vector>
#include <limits>
#include <cstring>
#include <cstdio>
#include <stdint.h>

#include <GL/glew.h>

#if (defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__
# include <windows.h>
#endif
#if defined __APPLE__
# include <unistd.h>
#endif

extern "C"
{
#include <ass/ass.h>
}

#include "gettext.h"
#define _(string) gettext(string)

#include "dbg.h"
#include "exc.h"
#include "str.h"
#include "blob.h"
#include "msg.h"
#include "thread.h"

#include "subtitle_renderer.h"


subtitle_renderer_initializer::subtitle_renderer_initializer(subtitle_renderer &renderer) :
    _subtitle_renderer(renderer)
{
}

void subtitle_renderer_initializer::run()
{
    _subtitle_renderer.init();
}


/* Rendering subtitles with LibASS is not thread-safe.
 *
 * We have multiple concurrent subtitle rendering threads if we are running from Equalizer
 * and the Equalizer configuration contains multiple channels per node (since each channel
 * has a video output, which in turn has a subtitle renderer).
 *
 * Everything works fine as long as every channel is connected to the same X11 display.
 * But if channels are connected to different X11 displays, the application crashes.
 * The culprit here seems to be the freetype library, as this library causes the same
 * trouble in other applications that are not related to LibASS.
 *
 * Anyway, to fix this we use one big global lock around LibASS calls.
 */
static mutex global_libass_mutex;

subtitle_renderer::subtitle_renderer() :
    _initializer(*this),
    _initialized(false),
    _fontconfig_conffile(NULL),
    _ass_library(NULL),
    _ass_renderer(NULL),
    _ass_track(NULL)
{
    _initializer.start();
}

subtitle_renderer::~subtitle_renderer()
{
    if (_initializer.is_running())
    {
        try
        {
            _initializer.finish();
        }
        catch (...)
        {
        }
    }
    if (_ass_track)
    {
        ass_free_track(_ass_track);
    }
    if (_ass_renderer)
    {
        ass_renderer_done(_ass_renderer);
    }
    if (_ass_library)
    {
        ass_library_done(_ass_library);
    }
    if (_fontconfig_conffile)
    {
        (void)std::remove(_fontconfig_conffile);
    }
}

static void libass_msg_callback(int level, const char *fmt, va_list args, void *)
{
    msg::level_t msg_levels[8] = { msg::ERR, msg::ERR, msg::WRN, msg::WRN, msg::WRN, msg::INF, msg::DBG, msg::DBG };
    msg::level_t l = (level < 0 || level > 7 ? msg::ERR : msg_levels[level]);
    std::string s = str::vasprintf(fmt, args);
    msg::msg(l, std::string("LibASS: ") + s);
}

const char *subtitle_renderer::get_fontconfig_conffile()
{
#if ((defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__) || defined __APPLE__
    /* Fontconfig annoyingly requires a configuration file, but there is no
     * default location for it on Windows or Mac OS X. So we just create a temporary one.
     * Note that if something goes wrong and we return NULL here, the application will
     * likely crash when trying to render a subtitle. */
    FILE *f;
# if ((defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__)
    /* Note that due to lack of mkstemp() on Windows, this code has race conditions. */
    char tmppathname[MAX_PATH];
    static char tmpfilename[MAX_PATH];
    DWORD ret;
    ret = GetTempPath(MAX_PATH, tmppathname);
    if (ret > MAX_PATH || ret == 0)
        return NULL;
    if (GetTempFileName(tmppathname, "fonts_conf", 0, tmpfilename) == 0)
        return NULL;
    if (!(f = fopen(tmpfilename, "w")))
        return NULL;
# else /* __APPLE__ */
    static char tmpfilename[] = "/tmp/fontsXXXXXX.conf";
    int d = mkstemps(tmpfilename, 5);
    if (d == -1)
        return NULL;
    if (!(f = fdopen(d, "w")))
        return NULL;
# endif
    fprintf(f,
            "<?xml version=\"1.0\"?>\n"
            "<!DOCTYPE fontconfig SYSTEM \"fonts.dtd\">\n"
            "<fontconfig>\n"
# if ((defined _WIN32 || defined __WIN32__) && !defined __CYGWIN__)
            "<dir>WINDOWSFONTDIR</dir>\n"
            "<dir>~/.fonts</dir>\n"
            "<cachedir>WINDOWSTEMPDIR_FONTCONFIG_CACHE</cachedir>\n"
            "<cachedir>~/.fontconfig</cachedir>\n"
# else /* __APPLE__ */
            "<dir>/usr/share/fonts</dir>\n"
            "<dir>/usr/X11/lib/X11/fonts</dir>\n"
            "<dir>/usr/X11/share/fonts</dir>\n"
            "<dir>/opt/X11/share/fonts</dir>\n"
            "<dir>/Library/Fonts</dir>\n"
            "<dir>/Network/Library/Fonts</dir>\n"
            "<dir>/System/Library/Fonts</dir>\n"
            "<dir>~/Library/Application Support/Bino/fonts</dir>\n"
            "<cachedir>/var/cache/fontconfig</cachedir>\n"
            "<cachedir>/usr/X11/var/cache/fontconfig</cachedir>\n"
            "<cachedir>/opt/X11/var/cache/fontconfig</cachedir>\n"
            "<cachedir>~/Library/Application Support/Bino/cache/fonts</cachedir>\n"
            "<cachedir>~/.fontconfig</cachedir>\n"
# endif
            "<config>\n"
            "<blank>\n"
            "<int>0x0020</int> <int>0x00A0</int> <int>0x00AD</int> <int>0x034F</int> <int>0x0600</int>\n"
            "<int>0x0601</int> <int>0x0602</int> <int>0x0603</int> <int>0x06DD</int> <int>0x070F</int>\n"
            "<int>0x115F</int> <int>0x1160</int> <int>0x1680</int> <int>0x17B4</int> <int>0x17B5</int>\n"
            "<int>0x180E</int> <int>0x2000</int> <int>0x2001</int> <int>0x2002</int> <int>0x2003</int>\n"
            "<int>0x2004</int> <int>0x2005</int> <int>0x2006</int> <int>0x2007</int> <int>0x2008</int>\n"
            "<int>0x2009</int> <int>0x200A</int> <int>0x200B</int> <int>0x200C</int> <int>0x200D</int>\n"
            "<int>0x200E</int> <int>0x200F</int> <int>0x2028</int> <int>0x2029</int> <int>0x202A</int>\n"
            "<int>0x202B</int> <int>0x202C</int> <int>0x202D</int> <int>0x202E</int> <int>0x202F</int>\n"
            "<int>0x205F</int> <int>0x2060</int> <int>0x2061</int> <int>0x2062</int> <int>0x2063</int>\n"
            "<int>0x206A</int> <int>0x206B</int> <int>0x206C</int> <int>0x206D</int> <int>0x206E</int>\n"
            "<int>0x206F</int> <int>0x2800</int> <int>0x3000</int> <int>0x3164</int> <int>0xFEFF</int>\n"
            "<int>0xFFA0</int> <int>0xFFF9</int> <int>0xFFFA</int> <int>0xFFFB</int>\n"
            "</blank>\n"
            "<rescan><int>30</int></rescan>\n"
            "</config>\n"
            "</fontconfig>\n");
    fclose(f);
    return tmpfilename;
#else
    /* Systems other than Windows and Mac OS are expected to have a default
     * fontconfig configuration file so that we can simply return NULL. */
    return NULL;
#endif
}

void subtitle_renderer::init()
{
    if (!_initialized)
    {
        try
        {
            global_libass_mutex.lock();

            ASS_Library *ass_library = ass_library_init();
            if (!ass_library)
            {
                throw exc(_("Cannot initialize LibASS."));
            }
            ass_set_message_cb(ass_library, libass_msg_callback, NULL);
            ass_set_extract_fonts(ass_library, 1);
            _ass_library = ass_library;

            ASS_Renderer *ass_renderer = ass_renderer_init(_ass_library);
            if (!ass_renderer)
            {
                throw exc(_("Cannot initialize LibASS renderer."));
            }
            ass_set_hinting(ass_renderer, ASS_HINTING_NATIVE);
            _fontconfig_conffile = get_fontconfig_conffile();
            ass_set_fonts(ass_renderer, NULL, "sans-serif", 1, _fontconfig_conffile, 1);
            _ass_renderer = ass_renderer;

            _initialized = true;
            global_libass_mutex.unlock();
        }
        catch (...)
        {
            // Either we threw this exception ourselves, or the thread was cancelled.
            // Note that if the thread was cancelled, we likely leak ressources.
            // But at least we avoid to deconstruct potentially inconsistent ASS_Library
            // and ASS_Renderer objects.
            global_libass_mutex.unlock();
            throw;
        }
    }
}

bool subtitle_renderer::is_initialized()
{
    if (_initializer.is_running())
    {
        return false;
    }
    else
    {
        // rethrow a possible exception if something went wrong
        _initializer.finish();
        return _initialized;
    }
}

bool subtitle_renderer::render_to_display_size(const subtitle_box &box) const
{
    return (box.format != subtitle_box::image);
}

bool subtitle_renderer::prerender(const subtitle_box &box, int64_t timestamp,
        const parameters &params,
        int width, int height, float pixel_aspect_ratio,
        int &bb_x, int &bb_y, int &bb_w, int &bb_h)
{
    assert(_initialized);
    bool r = false;
    _fmt = box.format;
    switch (_fmt)
    {
    case subtitle_box::text:
    case subtitle_box::ass:
        r = prerender_ass(box, timestamp, params, width, height, pixel_aspect_ratio);
        break;
    case subtitle_box::image:
        r = prerender_img(box);
        break;
    }
    bb_x = _bb_x;
    bb_y = _bb_y;
    bb_w = _bb_w;
    bb_h = _bb_h;
    return r;
}

void subtitle_renderer::render(uint32_t *bgra32_buffer)
{
    switch (_fmt)
    {
    case subtitle_box::text:
    case subtitle_box::ass:
        render_ass(bgra32_buffer);
        break;
    case subtitle_box::image:
        render_img(bgra32_buffer);
        break;
    }
}

void subtitle_renderer::blend_ass_image(const ASS_Image *img, uint32_t *buf)
{
    const unsigned int R = (img->color >> 24u) & 0xffu;
    const unsigned int G = (img->color >> 16u) & 0xffu;
    const unsigned int B = (img->color >>  8u) & 0xffu;
    const unsigned int A = 255u - (img->color & 0xffu);

    unsigned char *src = img->bitmap;
    for (int src_y = 0; src_y < img->h; src_y++)
    {
        int dst_y = src_y + img->dst_y - _bb_y;
        if (dst_y >= _bb_h)
        {
            break;
        }
        for (int src_x = 0; src_x < img->w; src_x++)
        {
            unsigned int a = src[src_x] * A / 255u;
            int dst_x = src_x + img->dst_x - _bb_x;
            if (dst_x >= _bb_w)
            {
                break;
            }
            uint32_t oldval = buf[dst_y * _bb_w + dst_x];
            // XXX: The BGRA layout used here may be wrong on big endian system
            uint32_t newval = std::min(a + (oldval >> 24u), 255u) << 24u
                | ((a * R + (255u - a) * ((oldval >> 16u) & 0xffu)) / 255u) << 16u
                | ((a * G + (255u - a) * ((oldval >>  8u) & 0xffu)) / 255u) << 8u
                | ((a * B + (255u - a) * ((oldval       ) & 0xffu)) / 255u);
            buf[dst_y * _bb_w + dst_x] = newval;
        }
        src += img->stride;
    }
}

void subtitle_renderer::set_ass_parameters(const parameters &params)
{
    std::vector<std::string> overrides;
    if (!params.subtitle_font_is_default())
    {
        overrides.push_back(std::string("Default.Fontname=") + params.subtitle_font());
    }
    if (!params.subtitle_size_is_default())
    {
        overrides.push_back(std::string("Default.Fontsize=") + str::from(params.subtitle_size()));
    }
    if (!params.subtitle_color_is_default())
    {
        unsigned int color = params.subtitle_color();
        unsigned int a = 255u - ((color >> 24u) & 0xffu);
        unsigned int r = (color >> 16u) & 0xffu;
        unsigned int g = (color >> 8u) & 0xffu;
        unsigned int b = color & 0xffu;
        std::string color_str = str::asprintf("&H%02x%02x%02x%02x", a, b, g, r);
        overrides.push_back(std::string("Default.PrimaryColour=") + color_str);
        overrides.push_back(std::string("Default.SecondaryColour=") + color_str);
    }
    if (!params.subtitle_shadow_is_default())
    {
        overrides.push_back(std::string("Default.Shadow=")
                + (params.subtitle_shadow() == 0 ? "0" : "3"));
    }
    const char *ass_overrides[overrides.size() + 1];
    for (size_t i = 0; i < overrides.size(); i++)
    {
        ass_overrides[i] = overrides[i].c_str();
    }
    ass_overrides[overrides.size()] = NULL;
    ass_set_style_overrides(_ass_library, const_cast<char **>(ass_overrides));
    ass_set_font_scale(_ass_renderer, (params.subtitle_scale() >= 0.0f ? params.subtitle_scale() : 1.0));
    ass_process_force_style(_ass_track);
}

bool subtitle_renderer::prerender_ass(const subtitle_box &box, int64_t timestamp,
        const parameters &params, int width, int height, float pixel_aspect_ratio)
{
    // Lock
    global_libass_mutex.lock();

    // Set basic parameters
    ass_set_frame_size(_ass_renderer, width, height);
    ass_set_aspect_ratio(_ass_renderer, 1.0, pixel_aspect_ratio);

    // Put subtitle data into ASS track
    if (_ass_track)
    {
        ass_free_track(_ass_track);
    }
    _ass_track = ass_new_track(_ass_library);
    if (!_ass_track)
    {
        global_libass_mutex.unlock();
        throw exc(_("Cannot initialize LibASS track."));
    }
    std::string conv_str = box.str;
    if (params.subtitle_encoding() != "")
    {
        try
        {
            conv_str = str::convert(box.str, params.subtitle_encoding(), "UTF-8");
        }
        catch (std::exception &e)
        {
            msg::err(_("Subtitle character set conversion failed: %s"), e.what());
            conv_str = std::string("Dialogue: 0,0:00:00.00,9:59:59.99,") + e.what();
        }
    }
    if (box.format == subtitle_box::ass)
    {
        ass_process_codec_private(_ass_track, const_cast<char *>(box.style.c_str()), box.style.length());
        ass_process_data(_ass_track, const_cast<char *>(conv_str.c_str()), conv_str.length());
    }
    else
    {
        // Set a default ASS style for text subtitles
        std::string style =
            "[Script Info]\n"
            "ScriptType: v4.00+\n"
            "\n"
            "[V4+ Styles]\n"
            "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
            "OutlineColour, BackColour, Bold, Italic, Underline, BorderStyle, "
            "Outline, Shadow, Alignment, MarginL, MarginR, MarginV, AlphaLevel, Encoding\n"
            "Style: Default,Arial,16,&Hffffff,&Hffffff,&H0,&H0,0,0,0,1,1,0,2,10,10,10,0,0\n"
            "\n"
            "[Events]\n"
            "Format: Layer, Start, End, Text\n"
            "\n";
        ass_process_codec_private(_ass_track, const_cast<char *>(style.c_str()), style.length());
        // Convert text to ASS
        str::replace(conv_str, "\r\n", "\\N");
        str::replace(conv_str, "\n", "\\N");
        std::string str = "Dialogue: 0,0:00:00.00,9:59:59.99," + conv_str;
        ass_process_data(_ass_track, const_cast<char *>(str.c_str()), str.length());
    }
    set_ass_parameters(params);

    // Render subtitle
    int change_detected = 0;
    _ass_img = ass_render_frame(_ass_renderer, _ass_track, timestamp / 1000, &change_detected);

    // Unlock
    global_libass_mutex.unlock();

    // Determine bounding box
    int min_x = width;
    int max_x = -1;
    int min_y = height;
    int max_y = -1;
    ASS_Image *img = _ass_img;
    while (img && img->w > 0 && img->h > 0)
    {
        if (img->dst_x < min_x)
            min_x = img->dst_x;
        if (img->dst_x + img->w - 1 > max_x)
            max_x = img->dst_x + img->w - 1;
        if (img->dst_y < min_y)
            min_y = img->dst_y;
        if (img->dst_y + img->h - 1 > max_y)
            max_y = img->dst_y + img->h - 1;
        img = img->next;
    }
    if (max_x < 0)
    {
        _bb_x = 0;
        _bb_y = 0;
        _bb_w = 0;
        _bb_h = 0;
    }
    else
    {
        _bb_x = min_x;
        _bb_w = max_x - min_x + 1;
        _bb_y = min_y;
        _bb_h = max_y - min_y + 1;
    }
    return change_detected;
}

void subtitle_renderer::render_ass(uint32_t *bgra32_buffer)
{
    if (_bb_w > 0 && _bb_h > 0)
    {
        std::memset(bgra32_buffer, 0, _bb_w * _bb_h * sizeof(uint32_t));
        ASS_Image *img = _ass_img;
        while (img && img->w > 0 && img->h > 0)
        {
            blend_ass_image(img, bgra32_buffer);
            img = img->next;
        }
    }
}

bool subtitle_renderer::prerender_img(const subtitle_box &box)
{
    _img_box = &box;
    // Determine bounding box
    int min_x = std::numeric_limits<int>::max();
    int max_x = -1;
    int min_y = std::numeric_limits<int>::max();
    int max_y = -1;
    for (size_t i = 0; i < box.images.size(); i++)
    {
        const subtitle_box::image_t &img = box.images[i];
        if (img.x < min_x)
            min_x = img.x;
        if (img.x + img.w - 1 > max_x)
            max_x = img.x + img.w - 1;
        if (img.y < min_y)
            min_y = img.y;
        if (img.y + img.h - 1 > max_y)
            max_y = img.y + img.h - 1;
    }
    if (max_x < 0)
    {
        _bb_x = 0;
        _bb_y = 0;
        _bb_w = 0;
        _bb_h = 0;
    }
    else
    {
        _bb_x = min_x;
        _bb_w = max_x - min_x + 1;
        _bb_y = min_y;
        _bb_h = max_y - min_y + 1;
    }
    return true;
}

void subtitle_renderer::render_img(uint32_t *bgra32_buffer)
{
    if (_bb_w <= 0 || _bb_h <= 0)
    {
        return;
    }

    std::memset(bgra32_buffer, 0, _bb_w * _bb_h * sizeof(uint32_t));
    for (size_t i = 0; i < _img_box->images.size(); i++)
    {
        const subtitle_box::image_t &img = _img_box->images[i];
        const uint8_t *src = &(img.data[0]);
        for (int src_y = 0; src_y < img.h; src_y++)
        {
            int dst_y = src_y + img.y - _bb_y;
            if (dst_y >= _bb_h)
            {
                break;
            }
            for (int src_x = 0; src_x < img.w; src_x++)
            {
                int dst_x = src_x + img.x - _bb_x;
                if (dst_x >= _bb_w)
                {
                    break;
                }
                int palette_index = src[src_x];
                uint32_t palette_entry = reinterpret_cast<const uint32_t *>(&(img.palette[0]))[palette_index];
                unsigned int A = (palette_entry >> 24u);
                unsigned int R = (palette_entry >> 16u) & 0xffu;
                unsigned int G = (palette_entry >> 8u) & 0xffu;
                unsigned int B = palette_entry & 0xffu;
                uint32_t oldval = bgra32_buffer[dst_y * _bb_w + dst_x];
                // XXX: The BGRA layout used here may be wrong on big endian system
                uint32_t newval = std::min(A + (oldval >> 24u), 255u) << 24u
                    | ((A * R + (255u - A) * ((oldval >> 16u) & 0xffu)) / 255u) << 16u
                    | ((A * G + (255u - A) * ((oldval >>  8u) & 0xffu)) / 255u) << 8u
                    | ((A * B + (255u - A) * ((oldval       ) & 0xffu)) / 255u);
                bgra32_buffer[dst_y * _bb_w + dst_x] = newval;
            }
            src += img.linesize;
        }
    }
}
