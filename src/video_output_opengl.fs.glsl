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

#if defined input_rgb24
uniform sampler2D rgb_l;
# if !defined mode_onechannel
uniform sampler2D rgb_r;
# endif
#elif defined input_yuv420p
uniform sampler2D y_l;
uniform sampler2D u_l;
uniform sampler2D v_l;
# if !defined mode_onechannel
uniform sampler2D y_r;
uniform sampler2D u_r;
uniform sampler2D v_r;
# endif
#endif

uniform float contrast;
uniform float brightness;
uniform float hue;
uniform float saturation;

const float pi = 3.14159265358979323846;

#if defined input_rgb24
vec3 rgb_to_yuv(vec3 rgb)
{
    mat3 m = mat3(
            0.299, -0.14713,  0.615,
            0.587, -0.28886, -0.51499,
            0.114,  0.436,   -0.10001);
    return m * rgb;
}
#endif

vec3 yuv_to_rgb(vec3 yuv)
{
    float a = 1.1643 * (yuv.x - 0.0625);
    float b = yuv.y - 0.5;
    float c = yuv.z - 0.5;
    return vec3(
            a + 1.5958 * c,
            a - 0.39173 * b - 0.81290 * c,
            a + 2.016 * b);
}

vec3 adjust_yuv(vec3 yuv)
{
    // Adapted from http://www.silicontrip.net/~mark/lavtools/yuvadjust.c
    // (Copyright 2002 Alfonso Garcia-Patiño Barbolani, released under GPLv2 or later)
    
    // Note: we don't clamp here, since the rgb values computed from these yuv values
    // will be clamped automatically. Should we clamp anyway?

    const float adj_con_cen = 0.5;
    const float adj_u = 0.0;
    const float adj_v = 0.0;
    float y, u, v;

    // brightness and contrast operation
    y = yuv.x - adj_con_cen;
    y = y * (contrast + 1.0) + brightness + adj_con_cen;
    
    u = yuv.y - 0.5;
    v = yuv.z - 0.5;

    // hue rotation, saturation and shift
    float h = hue * pi;
    float cos_hue = cos(h);
    float sin_hue = sin(h);
    float nu = cos_hue * u - sin_hue * v * (saturation + 1.0) + adj_v;
    float nv = sin_hue * u + cos_hue * v * (saturation + 1.0) + adj_u;
    u = nu + 0.5;
    v = nv + 0.5;

    return vec3(y, u, v);
}

void main()
{
    vec3 yuv_l;
#if defined input_rgb24
    yuv_l = rgb_to_yuv(texture2D(rgb_l, gl_TexCoord[0].xy).xyz);
#elif defined input_yuv420p
    yuv_l = vec3(
            texture2D(y_l, gl_TexCoord[0].xy).x,
            texture2D(u_l, gl_TexCoord[0].xy).x,
            texture2D(v_l, gl_TexCoord[0].xy).x);
    yuv_l = adjust_yuv(yuv_l);
#endif

#if !defined mode_onechannel
    vec3 yuv_r;
# if defined input_rgb24
    yuv_r = rgb_to_yuv(texture2D(rgb_r, gl_TexCoord[0].xy).xyz);
# elif defined input_yuv420p
    yuv_r = vec3(
            texture2D(y_r, gl_TexCoord[0].xy).x,
            texture2D(u_r, gl_TexCoord[0].xy).x,
            texture2D(v_r, gl_TexCoord[0].xy).x);
# endif
    yuv_r = adjust_yuv(yuv_r);
#endif

    vec3 rgb;
#if defined mode_onechannel
    rgb = yuv_to_rgb(yuv_l);
#elif defined mode_anaglyph_monochrome
    rgb = vec3(yuv_l.x, yuv_r.x, yuv_r.x);
#elif defined mode_anaglyph_full_color
    rgb = vec3(yuv_to_rgb(yuv_l).r, yuv_to_rgb(yuv_r).gb);
#elif defined mode_anaglyph_half_color
    rgb = vec3(yuv_l.x, yuv_to_rgb(yuv_r).gb);
#elif defined mode_anaglyph_dubois
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
