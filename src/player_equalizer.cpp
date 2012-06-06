/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
 * Martin Lambers <marlam@marlam.de>
 * Stefan Eilemann <eile@eyescale.ch>
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

#include <sstream>
#include <cmath>
#include <cstdlib>

#include <eq/eq.h>

#include "gettext.h"
#define _(string) gettext(string)

#include "dbg.h"
#include "msg.h"
#include "s11n.h"

#include "dispatch.h"
#include "media_input.h"
#include "video_output.h"
#include "player_equalizer.h"

/*
 * Every eq::Node has a special player: player_eq_node.
 * These node players do not control the video playing themselves. Instead,
 * they are told what to do via the frame data.
 *
 * eq::Config has the master player object. This master also plays the sound,
 * to be able to synchronize the video to it.
 *
 * The application node reuses the player of eq::Config, so that it does not
 * need to run two players.
 *
 * Each eq::Window has a special video_output: video_output_eq_window.
 * It manages the video textures.
 *
 * Each eq::Channel than calls the window's display function to render its subset of
 * the video.
 */

extern dispatch* global_dispatch;
static player_equalizer* global_player_equalizer = NULL;

/*
 * player_eq_node
 *
 * Implementation of player for eq_node.
 * The player_eq_node instance of the application node lives in eq_config instead of eq_node.
 */

class player_eq_node : public player
{
private:
    class media_input _media_input;

public:
    player_eq_node() : player()
    {
    }

    bool init(const open_input_data& input)
    {
        try {
            _media_input.open(input.urls, input.dev_request);
            if (_media_input.video_streams() == 0)
                throw exc(_("No video streams found."));
            if (input.params.stereo_layout_is_set() || input.params.stereo_layout_swap_is_set())
                _media_input.set_stereo_layout(input.params.stereo_layout(), input.params.stereo_layout_swap());
            _media_input.select_video_stream(input.params.video_stream());
            player::open();
        }
        catch (std::exception& e) {
            msg::err("%s", e.what());
            return false;
        }
        return true;
    }

    void seek(int64_t pos)
    {
        _media_input.seek(pos);
        // The master player read a video frame; do the same to keep sync
        start_frame_read();
    }

    void start_frame_read()
    {
        // Only called on slave nodes
        _media_input.start_video_frame_read();
    }

    void finish_frame_read()
    {
        // Only called on slave nodes
        _video_frame = _media_input.finish_video_frame_read();
        if (!_video_frame.is_valid())
        {
            msg::err(_("Reading input frame failed."));
            std::exit(1);
        }
    }
};

/*
 * video_output_eq_channel
 *
 * Implementation of video_output for eq_channel.
 *
 * Much of the video_output interface is not relevant for Equalizer, and thus
 * is implemented with simple stub functions.
 */

class video_output_eq_channel : public video_output
{
private:
    eq::Channel *_channel;
    const float _canvas_width;
    const float _canvas_height;
    float _canvas_video_area_w;
    float _canvas_video_area_h;

protected:
    int video_display_width() const { return screen_width() * _canvas_video_area_w; }
    int video_display_height() const { return screen_height() * _canvas_video_area_h; }
    int screen_width() const { return _channel->getPixelViewport().w / _channel->getViewport().w; }
    int screen_height() const { return _channel->getPixelViewport().h / _channel->getViewport().h; }
    float screen_pixel_aspect_ratio() const
    {
        float pixels_per_unit_x = _channel->getPixelViewport().w / (_canvas_width * _channel->getViewport().w);
        float pixels_per_unit_y = _channel->getPixelViewport().h / (_canvas_height * _channel->getViewport().h);
        return pixels_per_unit_y / pixels_per_unit_x;
    }
    int width() const { return _channel->getPixelViewport().w; }
    int height() const { return _channel->getPixelViewport().h; }
    int pos_x() const { return 0; }
    int pos_y() const { return 0; }
    GLEWContext* glewGetContext() const { return const_cast<GLEWContext*>(_channel->getWindow()->glewGetContext()); }
    bool context_is_stereo() const { return false; }
    void recreate_context(bool) { }
    void trigger_resize(int, int) { }

public:
    int64_t wait_for_subtitle_renderer()
    {
        if (!_subtitle_renderer.is_initialized())
        {
            msg::wrn(_("Waiting for subtitle renderer initialization..."));
            try
            {
                while (!_subtitle_renderer.is_initialized())
                {
                    usleep(10000);
                }
            }
            catch (std::exception &e)
            {
                msg::err("%s", e.what());
                std::exit(1);
            }
        }
        return 0;
    }
    bool supports_stereo() const { return false; }
    void center() { }
    void enter_fullscreen() { }
    void exit_fullscreen() { }
    void process_events() { }

public:
    video_output_eq_channel(eq::Channel *channel,
            float canvas_width, float canvas_height) :
        video_output(),
        _channel(channel),
        _canvas_width(canvas_width),
        _canvas_height(canvas_height)
    {
    }

