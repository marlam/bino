/*
 * This file is part of bino, a program to play stereoscopic videos.
 *
 * Copyright (C) 2010
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

#include <eq/eq.h>
#include <eq/client/segment.h>  // FIXME: Remove this when switching to a newer Equalizer version

#include "debug.h"
#include "msg.h"
#include "s11n.h"

#include "video_output_opengl.h"
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
 * Each eq::Window has a special video_output_opengl: video_output_opengl_eq_window.
 * It manages the video textures.
 *
 * Each eq::Channel than calls the window's display function to render its subset of
 * the video.
 */


/*
 * player_eq_node
 *
 * Implementation of player for eq_node.
 * The player_eq_node instance of the application node lives in eq_config instead of eq_node.
 */

class player_eq_node : public player
{
private:
    bool _is_master;
    enum decoder::video_frame_format _fmt;

public:
    player_eq_node() : player(player::slave), _is_master(false)
    {
    }

    void eq_make_master()
    {
        make_master();
        _is_master = true;
    }

    bool eq_init(const player_init_data &init_data,
            int *src_width, int *src_height, float *src_aspect_ratio,
            enum decoder::video_frame_format *src_preferred_frame_format)
    {
        try
        {
            set_benchmark(init_data.benchmark);
            reset_playstate();
            create_decoders(init_data.filenames);
            create_input(init_data.input_mode);
            get_input_info(src_width, src_height, src_aspect_ratio, src_preferred_frame_format);
            _fmt = *src_preferred_frame_format;
            if (_is_master)
            {
                create_audio_output();
            }
        }
        catch (std::exception &e)
        {
            msg::err("%s", e.what());
            return false;
        }
        return true;
    }

    video_output_state &eq_video_state()
    {
        return video_state();
    }

    void eq_seek_to(int64_t seek_to)
    {
        get_input()->seek(seek_to);
    }

    void eq_read_frame()        // Only called on slave nodes to force reading the next frame
    {
        if (get_input()->read_video_frame() < 0)
        {
            msg::err("reading input frame failed (EOF?)");
            abort();
        }
    }

    void eq_prep_frame(video_output *vo)
    {
        prepare_video_frame(vo);
    }

    void eq_run_step(bool *more_steps, int64_t *seek_to, bool *prep_frame, bool *drop_frame, bool *display_frame)
    {
        run_step(more_steps, seek_to, prep_frame, drop_frame, display_frame);
    }

    void eq_get_frame()
    {
        get_video_frame(_fmt);
    }

    void eq_release_frame()
    {
        release_video_frame();
    }
};

/*
 * video_output_opengl_eq_window
 *
 * Implementation of video_output_opengl for eq_window
 */

class video_output_opengl_eq_window : public video_output_opengl
{
public:
    video_output_opengl_eq_window() : video_output_opengl(false)
    {
    }

    void eq_display(enum video_output::mode mode, float x, float y, float w, float h)
    {
        video_output_opengl::display(mode, x, y, w, h);
    }

    void eq_initialize(int src_width, int src_height, float src_aspect_ratio,
            enum decoder::video_frame_format src_preferred_frame_format,
            bool have_pixel_buffer_object, bool have_texture_non_power_of_two, bool have_fragment_shader)
    {
        set_mode(stereo);       // just to ensure that prepare() handles both left and right view
        set_source_info(src_width, src_height, src_aspect_ratio, src_preferred_frame_format);
        initialize(have_pixel_buffer_object, have_texture_non_power_of_two, have_fragment_shader);
    }

    void eq_deinitialize()
    {
        deinitialize();
    }

    void eq_swap_tex_set()
    {
        swap_tex_set();
    }

    void eq_set_state(const video_output_state &video_state)
    {
        set_state(video_state);
    }

    // The rest of the video_output_opengl interface is irrelevant for Equalizer
    virtual int window_pos_x() { return 0; }
    virtual int window_pos_y() { return 0; }
    virtual void receive_notification(const notification &) {}
    virtual bool supports_stereo() { return false; }
    virtual void open(enum decoder::video_frame_format, int, int, float, int, const video_output_state&, unsigned int, int, int) {}
    virtual void activate() {}
    virtual void process_events() {}
    virtual void close() {}
};

