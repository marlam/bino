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

// input_rgb24 or input_yuv420p
#define $input

// mode_onechannel, mode_anaglyph_monochrome, mode_anaglyph_full_color,
// mode_anaglyph_half_color, or mode_anaglyph_dubois
#define $mode

#if defined(input_rgb24)
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

uniform float contrast;
uniform float brightness;
uniform float saturation;
uniform float cos_hue;
uniform float sin_hue;

#if defined(input_rgb24)
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

void main()
{
    vec3 yuv_l;
#if defined(input_rgb24)
    yuv_l = rgb_to_yuv(texture2D(rgb_l, gl_TexCoord[0].xy).xyz);
#elif defined(input_yuv420p)
    yuv_l = vec3(
            texture2D(y_l, gl_TexCoord[0].xy).x,
            texture2D(u_l, gl_TexCoord[0].xy).x,
            texture2D(v_l, gl_TexCoord[0].xy).x);
#endif
    yuv_l = adjust_yuv(yuv_l);

#if !defined(mode_onechannel)
    vec3 yuv_r;
# if defined(input_rgb24)
    yuv_r = rgb_to_yuv(texture2D(rgb_r, gl_TexCoord[0].xy).xyz);
# elif defined(input_yuv420p)
    yuv_r = vec3(
            texture2D(y_r, gl_TexCoord[0].xy).x,
            texture2D(u_r, gl_TexCoord[0].xy).x,
            texture2D(v_r, gl_TexCoord[0].xy).x);
# endif
    yuv_r = adjust_yuv(yuv_r);
#endif

    vec3 rgb;
#if defined(mode_onechannel)
    rgb = yuv_to_rgb(yuv_l);
#elif defined(mode_anaglyph_monochrome)
    rgb = vec3(yuv_l.x, yuv_r.x, yuv_r.x);
#elif defined(mode_anaglyph_full_color)
    rgb = vec3(yuv_to_rgb(yuv_l).r, yuv_to_rgb(yuv_r).gb);
#elif defined(mode_anaglyph_half_color)
    rgb = vec3(yuv_l.x, yuv_to_rgb(yuv_r).gb);
#elif defined(mode_anaglyph_dubois)
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
#endif

    gl_FragColor = vec4(rgb, 1.0);
}