    void set_canvas_size(float canvas_video_area_w, float canvas_video_area_h)
    {
        _canvas_video_area_w = canvas_video_area_w;
        _canvas_video_area_h = canvas_video_area_h;
    }

    void display_current_frame(bool mono_right_instead_of_left,
            float x, float y, float w, float h,
            const GLint viewport[4], const float tex_coords[4][2])
    {
        float my_tex_coords[2][4][2] =
        {
            {
                { tex_coords[0][0], tex_coords[0][1] },
                { tex_coords[1][0], tex_coords[1][1] },
                { tex_coords[2][0], tex_coords[2][1] },
                { tex_coords[3][0], tex_coords[3][1] },
            },
            {
                { tex_coords[0][0], tex_coords[0][1] },
                { tex_coords[1][0], tex_coords[1][1] },
                { tex_coords[2][0], tex_coords[2][1] },
                { tex_coords[3][0], tex_coords[3][1] },
            },
        };
        GLint vp[2][4];
        std::memcpy(vp[0], viewport, 4 * sizeof(int));
        std::memcpy(vp[1], viewport, 4 * sizeof(int));
        video_output::display_current_frame(0, true, mono_right_instead_of_left,
                x, y, w, h, vp, my_tex_coords, 0, 0, parameters::mode_mono_left);
    }
};

/*
 * Define errors produced by player.
 */

enum Error
{
    ERROR_MAP_INITDATA_FAILED = eq::ERROR_CUSTOM,
    ERROR_MAP_FRAMEDATA_FAILED,
    ERROR_PLAYER_INIT_FAILED,
    ERROR_OPENGL_2_1_NEEDED
};

struct ErrorData
{
    const uint32_t code;
    const std::string text;
};

static ErrorData errors[] =
{
    { ERROR_MAP_FRAMEDATA_FAILED, _("Init data mapping failed.") },
    { ERROR_MAP_INITDATA_FAILED, _("Frame data mapping failed.") },
    { ERROR_PLAYER_INIT_FAILED, _("Video player initialization failed.") },
    { ERROR_OPENGL_2_1_NEEDED, _("Need at least OpenGL 2.1.") },
    { 0, "" } // last!
};

static void initErrors()
{
    co::base::ErrorRegistry &registry = co::base::Global::getErrorRegistry();
    for (size_t i = 0; errors[i].code != 0; i++)
    {
        registry.setString(errors[i].code, errors[i].text);
    }
}

static void exitErrors()
{
    co::base::ErrorRegistry &registry = co::base::Global::getErrorRegistry();
    for (size_t i = 0; errors[i].code != 0; i++)
    {
        registry.eraseString(errors[i].code);
    }
}

/*
 * eq_init_data
 */

class eq_init_data : public co::Object
{
public:
    eq::uint128_t frame_data_id;
    open_input_data input;
    parameters params;
    bool flat_screen;
    float canvas_width;
    float canvas_height;

    eq_init_data()
    {
        flat_screen = true;
    }

    virtual ~eq_init_data()
    {
    }

protected:
    virtual ChangeType getChangeType() const
    {
        return co::Object::STATIC;
    }

    virtual void getInstanceData(co::DataOStream &os)
    {
        std::ostringstream oss;
        s11n::save(oss, frame_data_id.high());
        s11n::save(oss, frame_data_id.low());
        s11n::save(oss, input);
        s11n::save(oss, params);
        s11n::save(oss, flat_screen);
        s11n::save(oss, canvas_width);
        s11n::save(oss, canvas_height);
        os << oss.str();
    }

    virtual void applyInstanceData(co::DataIStream &is)
    {
        std::string s;
        is >> s;
        std::istringstream iss(s);
        s11n::load(iss, frame_data_id.high());
        s11n::load(iss, frame_data_id.low());
        s11n::load(iss, input);
        s11n::load(iss, params);
        s11n::load(iss, flat_screen);
        s11n::load(iss, canvas_width);
        s11n::load(iss, canvas_height);
    }
};