/*
 * eq_init_data
 */

class eq_init_data : public eq::net::Object
{
public:
    uint32_t frame_data_id;
    player_init_data init_data;
    bool flat_screen;
    struct { float x, y, w, h, d; } canvas_video_area;

    eq_init_data() : frame_data_id(EQ_ID_INVALID), init_data()
    {
        flat_screen = true;
        canvas_video_area.x = 0.0f;
        canvas_video_area.y = 0.0f;
        canvas_video_area.w = 1.0f;
        canvas_video_area.h = 1.0f;
        canvas_video_area.d = 1.0f;
    }

    virtual ~eq_init_data()
    {
        frame_data_id = EQ_ID_INVALID;
    }

protected:
    virtual ChangeType getChangeType() const
    {
        return eq::net::Object::STATIC;
    }

    virtual void getInstanceData(eq::net::DataOStream &os)
    {
        std::ostringstream oss;
        s11n::save(oss, frame_data_id);
        s11n::save(oss, static_cast<int>(init_data.log_level));
        s11n::save(oss, init_data.filenames);
        s11n::save(oss, static_cast<int>(init_data.input_mode));
        s11n::save(oss, static_cast<int>(init_data.video_mode));
        s11n::save(oss, init_data.video_state.contrast);
        s11n::save(oss, init_data.video_state.brightness);
        s11n::save(oss, init_data.video_state.hue);
        s11n::save(oss, init_data.video_state.saturation);
        s11n::save(oss, init_data.video_state.fullscreen);
        s11n::save(oss, init_data.video_state.swap_eyes);
        s11n::save(oss, init_data.video_flags);
        s11n::save(oss, flat_screen);
        s11n::save(oss, canvas_video_area.x);
        s11n::save(oss, canvas_video_area.y);
        s11n::save(oss, canvas_video_area.w);
        s11n::save(oss, canvas_video_area.h);
        s11n::save(oss, canvas_video_area.d);
        os << oss.str();
    }

    virtual void applyInstanceData(eq::net::DataIStream &is)
    {
        int x;
        std::string s;
        is >> s;
        std::istringstream iss(s);
        s11n::load(iss, frame_data_id);
        s11n::load(iss, x);
        init_data.log_level = static_cast<msg::level_t>(x);
        s11n::load(iss, init_data.filenames);
        s11n::load(iss, x);
        init_data.input_mode = static_cast<enum input::mode>(x);
        s11n::load(iss, x);
        init_data.video_mode = static_cast<enum video_output::mode>(x);
        s11n::load(iss, init_data.video_state.contrast);
        s11n::load(iss, init_data.video_state.brightness);
        s11n::load(iss, init_data.video_state.hue);
        s11n::load(iss, init_data.video_state.saturation);
        s11n::load(iss, init_data.video_state.fullscreen);
        s11n::load(iss, init_data.video_state.swap_eyes);
        s11n::load(iss, init_data.video_flags);
        s11n::load(iss, flat_screen);
        s11n::load(iss, canvas_video_area.x);
        s11n::load(iss, canvas_video_area.y);
        s11n::load(iss, canvas_video_area.w);
        s11n::load(iss, canvas_video_area.h);
        s11n::load(iss, canvas_video_area.d);
    }
};

/*
 * eq_frame_data
 */

class eq_frame_data : public eq::net::Object
{
public:
    video_output_state video_state;
    int64_t seek_to;
    bool prep_frame;
    bool drop_frame;
    bool display_frame;

public:
    eq_frame_data()
        : video_state(), seek_to(-1),
        prep_frame(false), drop_frame(false), display_frame(false)
    {
    }

protected:
    virtual ChangeType getChangeType() const
    {
        return eq::net::Object::INSTANCE;
    }

