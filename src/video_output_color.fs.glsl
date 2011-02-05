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

// layout_yuv_p
// layout_bgra32
#define $layout

// color_space_yuv601
// color_space_yuv709
// color_space_srgb
#define $color_space

// value_range_8bit_full
// value_range_8bit_mpeg
#define $value_range

// the offset between the y texture coordinates and the appropriate
// u and v texture coordinates, according to the chroma sample location
#define chroma_offset_x $chroma_offset_x
#define chroma_offset_y $chroma_offset_y

#if defined(layout_yuv_p)
uniform sampler2D y_tex;
uniform sampler2D u_tex;
uniform sampler2D v_tex;
#elif defined(layout_bgra32)
uniform sampler2D srgb_tex;
#endif

uniform float contrast;
uniform float brightness;
uniform float saturation;
uniform float cos_hue;
uniform float sin_hue;

/* The YUV triplets used internally in this shader use the following
 * conventions:
 * - All three components are in the range [0,1]
 * - The U and V components refer to the normalized E_Cr and E_Cb
 *   components in the ITU.BT-* documents, shifted from [-0.5,+0.5]
 *   to [0,1]
 * - The color space is either the one defined in ITU.BT-601 or the one
 *   defined in ITU.BT-709. */

#if defined(layout_bgra32)
vec3 srgb_to_yuv(vec3 srgb)
{
    // According to ITU.BT-601 (see formulas in Sec. 2.5.1 and 2.5.2)
    mat3 m = mat3(
            0.299, -0.168736,  0.5,
            0.587, -0.331264, -0.418688,
            0.114,  0.5,      -0.081312);
    return m * srgb + vec3(0.0, 0.5, 0.5);
}
#endif

vec3 yuv_to_srgb(vec3 yuv)
{
#if defined(value_range_8bit_mpeg)
    // Convert the MPEG range to the full range for each component
    yuv = (yuv - vec3(16.0 / 255.0)) * vec3(256.0 / 220.0, 256.0 / 225.0, 256.0 / 225.0);
#endif
#if defined(color_space_yuv709)
    // According to ITU.BT-709 (see entries 3.2 and 3.3 in Sec. 3 ("Signal format"))
    mat3 m = mat3(
            1.0,     1.0,      1.0,
            0.0,    -0.187324, 1.8556,
            1.5748, -0.468124, 0.0);
    return m * (yuv - vec3(0.0, 0.5, 0.5));
#else // 601 or RGB (which was converted to 601)
    // According to ITU.BT-601 (see formulas in Sec. 2.5.1 and 2.5.2)
    mat3 m = mat3(
            1.0,    1.0,      1.0,
            0.0,   -0.344136, 1.772,
            1.402, -0.714136, 0.0);
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
#if defined(layout_bgra32)
    return srgb_to_yuv(texture2D(srgb_tex, tex_coord).xyz);
#elif defined(layout_yuv_p)
    return vec3(
            texture2D(y_tex, tex_coord).x,
            texture2D(u_tex, tex_coord + vec2(chroma_offset_x, chroma_offset_y)).x,
            texture2D(v_tex, tex_coord + vec2(chroma_offset_x, chroma_offset_y)).x);
#endif
}

void main()
{
    vec3 yuv = get_yuv(gl_TexCoord[0].xy);
    vec3 adjusted_yuv = adjust_yuv(yuv);
    vec3 srgb = yuv_to_srgb(adjusted_yuv);
    gl_FragColor = vec4(srgb, 1.0);
}