/*
 * eq_frame_data
 */

class eq_frame_data : public co::Object
{
public:
    std::string dispatch_state;
    subtitle_box subtitle;
    int64_t seek_to;
    bool prep_frame;
    bool drop_frame;
    bool display_frame;
    bool display_statistics;
    struct { float x, y, w, h, d; } canvas_video_area;
    float tex_coords[4][2];

public:
    eq_frame_data() :
        seek_to(0),
        prep_frame(false),
        drop_frame(false),
        display_frame(false),
        display_statistics(false)
    {
    }

protected:
    virtual ChangeType getChangeType() const
    {
        return co::Object::INSTANCE;
    }

    virtual void getInstanceData(co::DataOStream &os)
    {
        std::ostringstream oss;
        s11n::save(oss, dispatch_state);
        s11n::save(oss, subtitle);
        s11n::save(oss, seek_to);
        s11n::save(oss, prep_frame);
        s11n::save(oss, drop_frame);
        s11n::save(oss, display_frame);
        s11n::save(oss, display_statistics);
        s11n::save(oss, &canvas_video_area, sizeof(canvas_video_area));
        s11n::save(oss, tex_coords, sizeof(tex_coords));
        os << oss.str();
    }

    virtual void applyInstanceData(co::DataIStream &is)
    {
        std::string s;
        is >> s;
        std::istringstream iss(s);
        s11n::load(iss, dispatch_state);
        s11n::load(iss, subtitle);
        s11n::load(iss, seek_to);
        s11n::load(iss, prep_frame);
        s11n::load(iss, drop_frame);
        s11n::load(iss, display_frame);
        s11n::load(iss, display_statistics);
        s11n::load(iss, &canvas_video_area, sizeof(canvas_video_area));
        s11n::load(iss, tex_coords, sizeof(tex_coords));
    }
};

/*
 * eq_config
 */

class eq_config : public eq::Config
{
private:
    eq_init_data _eq_init_data;         // Master eq_init_data instance
    eq_frame_data _eq_frame_data;       // Master eq_frame_data instance

public:
    eq_config(eq::ServerPtr parent) : eq::Config(parent)
    {
    }

    bool init(const open_input_data& input, bool flat_screen)
    {
        msg::dbg(HERE);
        setLatency( 0 );

        // Initialize master init/frame data instances
        _eq_init_data.input = input;
        _eq_init_data.params = dispatch::parameters();
        _eq_init_data.flat_screen = flat_screen;
        // Find canvas
        if (getCanvases().size() < 1)
        {
            msg::err(_("No canvas in Equalizer configuration."));
            return false;
        }
        float canvas_w = getCanvases()[0]->getWall().getWidth();
        float canvas_h = getCanvases()[0]->getWall().getHeight();
        _eq_init_data.canvas_width = canvas_w;
        _eq_init_data.canvas_height = canvas_h;
        msg::inf(_("Equalizer canvas: %gx%g, aspect ratio %g:1"), canvas_w, canvas_h, canvas_w / canvas_h);
        // Register master instances
        registerObject(&_eq_frame_data);
        _eq_init_data.frame_data_id = _eq_frame_data.getID();
        registerObject(&_eq_init_data);
        msg::dbg(HERE);
        return eq::Config::init(_eq_init_data.getID());
    }

    virtual bool exit()
    {
        msg::dbg(HERE);
        bool ret = eq::Config::exit();
        // Deregister master instances
        deregisterObject(&_eq_init_data);
        deregisterObject(&_eq_frame_data);
        msg::dbg(HERE);
        return ret;
    }