    virtual void getInstanceData(eq::net::DataOStream &os)
    {
        std::ostringstream oss;
        s11n::save(oss, video_state.contrast);
        s11n::save(oss, video_state.brightness);
        s11n::save(oss, video_state.hue);
        s11n::save(oss, video_state.saturation);
        s11n::save(oss, video_state.fullscreen);
        s11n::save(oss, video_state.swap_eyes);
        s11n::save(oss, seek_to);
        s11n::save(oss, prep_frame);
        s11n::save(oss, drop_frame);
        s11n::save(oss, display_frame);
        os << oss.str();
    }

    virtual void applyInstanceData(eq::net::DataIStream &is)
    {
        std::string s;
        is >> s;
        std::istringstream iss(s);
        s11n::load(iss, video_state.contrast);
        s11n::load(iss, video_state.brightness);
        s11n::load(iss, video_state.hue);
        s11n::load(iss, video_state.saturation);
        s11n::load(iss, video_state.fullscreen);
        s11n::load(iss, video_state.swap_eyes);
        s11n::load(iss, seek_to);
        s11n::load(iss, prep_frame);
        s11n::load(iss, drop_frame);
        s11n::load(iss, display_frame);
    }
};

/*
 * eq_config
 */

class eq_config : public eq::Config
{
private:
    bool _is_master_config;
    eq_init_data _eq_init_data;         // Master eq_init_data instance
    eq_frame_data _eq_frame_data;       // Master eq_frame_data instance
    player_eq_node _player;             // Master player
    controller _controller;             // Sends commands to the player

public:
    // Source video properties:
    int src_width, src_height;
    float src_aspect_ratio;
    enum decoder::video_frame_format src_preferred_frame_format;

public:
    eq_config(eq::ServerPtr parent)
        : eq::Config(parent), _is_master_config(false), _eq_init_data(), _eq_frame_data(),
        _player(), _controller(false),
        src_width(-1), src_height(-1), src_aspect_ratio(0.0f),
        src_preferred_frame_format(decoder::frame_format_yuv420p)
    {
    }

    bool is_master_config()
    {
        return _is_master_config;
    }

    bool init(const player_init_data &init_data, bool flat_screen)
    {
        msg::set_level(init_data.log_level);
        msg::dbg(HERE);
        // If this function is called, then this is the master config
        _is_master_config = true;
        // Initialize master init/frame data instances
        _eq_init_data.init_data = init_data;
        _eq_init_data.flat_screen = flat_screen;
        _eq_frame_data.video_state = _eq_init_data.init_data.video_state;
        // Initialize master player
        _player.eq_make_master();
        if (!_player.eq_init(init_data, &src_width, &src_height, &src_aspect_ratio, &src_preferred_frame_format))
        {
            return false;
        }
        // Find region of canvas to use, depending on the video aspect ratio
        if (getCanvases().size() < 1)
        {
            msg::err("no canvas in Equalizer configuration");
            abort();
        }
        float canvas_w = getCanvases()[0]->getWall().getWidth();
        float canvas_h = getCanvases()[0]->getWall().getHeight();
        float canvas_aspect_ratio = canvas_w / canvas_h;
        _eq_init_data.canvas_video_area.w = 1.0f;
        _eq_init_data.canvas_video_area.h = 1.0f;

        if (flat_screen)
        {
            if (src_aspect_ratio > canvas_aspect_ratio)
            {
                // need black borders top and bottom
                _eq_init_data.canvas_video_area.h = canvas_aspect_ratio / src_aspect_ratio;
            }
            else
            {
                // need black borders left and right
                _eq_init_data.canvas_video_area.w = src_aspect_ratio / canvas_aspect_ratio;
            }
            _eq_init_data.canvas_video_area.x = (1.0f - _eq_init_data.canvas_video_area.w) / 2.0f;
            _eq_init_data.canvas_video_area.y = (1.0f - _eq_init_data.canvas_video_area.h) / 2.0f;
        }
        else
        {
            compute_3d_canvas(&_eq_init_data.canvas_video_area.h, &_eq_init_data.canvas_video_area.d);
            // compute width and offset for 1m high 'screen' quad in 3D space
            _eq_init_data.canvas_video_area.w = _eq_init_data.canvas_video_area.h * src_aspect_ratio;
            _eq_init_data.canvas_video_area.x = -0.5f * _eq_init_data.canvas_video_area.w;
            _eq_init_data.canvas_video_area.y = -0.5f * _eq_init_data.canvas_video_area.h;
        }
        msg::inf("equalizer canvas:");
        msg::inf("    %gx%g, aspect ratio %g:1", canvas_w, canvas_h, canvas_w / canvas_h);
        msg::inf("    area for %g:1 video: [ %g %g %g %g @ %g ]", src_aspect_ratio,
                _eq_init_data.canvas_video_area.x, _eq_init_data.canvas_video_area.y,
                _eq_init_data.canvas_video_area.w, _eq_init_data.canvas_video_area.h,
                _eq_init_data.canvas_video_area.d);
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
        _eq_init_data.frame_data_id = EQ_ID_INVALID;
        // Cleanup
        _player.close();
        msg::dbg(HERE);
        return ret;
    }

