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

#version 110

// input_bgra32
// input_yuv420p
#define $input

// mode_onechannel
// mode_anaglyph_monochrome
// mode_anaglyph_full_color
// mode_anaglyph_half_color
// mode_anaglyph_dubois
// mode_even_odd_rows
// mode_even_odd_columns
// mode_checkerboard
#define $mode

#if defined(input_bgra32)
uniform sampler2D rgb_l;
# if !defined(mode_onechannel)
uniform sampler2D rgb_r;
# endif
#elif defined(input_yuv420p)
uniform sampler2D y_l;
uniform sampler2D u_l;
uniform sampler2D v_l;
# if !defined(mode_onechannel)
uniform sampler2D y_r;
uniform sampler2D u_r;
uniform sampler2D v_r;
# endif
#endif

#if defined(mode_even_odd_rows) || defined(mode_even_odd_columns) || defined(mode_checkerboard)
uniform sampler2D mask_tex;
uniform float step_x;
uniform float step_y;
#endif

uniform float contrast;
uniform float brightness;
uniform float saturation;
uniform float cos_hue;
uniform float sin_hue;

#if defined(input_bgra32)
vec3 rgb_to_yuv(vec3 rgb)
{
    // Values taken from http://www.fourcc.org/fccyvrgb.php
    float y = dot(rgb, vec3(0.299, 0.587, 0.114));
    float u = 0.564 * (rgb.b - y) + 0.5;
    float v = 0.713 * (rgb.r - y) + 0.5;
    return vec3(y, u, v);
}
#endif

vec3 yuv_to_rgb(vec3 yuv)
{
    // Values taken from http://www.fourcc.org/fccyvrgb.php
    float r = yuv.x                         + 1.403 * (yuv.z - 0.5);
    float g = yuv.x - 0.344 * (yuv.y - 0.5) - 0.714 * (yuv.z - 0.5);
    float b = yuv.x + 1.770 * (yuv.y - 0.5);
    return vec3(r, g, b);
}

vec3 adjust_yuv(vec3 yuv)
{
    // Adapted from http://www.silicontrip.net/~mark/lavtools/yuvadjust.c
    // (Copyright 2002 Alfonso Garcia-Patiño Barbolani, released under GPLv2 or later)

    // brightness and contrast
    float ay = (yuv.x - 0.5) * (contrast + 1.0) + brightness + 0.5;
    // hue and saturation
    float au = (cos_hue * (yuv.y - 0.5) - sin_hue * (yuv.z - 0.5)) * (saturation + 1.0) + 0.5;
    float av = (sin_hue * (yuv.y - 0.5) + cos_hue * (yuv.z - 0.5)) * (saturation + 1.0) + 0.5;

    return vec3(ay, au, av);
}

vec3 get_yuv_l(vec2 tex_coord)
{
#if defined(input_bgra32)
    return rgb_to_yuv(texture2D(rgb_l, tex_coord).xyz);
#elif defined(input_yuv420p)
    return vec3(
            texture2D(y_l, tex_coord).x,
            texture2D(u_l, tex_coord).x,
            texture2D(v_l, tex_coord).x);
#endif
}

#if !defined(mode_onechannel)
vec3 get_yuv_r(vec2 tex_coord)
{
# if defined(input_bgra32)
    return rgb_to_yuv(texture2D(rgb_r, tex_coord).xyz);
# elif defined(input_yuv420p)
    return vec3(
            texture2D(y_r, tex_coord).x,
            texture2D(u_r, tex_coord).x,
            texture2D(v_r, tex_coord).x);
# endif
}
#endif