    virtual uint32_t startFrame()
    {
        // Run player steps until we are told to do something
        bool more_steps;
        do {
            global_player_equalizer->step(&more_steps, &_eq_frame_data.seek_to,
                    &_eq_frame_data.prep_frame, &_eq_frame_data.drop_frame, &_eq_frame_data.display_frame);
            dispatch::process_all_events();
            if (!dispatch::playing())
                more_steps = false;
        }
        while (more_steps
                && _eq_frame_data.seek_to == -1
                && !_eq_frame_data.prep_frame
                && !_eq_frame_data.drop_frame
                && !_eq_frame_data.display_frame
                && !dispatch::pausing());
        if (!more_steps) {
            this->exit();
            return 0;
        }
        // Update the video state for all (it might have changed via handleEvent())
        _eq_frame_data.subtitle = global_player_equalizer->get_subtitle_box();
        _eq_frame_data.dispatch_state = global_dispatch->save_state();
        // Find region of canvas to use, depending on the video aspect ratio and zoom level
        float aspect_ratio = dispatch::media_input()->video_frame_template().aspect_ratio;
        float crop_aspect_ratio = dispatch::parameters().crop_aspect_ratio();
        float canvas_aspect_ratio = _eq_init_data.canvas_width / _eq_init_data.canvas_height;
        float zoom = dispatch::parameters().zoom();
        if (_eq_init_data.flat_screen) {
            _eq_frame_data.tex_coords[0][0] = 0.0f;
            _eq_frame_data.tex_coords[0][1] = 0.0f;
            _eq_frame_data.tex_coords[1][0] = 1.0f;
            _eq_frame_data.tex_coords[1][1] = 0.0f;
            _eq_frame_data.tex_coords[2][0] = 1.0f;
            _eq_frame_data.tex_coords[2][1] = 1.0f;
            _eq_frame_data.tex_coords[3][0] = 0.0f;
            _eq_frame_data.tex_coords[3][1] = 1.0f;
            if (crop_aspect_ratio > 0.0f) {
                if (aspect_ratio >= crop_aspect_ratio) {
                    float cutoff = (1.0f - crop_aspect_ratio / aspect_ratio) / 2.0f;
                    _eq_frame_data.tex_coords[0][0] += cutoff;
                    _eq_frame_data.tex_coords[1][0] -= cutoff;
                    _eq_frame_data.tex_coords[2][0] -= cutoff;
                    _eq_frame_data.tex_coords[3][0] += cutoff;
                } else {
                    float cutoff = (1.0f - aspect_ratio / crop_aspect_ratio) / 2.0f;
                    _eq_frame_data.tex_coords[0][1] += cutoff;
                    _eq_frame_data.tex_coords[1][1] += cutoff;
                    _eq_frame_data.tex_coords[2][1] -= cutoff;
                    _eq_frame_data.tex_coords[3][1] -= cutoff;
                }
                aspect_ratio = crop_aspect_ratio;
            }
            if (aspect_ratio >= canvas_aspect_ratio) {
                // need black borders top and bottom
                float zoom_aspect_ratio = zoom * canvas_aspect_ratio + (1.0f - zoom) * aspect_ratio;
                _eq_frame_data.canvas_video_area.w = 1.0f;
                _eq_frame_data.canvas_video_area.h = canvas_aspect_ratio / zoom_aspect_ratio;
                _eq_frame_data.canvas_video_area.x = (1.0f - _eq_frame_data.canvas_video_area.w) / 2.0f;
                _eq_frame_data.canvas_video_area.y = (1.0f - _eq_frame_data.canvas_video_area.h) / 2.0f;
                float cutoff = (1.0f - zoom_aspect_ratio / aspect_ratio) / 2.0f;
                _eq_frame_data.tex_coords[0][0] += cutoff;
                _eq_frame_data.tex_coords[1][0] -= cutoff;
                _eq_frame_data.tex_coords[2][0] -= cutoff;
                _eq_frame_data.tex_coords[3][0] += cutoff;
            } else {
                // need black borders left and right
                _eq_frame_data.canvas_video_area.w = aspect_ratio / canvas_aspect_ratio;
                _eq_frame_data.canvas_video_area.h = 1.0f;
                _eq_frame_data.canvas_video_area.x = (1.0f - _eq_frame_data.canvas_video_area.w) / 2.0f;
                _eq_frame_data.canvas_video_area.y = (1.0f - _eq_frame_data.canvas_video_area.h) / 2.0f;
            }
        }
        else
        {
            compute_3d_canvas(&_eq_frame_data.canvas_video_area.h, &_eq_frame_data.canvas_video_area.d);
            // compute width and offset for 1m high 'screen' quad in 3D space
            _eq_frame_data.canvas_video_area.w = _eq_frame_data.canvas_video_area.h * aspect_ratio;
            _eq_frame_data.canvas_video_area.x = -0.5f * _eq_frame_data.canvas_video_area.w;
            _eq_frame_data.canvas_video_area.y = -0.5f * _eq_frame_data.canvas_video_area.h;
            _eq_frame_data.tex_coords[0][0] = 0.0f;
            _eq_frame_data.tex_coords[0][1] = 0.0f;
            _eq_frame_data.tex_coords[1][0] = 1.0f;
            _eq_frame_data.tex_coords[1][1] = 0.0f;
            _eq_frame_data.tex_coords[2][0] = 1.0f;
            _eq_frame_data.tex_coords[2][1] = 1.0f;
            _eq_frame_data.tex_coords[3][0] = 0.0f;
            _eq_frame_data.tex_coords[3][1] = 1.0f;
        }
        // Commit the updated frame data
        const eq::uint128_t version = _eq_frame_data.commit();
        // Start this frame with the committed frame data
        return eq::Config::startFrame(version);
    }

