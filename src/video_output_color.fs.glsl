/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012
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

// quality: 0 .. 4
#define quality $quality

// layout_yuv_p
// layout_bgra32
#define $layout

// color_space_yuv601
// color_space_yuv709
// color_space_srgb
#define $color_space

// value_range_8bit_full
// value_range_8bit_mpeg
// value_range_10bit_full
// value_range_10bit_mpeg
#define $value_range

// the offset between the y texture coordinates and the appropriate
// u and v texture coordinates, according to the chroma sample location
#define chroma_offset_x $chroma_offset_x
#define chroma_offset_y $chroma_offset_y

// storage_srgb
// storage_linear_rgb
#define $storage

#if defined(layout_yuv_p)
uniform sampler2D y_tex;
uniform sampler2D u_tex;
uniform sampler2D v_tex;
#elif defined(layout_bgra32)
uniform sampler2D srgb_tex;
#endif

/* The YUV triplets used internally in this shader use the following
 * conventions:
 * - All three components are in the range [0,1]
 * - The U and V components refer to the normalized E_Cr and E_Cb
 *   components in the ITU.BT-* documents, shifted from [-0.5,+0.5]
 *   to [0,1]
 * - The color space is either the one defined in ITU.BT-601 or the one
 *   defined in ITU.BT-709. */
#if !defined(layout_bgra32)
vec3 yuv_to_srgb(vec3 yuv)
{
    // Convert the MPEG range to the full range for each component, if necessary
# if defined(value_range_8bit_mpeg)
    yuv = (yuv - vec3(16.0 / 255.0)) * vec3(256.0 / 220.0, 256.0 / 225.0, 256.0 / 225.0);
# elif defined(value_range_10bit_mpeg)
    yuv = (yuv - vec3(64.0 / 1023.0)) * vec3(1024.0 / 877.0, 1024.0 / 897.0, 1024.0 / 897.0);
# endif

# if defined(color_space_yuv709)
    // According to ITU.BT-709 (see entries 3.2 and 3.3 in Sec. 3 ("Signal format"))
    mat3 m = mat3(
            1.0,     1.0,      1.0,
            0.0,    -0.187324, 1.8556,
            1.5748, -0.468124, 0.0);
    return m * (yuv - vec3(0.0, 0.5, 0.5));
# else
    // According to ITU.BT-601 (see formulas in Sec. 2.5.1 and 2.5.2)
    mat3 m = mat3(
            1.0,    1.0,      1.0,
            0.0,   -0.344136, 1.772,
            1.402, -0.714136, 0.0);
    return m * (yuv - vec3(0.0, 0.5, 0.5));
# endif
}
#endif

#if defined(storage_linear_rgb)
# if quality >= 4
// See GL_ARB_framebuffer_sRGB extension
float nonlinear_to_linear(float x)
{
    return (x <= 0.04045 ? (x / 12.92) : pow((x + 0.055) / 1.055, 2.4));
}
vec3 srgb_to_rgb(vec3 srgb)
{
    float r = nonlinear_to_linear(srgb.r);
    float g = nonlinear_to_linear(srgb.g);
    float b = nonlinear_to_linear(srgb.b);
    return vec3(r, g, b);
}
# elif quality >= 2
vec3 srgb_to_rgb(vec3 srgb)
{
    return pow(srgb, vec3(2.2));
}
# else
vec3 srgb_to_rgb(vec3 srgb)
{
    return srgb * srgb;
}
# endif
#endif

vec3 get_srgb(vec2 tex_coord)
{
#if defined(layout_bgra32)
    return texture2D(srgb_tex, tex_coord).xyz;
#elif defined(value_range_8bit_full) || defined(value_range_8bit_mpeg)
    return yuv_to_srgb(vec3(
                texture2D(y_tex, tex_coord).x,
                texture2D(u_tex, tex_coord + vec2(chroma_offset_x, chroma_offset_y)).x,
                texture2D(v_tex, tex_coord + vec2(chroma_offset_x, chroma_offset_y)).x));
#else
    return yuv_to_srgb((65535.0 / 1023.0) * vec3(
                texture2D(y_tex, tex_coord).x,
                texture2D(u_tex, tex_coord + vec2(chroma_offset_x, chroma_offset_y)).x,
                texture2D(v_tex, tex_coord + vec2(chroma_offset_x, chroma_offset_y)).x));
#endif
}

void main()
{
    vec3 srgb = get_srgb(gl_TexCoord[0].xy);
#if defined(storage_srgb)
    gl_FragColor = vec4(srgb, 1.0);
#else
    vec3 rgb = srgb_to_rgb(srgb);
    gl_FragColor = vec4(rgb, 1.0);
#endif
}
