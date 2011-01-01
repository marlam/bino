/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
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
    // According to ITU.BT-601
    mat3 m = mat3(
            0.299, -0.169,  0.5,
            0.587, -0.331, -0.419,
            0.114,  0.500, -0.081);
    return m * srgb + vec3(0.0, 0.5, 0.5);
# if 0
    // According to ITU.BT-709
    mat3 m = mat3(
            0.2215, -0.1145,  0.5016,
            0.7154, -0.3855, -0.4556,
            0.0721,  0.5,    -0.0459);
    return m * srgb + vec3(0.0, 0.5, 0.5);
#endif
}
#endif

vec3 yuv_to_srgb(vec3 yuv)
{
    // According to ITU.BT-601
    mat3 m = mat3(
            1.0,     1.0,  1.0,
            0.0,   -0.344, 1.773,
            1.403, -0.714, 0.0);
    return m * (yuv - vec3(0.0, 0.5, 0.5));
# if 0
    // According to ITU.BT-709
    mat3 m = mat3(
            1.0,     1.0,     1.0,
            0.0,    -0.187,  -1.8556,
            1.5701, -0.4664,  0.0);
    return m * (yuv - vec3(0.0, 0.5, 0.5));
#endif
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