    virtual bool handleEvent(const eq::ConfigEvent *event)
    {
        if (eq::Config::handleEvent(event))
        {
            return true;
        }
        if (event->data.type == eq::Event::KEY_PRESS)
        {
            switch (event->data.keyPress.key)
            {
            case 'S':
                _eq_frame_data.display_statistics = !_eq_frame_data.display_statistics;
                break;
            case 'q':
                controller::send_cmd(command::toggle_play);
                break;
            case 'e':
            case eq::KC_F7:
                controller::send_cmd(command::toggle_stereo_mode_swap);
                break;
            case 'f':
                /* fullscreen toggling not supported with Equalizer */
                break;
            case 'c':
                /* window centering not supported with Equalizer */
                break;
            case ' ':
            case 'p':
                controller::send_cmd(command::toggle_pause);
                break;
            case '.':
                controller::send_cmd(command::step);
                break;
            case 'v':
                /* TODO: cycling video streams is currently not supported with Equalizer.
                 * We would have to cycle the streams in all node players, and thus communicate
                 * the change via frame data. */
                //controller::send_cmd(command::cycle_video_stream);
                break;
            case 'a':
                /* TODO: cycling audio streams is currently not supported with Equalizer.
                 * We would have to cycle the streams in all node players, and thus communicate
                 * the change via frame data. */
                //controller::send_cmd(command::cycle_audio_stream);
                break;
            case 's':
                /* TODO: cycling subtitle streams is currently not supported with Equalizer.
                 * We would have to cycle the streams in all node players, and thus communicate
                 * the change via frame data. */
                //controller::send_cmd(command::cycle_subtitle_stream);
                break;
            case '1':
                controller::send_cmd(command::adjust_contrast, -0.05f);
                break;
            case '2':
                controller::send_cmd(command::adjust_contrast, +0.05f);
                break;
            case '3':
                controller::send_cmd(command::adjust_brightness, -0.05f);
                break;
            case '4':
                controller::send_cmd(command::adjust_brightness, +0.05f);
                break;
            case '5':
                controller::send_cmd(command::adjust_hue, -0.05f);
                break;
            case '6':
                controller::send_cmd(command::adjust_hue, +0.05f);
                break;
            case '7':
                controller::send_cmd(command::adjust_saturation, -0.05f);
                break;
            case '8':
                controller::send_cmd(command::adjust_saturation, +0.05f);
                break;
            case '[':
                controller::send_cmd(command::adjust_parallax, -0.01f);
                break;
            case ']':
                controller::send_cmd(command::adjust_parallax, +0.01f);
                break;
            case '(':
                controller::send_cmd(command::adjust_ghostbust, -0.01f);
                break;
            case ')':
                controller::send_cmd(command::adjust_ghostbust, +0.01f);
                break;
            case '<':
                controller::send_cmd(command::adjust_zoom, -0.1f);
                break;
            case '>':
                controller::send_cmd(command::adjust_zoom, +0.1f);
                break;
            case eq::KC_LEFT:
                controller::send_cmd(command::seek, -10.0f);
                break;
            case eq::KC_RIGHT:
                controller::send_cmd(command::seek, +10.0f);
                break;
            case eq::KC_DOWN:
                controller::send_cmd(command::seek, -60.0f);
                break;
            case eq::KC_UP:
                controller::send_cmd(command::seek, +60.0f);
                break;
            case eq::KC_PAGE_DOWN:
                controller::send_cmd(command::seek, -600.0f);
                break;
            case eq::KC_PAGE_UP:
                controller::send_cmd(command::seek, +600.0f);
                break;
            }
        }
        else if (event->data.type == eq::Event::CHANNEL_POINTER_BUTTON_RELEASE)
        {
            float event_px = event->data.pointerButtonRelease.x;        // Event position in pixels
            float channel_pw = event->data.context.pvp.w;               // Channel width in pixels
            float event_x = event_px / channel_pw;                      // Event position relative to channel
            // Event position relative to destination view (which seems to be the same as the canvas?)
            float dest = event->data.context.vp.x + event_x * event->data.context.vp.w;
            dest = std::min(std::max(dest, 0.0f), 1.0f);                // Clamp to [0,1] - just to be sure
            controller::send_cmd(command::set_pos, dest);               // Seek to this position
        }
        return true;
    }

private:
    void compute_3d_canvas(float *height, float *distance)
    {
        float angle = -1.0f;
        *height = 0.0f;
        *distance = 0.0f;

        const eq::Canvases &canvases = getCanvases();
        for (eq::Canvases::const_iterator i = canvases.begin(); i != canvases.end(); i++)
        {
            const eq::Segments &segments = (*i)->getSegments();
            for (eq::Segments::const_iterator j = segments.begin(); j != segments.end(); j++)
            {
                const eq::Segment *segment = *j;
                eq::Wall wall = segment->getWall();
#if 0 // Hack to compute rotated walls for Equalizer configuration. See doc/multi-display.txt.
                eq::Matrix4f matrix(eq::Matrix4f::IDENTITY);
                matrix.rotate(1.3f, eq::Vector3f::FORWARD);
                wall.bottomLeft = matrix * wall.bottomLeft;
                wall.bottomRight = matrix * wall.bottomRight;
                wall.topLeft = matrix * wall.topLeft;
                std::cout << wall << std::endl;
#endif
                const eq::Vector3f u = wall.bottomRight - wall.bottomLeft;
                const eq::Vector3f v = wall.topLeft - wall.bottomLeft;
                eq::Vector3f w = u.cross(v);
                w.normalize();

                const eq::Vector3f dot = w.dot(eq::Vector3f::FORWARD);
                const float val = dot.squared_length();
                if (val < angle) // facing more away then previous segment
                {
                    continue;
                }

                // transform wall to full canvas
                eq::Viewport vp = eq::Viewport::FULL;
                vp.transform(segment->getViewport());
                wall.apply(vp);

                const eq::Vector3f topRight = wall.topLeft + wall.bottomRight - wall.bottomLeft;
                float yMin = EQ_MIN(wall.bottomLeft.y(), wall.bottomRight.y());
                float yMax = EQ_MAX(wall.bottomLeft.y(), wall.bottomRight.y());
                yMin = EQ_MIN(yMin, wall.topLeft.y());
                yMax = EQ_MAX(yMax, wall.topLeft.y());
                yMin = EQ_MIN(yMin, topRight.y());
                yMax = EQ_MAX(yMax, topRight.y());

                const float h = yMax - yMin;
                const eq::Vector3f center = (wall.bottomRight + wall.topLeft) * 0.5f;
                const float d = -center.z();

                // 'same' orientation and distance
                if (std::fabs(angle - val) < 0.0001f && std::fabs(d - *distance) < 0.0001f)
                {
                    if (h > *height)
                    {
                        *height = h;
                    }
                }
                else
                {
                    *height = h;
                    *distance = d;
                    angle = val;
                }
            }
        }
    }
};