void main()
{
    vec3 rgb;

#if defined(mode_even_odd_rows) || defined(mode_even_odd_columns) || defined(mode_checkerboard)

    /* This implementation of the masked modes works around many different problems and therefore may seem strange.
     * Why not use stencil buffers?
     *  - Because we want to filter, to account for masked out features
     *  - Because stencil did not work with some drivers when switching fullscreen on/off
     * Why not use polygon stipple?
     *  - Because we want to filter, to account for masked out features
     *  - Because polygon stippling may not be hardware accelerated and is currently broken with many free drivers
     * Why use a mask texture? Why not use the mod() function to check for even/odd pixels?
     *  - Because mod() is broken with many drivers, and I found no reliable way to work around it. Some
     *    drivers seem to use extremely low precision arithmetic in the shaders; too low for reliable pixel
     *    position computations.
     * Why use local variables in the if/else branches, and a local computation of the rgb value?
     *  - To work around driver bugs.
     */
    float m = texture2D(mask_tex, gl_TexCoord[1].xy).x;
# if defined(mode_even_odd_rows)
    if (m > 0.5)
    {
        vec3 yuv0 = get_yuv_l(gl_TexCoord[0].xy - vec2(0.0, step_y));
        vec3 yuv1 = get_yuv_l(gl_TexCoord[0].xy);
        vec3 yuv2 = get_yuv_l(gl_TexCoord[0].xy + vec2(0.0, step_y));
        rgb = yuv_to_rgb(adjust_yuv((yuv0 + 2.0 * yuv1 + yuv2) / 4.0));
    }
    else
    {
        vec3 yuv0 = get_yuv_r(gl_TexCoord[0].xy - vec2(0.0, step_y));
        vec3 yuv1 = get_yuv_r(gl_TexCoord[0].xy);
        vec3 yuv2 = get_yuv_r(gl_TexCoord[0].xy + vec2(0.0, step_y));
        rgb = yuv_to_rgb(adjust_yuv((yuv0 + 2.0 * yuv1 + yuv2) / 4.0));
    }
# elif defined(mode_even_odd_columns)
    if (m > 0.5)
    {
        vec3 yuv0 = get_yuv_l(gl_TexCoord[0].xy - vec2(step_x, 0.0));
        vec3 yuv1 = get_yuv_l(gl_TexCoord[0].xy);
        vec3 yuv2 = get_yuv_l(gl_TexCoord[0].xy + vec2(step_x, 0.0));
        rgb = yuv_to_rgb(adjust_yuv((yuv0 + 2.0 * yuv1 + yuv2) / 4.0));
    }
    else
    {
        vec3 yuv0 = get_yuv_r(gl_TexCoord[0].xy - vec2(step_x, 0.0));
        vec3 yuv1 = get_yuv_r(gl_TexCoord[0].xy);
        vec3 yuv2 = get_yuv_r(gl_TexCoord[0].xy + vec2(step_x, 0.0));
        rgb = yuv_to_rgb(adjust_yuv((yuv0 + 2.0 * yuv1 + yuv2) / 4.0));
    }
# elif defined(mode_checkerboard)
    if (m > 0.5)
    {
        vec3 yuv0 = get_yuv_l(gl_TexCoord[0].xy - vec2(0.0, step_y));
        vec3 yuv1 = get_yuv_l(gl_TexCoord[0].xy - vec2(step_x, 0.0));
        vec3 yuv2 = get_yuv_l(gl_TexCoord[0].xy);
        vec3 yuv3 = get_yuv_l(gl_TexCoord[0].xy + vec2(step_x, 0.0));
        vec3 yuv4 = get_yuv_l(gl_TexCoord[0].xy + vec2(0.0, step_y));
        rgb = yuv_to_rgb(adjust_yuv((yuv0 + yuv1 + 4.0 * yuv2 + yuv3 + yuv4) / 8.0));
    }
    else
    {
        vec3 yuv0 = get_yuv_r(gl_TexCoord[0].xy - vec2(0.0, step_y));
        vec3 yuv1 = get_yuv_r(gl_TexCoord[0].xy - vec2(step_x, 0.0));
        vec3 yuv2 = get_yuv_r(gl_TexCoord[0].xy);
        vec3 yuv3 = get_yuv_r(gl_TexCoord[0].xy + vec2(step_x, 0.0));
        vec3 yuv4 = get_yuv_r(gl_TexCoord[0].xy + vec2(0.0, step_y));
        rgb = yuv_to_rgb(adjust_yuv((yuv0 + yuv1 + 4.0 * yuv2 + yuv3 + yuv4) / 8.0));
    }
# endif

#else

    vec3 yuv_l = adjust_yuv(get_yuv_l(gl_TexCoord[0].xy));
# if !defined(mode_onechannel)
    vec3 yuv_r = adjust_yuv(get_yuv_r(gl_TexCoord[0].xy));
# endif

# if defined(mode_onechannel)
    rgb = yuv_to_rgb(yuv_l);
# elif defined(mode_anaglyph_monochrome)
    rgb = vec3(yuv_l.x, yuv_r.x, yuv_r.x);
# elif defined(mode_anaglyph_full_color)
    rgb = vec3(yuv_to_rgb(yuv_l).r, yuv_to_rgb(yuv_r).gb);
# elif defined(mode_anaglyph_half_color)
    rgb = vec3(yuv_l.x, yuv_to_rgb(yuv_r).gb);
# elif defined(mode_anaglyph_dubois)
    // Dubois anaglyph method.
    // Authors page: http://www.site.uottawa.ca/~edubois/anaglyph/
    // This method depends on the characteristics of the display device
    // and the anaglyph glasses.
    // The matrices below are taken from "A Uniform Metric for Anaglyph Calculation"
    // by Zhe Zhang and David F. McAllister, Proc. Electronic Imaging 2006, available
    // here: http://research.csc.ncsu.edu/stereographics/ei06.pdf
    // These matrices are supposed to work fine for LCD monitors and most red-cyan
    // glasses. See also the remarks in http://research.csc.ncsu.edu/stereographics/LS.pdf
    // (where slightly different values are used).
    mat3 m0 = mat3(
            0.4155, -0.0458, -0.0545,
            0.4710, -0.0484, -0.0614,
            0.1670, -0.0258,  0.0128);
    mat3 m1 = mat3(
            -0.0109, 0.3756, -0.0651,
            -0.0365, 0.7333, -0.1286,
            -0.0060, 0.0111,  1.2968);
    rgb = m0 * yuv_to_rgb(yuv_l) + m1 * yuv_to_rgb(yuv_r);
# endif

#endif

    gl_FragColor = vec4(rgb, 1.0);
}
