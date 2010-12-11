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

#version 120

// input_bgra32
// input_yuv420p
#define $input

#if defined(input_yuv420p)
uniform sampler2D y_tex;
uniform sampler2D u_tex;
uniform sampler2D v_tex;
#elif defined(input_bgra32)
uniform sampler2D srgb_tex;
#endif

uniform float contrast;
uniform float brightness;
uniform float saturation;
uniform float cos_hue;
uniform float sin_hue;

#if defined(input_bgra32)
vec3 srgb_to_yuv(vec3 srgb)
{
    // Values taken from http://www.fourcc.org/fccyvrgb.php
    float y = dot(srgb, vec3(0.299, 0.587, 0.114));
    float u = 0.564 * (srgb.b - y) + 0.5;
    float v = 0.713 * (srgb.r - y) + 0.5;
    return vec3(y, u, v);
}
#endif

vec3 yuv_to_srgb(vec3 yuv)
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

vec3 get_yuv(vec2 tex_coord)
{
#if defined(input_bgra32)
    return srgb_to_yuv(texture2D(srgb_tex, tex_coord).xyz);
#elif defined(input_yuv420p)
    return vec3(
            texture2D(y_tex, tex_coord).x,
            texture2D(u_tex, tex_coord).x,
            texture2D(v_tex, tex_coord).x);
#endif
}

void main()
{
    vec3 yuv = get_yuv(gl_TexCoord[0].xy);
    vec3 adjusted_yuv = adjust_yuv(yuv);
    vec3 srgb = yuv_to_srgb(adjusted_yuv);
    gl_FragColor = vec4(srgb, 1.0);
}