/*
 * eq_node
 */

class eq_node : public eq::Node
{
private:
    dispatch* _dispatch;
    player_eq_node _player;

public:
    eq_init_data init_data;
    eq_frame_data frame_data;

    eq_node(eq::Config *parent) : eq::Node(parent), _dispatch(NULL)
    {
    }

    ~eq_node()
    {
        delete _dispatch;
    }

protected:
    virtual bool configInit(const eq::uint128_t &init_id)
    {
        if (!eq::Node::configInit(init_id))
        {
            return false;
        }
        // Map our InitData instance to the master instance
        eq_config *config = static_cast<eq_config *>(getConfig());
        if (!config->mapObject(&init_data, init_id))
        {
            setError(ERROR_MAP_INITDATA_FAILED);
            return false;
        }
        // Map our FrameData instance to the master instance
        if (!config->mapObject(&frame_data, init_data.frame_data_id))
        {
            setError(ERROR_MAP_FRAMEDATA_FAILED);
            return false;
        }

        msg::dbg(HERE);
        // Create decoders and input
        if (!isApplicationNode( ))
        {
            _dispatch = new dispatch(NULL, NULL, true, init_data.flat_screen, true,
                    false, false, init_data.params.log_level(), init_data.params.benchmark(),
                    init_data.params.swap_interval());
            if (!_player.init(init_data.input))
            {
                setError(ERROR_PLAYER_INIT_FAILED);
                return false;
            }
        }
        msg::dbg(HERE);
        return true;
    }