    virtual uint32_t startFrame()
    {
        // Run one player step to find out what to do
        bool more_steps;
        _player.eq_run_step(&more_steps, &_eq_frame_data.seek_to,
                &_eq_frame_data.prep_frame, &_eq_frame_data.drop_frame, &_eq_frame_data.display_frame);
        if (!more_steps)
        {
            this->exit();
        }
        // Update the video state for all (it might have changed via handleEvent())
        _eq_frame_data.video_state = _player.eq_video_state();
        // Commit the updated frame data
        const uint32_t version = _eq_frame_data.commit();
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
            case 'q':
                _controller.send_cmd(command::toggle_play);
                break;
            case 's':
                _controller.send_cmd(command::toggle_swap_eyes);
                break;
            case 'f':
                /* fullscreen toggling not supported with Equalizer */
                break;
            case 'c':
                /* window centering not supported with Equalizer */
                break;
            case ' ':
            case 'p':
                _controller.send_cmd(command::toggle_pause);
                break;
            case '1':
                _controller.send_cmd(command::adjust_contrast, -0.05f);
                break;
            case '2':
                _controller.send_cmd(command::adjust_contrast, +0.05f);
                break;
            case '3':
                _controller.send_cmd(command::adjust_brightness, -0.05f);
                break;
            case '4':
                _controller.send_cmd(command::adjust_brightness, +0.05f);
                break;
            case '5':
                _controller.send_cmd(command::adjust_hue, -0.05f);
                break;
            case '6':
                _controller.send_cmd(command::adjust_hue, +0.05f);
                break;
            case '7':
                _controller.send_cmd(command::adjust_saturation, -0.05f);
                break;
            case '8':
                _controller.send_cmd(command::adjust_saturation, +0.05f);
                break;
            case eq::KC_LEFT:
                _controller.send_cmd(command::seek, -10.0f);
                break;
            case eq::KC_RIGHT:
                _controller.send_cmd(command::seek, +10.0f);
                break;
            case eq::KC_UP:
                _controller.send_cmd(command::seek, -60.0f);
                break;
            case eq::KC_DOWN:
                _controller.send_cmd(command::seek, +60.0f);
                break;
            case eq::KC_PAGE_UP:
                _controller.send_cmd(command::seek, -600.0f);
                break;
            case eq::KC_PAGE_DOWN:
                _controller.send_cmd(command::seek, +600.0f);
                break;
            }
        }
        return true;
    }

    player_eq_node *player()
    {
        return &_player;
    }