    virtual bool configExit()
    {
        msg::dbg(HERE);
        eq::Config *config = getConfig();
        // Unmap our FrameData instance
        config->unmapObject(&frame_data);
        // Unmap our InitData instance
        config->unmapObject(&init_data);
        // Cleanup
        if (!isApplicationNode( ))
            _player.close();
        msg::dbg(HERE);
        return eq::Node::configExit();
    }

    virtual void frameStart(const eq::uint128_t &frame_id, const uint32_t frame_number)
    {
        // Update our frame data
        frame_data.sync(frame_id);
        // Do as we're told
        if (isApplicationNode( ))
        {
            // Nothing to do since the config's master player already did it
        }
        else
        {
            _dispatch->load_state(frame_data.dispatch_state);
            if (frame_data.seek_to >= 0)
            {
                _player.seek(frame_data.seek_to);
            }
            if (frame_data.prep_frame)
            {
                _player.finish_frame_read();
            }
            if (frame_data.drop_frame)
            {
                _player.finish_frame_read();
                _player.start_frame_read();
            }
        }
        startFrame(frame_number);
    }

    virtual void frameFinish(const eq::uint128_t &, const uint32_t frame_number)
    {
        if (isApplicationNode( ))
        {
            // Nothing to do since the config's master player already did it
        }
        else
        {
            if (frame_data.prep_frame)
            {
                // The frame was uploaded to texture memory.
                // Start reading the next one asynchronously.
                _player.start_frame_read();
            }
        }
        releaseFrame(frame_number);
    }

public:
    const video_frame &get_video_frame()
    {
        if (isApplicationNode( ))
            return global_player_equalizer->get_video_frame();
        else
            return _player.get_video_frame();
    }
};

/*
 * eq_pipe
 */

class eq_pipe : public eq::Pipe
{
public:
    eq_pipe(eq::Node *parent) : eq::Pipe(parent)
    {
    }
};

/*
 * eq_window
 */

class eq_window : public eq::Window
{
public:
    eq_window(eq::Pipe *parent) : eq::Window(parent)
    {
    }

protected:

    virtual bool configInitGL(const eq::uint128_t &init_id)
    {
        msg::dbg(HERE);
        if (!eq::Window::configInitGL(init_id))
        {
            return false;
        }
        if (!glewContextIsSupported(const_cast<GLEWContext *>(glewGetContext()),
                    "GL_VERSION_2_1 GL_EXT_framebuffer_object"))
        {
            msg::err(_("This OpenGL implementation does not support OpenGL 2.1 and framebuffer objects."));
            setError(ERROR_OPENGL_2_1_NEEDED);
            return false;
        }

        // Disable some things that Equalizer seems to enable for some reason.
        glDisable(GL_LIGHTING);

        msg::dbg(HERE);
        return true;
    }

    virtual bool configExitGL()
    {
        msg::dbg(HERE);
        return eq::Window::configExitGL();
    }

    virtual void swapBuffers()
    {
        eq_node *node = static_cast<eq_node *>(getNode());
        if (node->frame_data.display_frame)
            eq::Window::swapBuffers();
    }
};

/*
 * eq_channel
 */

class eq_channel : public eq::Channel
{
private:
    video_output_eq_channel _video_output;

public:
    eq_channel(eq::Window *parent) :
        eq::Channel(parent),
        _video_output(this,
                static_cast<eq_node *>(getNode())->init_data.canvas_width,
                static_cast<eq_node *>(getNode())->init_data.canvas_height)
    {
    }

protected:

    virtual bool configExit()
    {
        msg::dbg(HERE);
        getWindow()->makeCurrent();
        _video_output.deinit();
        msg::dbg(HERE);
        return eq::Channel::configExit();
    }