private:
    void compute_3d_canvas(float *height, float *distance)
    {
        float angle = -1.0f;
        *height = 0.0f;
        *distance = 0.0f;

        const eq::CanvasVector &canvases = getCanvases();
        for (eq::CanvasVector::const_iterator i = canvases.begin(); i != canvases.end(); i++)
        {
            const eq::SegmentVector &segments = (*i)->getSegments();
            for (eq::SegmentVector::const_iterator j = segments.begin(); j != segments.end(); j++)
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
    bool _is_app_node;
    player_eq_node _player;

public:
    eq_init_data init_data;
    eq_frame_data frame_data;
    int src_width, src_height;
    float src_aspect_ratio;
    enum decoder::video_frame_format src_preferred_frame_format;

    eq_node(eq::Config *parent)
        : eq::Node(parent), _is_app_node(false),
        _player(), init_data(), frame_data(),
        src_width(-1), src_height(-1), src_aspect_ratio(-1.0f),
        src_preferred_frame_format(decoder::frame_format_yuv420p)
    {
    }

protected:
    virtual bool configInit(const uint32_t initID)
    {
        if (!eq::Node::configInit(initID))
        {
            return false;
        }
        // Map our InitData instance to the master instance
        eq_config *config = static_cast<eq_config *>(getConfig());
        if (!config->mapObject(&init_data, initID))
        {
            return false;
        }
        // Is this the application node?
        if (config->is_master_config())
        {
            _is_app_node = true;
        }
        // Map our FrameData instance to the master instance
        if (!config->mapObject(&frame_data, init_data.frame_data_id))
        {
            return false;
        }

        msg::set_level(init_data.init_data.log_level);
        msg::dbg(HERE);
        // Create decoders and input
        if (!_is_app_node)
        {
            if (!_player.eq_init(init_data.init_data, &src_width, &src_height, &src_aspect_ratio, &src_preferred_frame_format))
            {
                return false;
            }
        }
        else
        {
            src_width = config->src_width;
            src_height = config->src_height;
            src_aspect_ratio = config->src_aspect_ratio;
            src_preferred_frame_format = config->src_preferred_frame_format;
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
        _player.close();
        msg::dbg(HERE);
        return eq::Node::configExit();
    }

    virtual void frameStart(const uint32_t frameID, const uint32_t frameNumber)
    {
        // Update our frame data
        frame_data.sync(frameID);
        // Do as we're told
        if (_is_app_node)
        {
            eq_config *config = static_cast<eq_config *>(getConfig());
            if (frame_data.prep_frame)
            {
                config->player()->eq_get_frame();
            }
            if (frame_data.drop_frame)
            {
                config->player()->eq_release_frame();
            }
        }
        else
        {
            if (frame_data.seek_to >= 0)
            {
                _player.eq_seek_to(frame_data.seek_to);
            }
            if (frame_data.prep_frame)
            {
                _player.eq_read_frame();
                _player.eq_get_frame();
            }
            if (frame_data.drop_frame)
            {
                _player.eq_read_frame();
                _player.eq_release_frame();
            }
        }
        startFrame(frameNumber);
    }

    virtual void frameFinish(const uint32_t, const uint32_t frameNumber)
    {
        // Do as we're told
        if (_is_app_node)
        {
            eq_config *config = static_cast<eq_config *>(getConfig());
            if (frame_data.prep_frame)
            {
                config->player()->eq_release_frame();
            }
        }
        else
        {
            if (frame_data.prep_frame)
            {
                _player.eq_release_frame();
            }
        }
        releaseFrame(frameNumber);
    }

public:
    void prep_frame(video_output *vo, enum decoder::video_frame_format fmt)
    {
        if (fmt != src_preferred_frame_format)
        {
            msg::err("cannot provide video in requested frame format");
            abort();
        }
        if (_is_app_node)
        {
            eq_config *config = static_cast<eq_config *>(getConfig());
            config->player()->eq_prep_frame(vo);
        }
        else
        {
            _player.eq_prep_frame(vo);
        }
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
private:
    video_output_opengl_eq_window _video_output;

public:
    eq_window(eq::Pipe *parent) : eq::Window(parent), _video_output()
    {
    }

    void display(enum video_output::mode mode, float x, float y, float w, float h)
    {
        _video_output.eq_display(mode, x, y, w, h);
    }

protected:
    virtual bool configInitGL(const uint32_t initID)
    {
        msg::dbg(HERE);
        if (!eq::Window::configInitGL(initID))
        {
            return false;
        }
        eq_node *node = static_cast<eq_node *>(getNode());

        // Disable some things that Equalizer seems to enable for some reason.
        glDisable(GL_LIGHTING);

        _video_output.eq_initialize(node->src_width, node->src_height,
                node->src_aspect_ratio, node->src_preferred_frame_format,
                GLEW_ARB_pixel_buffer_object, GLEW_ARB_texture_non_power_of_two, GLEW_ARB_fragment_shader);

        msg::dbg(HERE);
        return true;
    }

    virtual bool configExitGL()
    {
        msg::dbg(HERE);
        _video_output.eq_deinitialize();
        msg::dbg(HERE);
        return eq::Window::configExitGL();
    }

    virtual void frameStart(const uint32_t, const uint32_t frameNumber)
    {
        // Get frame data via from the node
        eq_node *node = static_cast<eq_node *>(getNode());
        _video_output.eq_set_state(node->frame_data.video_state);
        // Do as we're told
        if (node->frame_data.prep_frame)
        {
            makeCurrent();      // XXX Is this necessary?
            node->prep_frame(&_video_output, _video_output.frame_format());
        }
        if (node->frame_data.display_frame)
        {
            makeCurrent();      // XXX Is this necessary?
            _video_output.eq_swap_tex_set();
        }
        startFrame(frameNumber);
    }

    virtual void frameFinish(const uint32_t, const uint32_t frameNumber)
    {
        releaseFrame(frameNumber);
    }
};

/*
 * eq_channel
 */

class eq_channel : public eq::Channel
{
private:

public:
    eq_channel(eq::Window *parent) : eq::Channel(parent)
    {
    }

protected:
    virtual void frameDraw(const uint32_t frameID)
    {
        // Let Equalizer initialize some stuff
        eq::Channel::frameDraw(frameID);
        // Get the canvas video area and the canvas channel area
        eq_node *node = static_cast<eq_node *>(getNode());
        const struct { float x, y, w, h, d; } canvas_video_area =
        {
            node->init_data.canvas_video_area.x,
            node->init_data.canvas_video_area.y,
            node->init_data.canvas_video_area.w,
            node->init_data.canvas_video_area.h,
            node->init_data.canvas_video_area.d
        };
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
        eq_window *window = static_cast<eq_window *>(getWindow());
        window->display(getEye() == eq::EYE_RIGHT ? video_output::mono_right : video_output::mono_left,
                quad_x, quad_y, quad_w, quad_h);
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

player_equalizer::player_equalizer(int *argc, char *argv[], bool flat_screen)
    : player(player::slave), _flat_screen(flat_screen)
{
    /* Initialize Equalizer */
    _node_factory = static_cast<void *>(new eq_node_factory);
    if (!eq::init(*argc, argv, static_cast<eq::NodeFactory *>(_node_factory)))
    {
        throw exc("equalizer initialization failed");
    }
    /* Get a configuration */
    _config = static_cast<void *>(eq::getConfig(*argc, argv));
    // The following code is only executed on the application node because
    // eq::getConfig() does not return on other nodes.
    if (!_config)
    {
        throw exc("cannot get equalizer configuration");
    }
}

player_equalizer::~player_equalizer()
{
    delete static_cast<eq_node_factory *>(_node_factory);
}

void player_equalizer::open(const player_init_data &init_data)
{
    eq_config *config = static_cast<eq_config *>(_config);
    if (!config->init(init_data, _flat_screen))
    {
        throw exc("equalizer configuration initialization failed");
    }
}

void player_equalizer::run()
{
    eq_config *config = static_cast<eq_config *>(_config);
    while (config->isRunning())
    {
        config->startFrame();
        config->finishFrame();
    }
    config->exit();
    eq::releaseConfig(config);
    eq::exit();
}

void player_equalizer::close()
{
}

std::vector<std::string> equalizer_versions()
{
    std::vector<std::string> v;
    v.push_back(str::asprintf("Equalizer %d.%d.%d / %d.%d.%d",
                EQ_VERSION_MAJOR, EQ_VERSION_MINOR, EQ_VERSION_PATCH,
                static_cast<int>(eq::Version::getMajor()),
                static_cast<int>(eq::Version::getMinor()),
                static_cast<int>(eq::Version::getPatch())));
    return v;
}