    virtual void frameDraw(const eq::uint128_t &frame_id)
    {
        // Let Equalizer initialize some stuff
        eq::Channel::frameDraw(frame_id);

        // Get the canvas video area and the canvas channel area
        eq_node *node = static_cast<eq_node *>(getNode());
        const struct { float x, y, w, h, d; } canvas_video_area =
        {
            node->frame_data.canvas_video_area.x,
            node->frame_data.canvas_video_area.y,
            node->frame_data.canvas_video_area.w,
            node->frame_data.canvas_video_area.h,
            node->frame_data.canvas_video_area.d
        };
        _video_output.set_canvas_size(canvas_video_area.w, canvas_video_area.h);
        const eq::Viewport &canvas_channel_area = getViewport();
        // Determine the video quad to render
        float quad_x = canvas_video_area.x;
        float quad_y = canvas_video_area.y;
        float quad_w = canvas_video_area.w;
        float quad_h = canvas_video_area.h;
        if (node->init_data.flat_screen)
        {
            quad_x = ((quad_x - canvas_channel_area.x) / canvas_channel_area.w - 0.5f) * 2.0f;
            quad_y = ((quad_y - canvas_channel_area.y) / canvas_channel_area.h - 0.5f) * 2.0f;
            quad_w = 2.0f * quad_w / canvas_channel_area.w;
            quad_h = 2.0f * quad_h / canvas_channel_area.h;
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
        }
        else
        {
            glTranslatef(0.0f, 0.0f, -canvas_video_area.d);
        }

        // Display
        glEnable(GL_TEXTURE_2D);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        bool mono_right_instead_of_left = (getEye() == eq::EYE_RIGHT);
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        _video_output.display_current_frame(mono_right_instead_of_left, quad_x, quad_y, quad_w, quad_h,
                viewport, node->frame_data.tex_coords);
    }

    virtual void frameStart(const eq::uint128_t &, const uint32_t frame_number)
    {
        // Get frame data via from the node
        eq_node *node = static_cast<eq_node *>(getNode());
        // Do as we're told
        if (node->frame_data.prep_frame)
        {
            getWindow()->makeCurrent();
            if (node->frame_data.subtitle.is_valid())
            {
                _video_output.wait_for_subtitle_renderer();
            }
            _video_output.prepare_next_frame(node->get_video_frame(), node->frame_data.subtitle);
        }
        if (node->frame_data.display_frame)
        {
            _video_output.activate_next_frame();
        }
        startFrame(frame_number);
    }

    void frameViewFinish( const eq::uint128_t& id )
    {
        eq_node *node = static_cast<eq_node *>(getNode());
        if (node->frame_data.display_statistics)
            drawStatistics();
        eq::Channel::frameViewFinish( id );
    }
};

/*
 * eq_node_factory
 */

class eq_node_factory : public eq::NodeFactory
{
public:
    virtual eq::Config *createConfig(eq::ServerPtr parent)
    {
        return new eq_config(parent);
    }

    virtual eq::Node *createNode(eq::Config *parent)
    {
        return new eq_node(parent);
    }

    virtual eq::Pipe *createPipe(eq::Node *parent)
    {
        return new eq_pipe(parent);
    }

    virtual eq::Window *createWindow(eq::Pipe *parent)
    {
        return new eq_window(parent);
    }

    virtual eq::Channel *createChannel(eq::Window *parent)
    {
        return new eq_channel(parent);
    }
};

/*
 * player_equalizer
 */

player_equalizer::player_equalizer(int *argc, char *argv[], bool flat_screen) :
    player(), _flat_screen(flat_screen)
{
    assert(!global_player_equalizer);
    global_player_equalizer = this;
    /* Initialize Equalizer */
    initErrors();
    _node_factory = new eq_node_factory;
    if (!eq::init(*argc, argv, _node_factory))
    {
        throw exc(_("Equalizer initialization failed."));
    }
    /* Get a configuration */
    _config = static_cast<eq_config *>(eq::getConfig(*argc, argv));
    // The following code is only executed on the application node because
    // eq::getConfig() does not return on other nodes.
    if (!_config)
    {
        throw exc(_("Cannot get equalizer configuration."));
    }
}

player_equalizer::~player_equalizer()
{
    eq::releaseConfig(_config);
    eq::exit();
    exitErrors();
    delete _node_factory;
    global_player_equalizer = NULL;
}

void player_equalizer::open()
{
    if (!_config->init(*(global_dispatch->get_input_data()), _flat_screen))
    {
        throw exc(_("Equalizer configuration initialization failed."));
    }
}

static bool global_quit_request;

class eq_quit_controller : public controller
{
    virtual void receive_notification(const notification& note)
    {
        if (note.type == notification::quit)
            global_quit_request = true;
    }
};

void player_equalizer::mainloop()
{
    global_quit_request = false;
    eq_quit_controller qc;
    for (;;) {
        if (!global_player_equalizer) {
            dispatch::step();
            dispatch::process_all_events();
            if (global_quit_request)
                return;
        }
        if (global_player_equalizer) {
            eq_config* config = global_player_equalizer->_config;
            while (config->isRunning()) {
                config->startFrame();
                config->finishFrame();
            }
            global_dispatch->stop_eq_player();
            delete global_player_equalizer;
        }
    }
}
