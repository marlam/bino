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

// mode_onechannel
// mode_anaglyph_monochrome
// mode_anaglyph_full_color
// mode_anaglyph_half_color
// mode_anaglyph_dubois
// mode_even_odd_rows
// mode_even_odd_columns
// mode_checkerboard
#define $mode

uniform sampler2D rgb_l;
uniform sampler2D rgb_r;

#if defined(mode_even_odd_rows) || defined(mode_even_odd_columns) || defined(mode_checkerboard)
uniform sampler2D mask_tex;
uniform float step_x;
uniform float step_y;
#endif

#if defined(mode_anaglyph_monochrome) || defined(mode_anaglyph_half_color)
float srgb_to_lum(vec3 srgb)
{
    // Values taken from http://www.fourcc.org/fccyvrgb.php
    return dot(srgb, vec3(0.299, 0.587, 0.114));
}
#endif

vec3 rgb_to_srgb(vec3 rgb)
{
#if $srgb_broken
    return rgb;
#else
# if 1
    // Correct variant, see GL_ARB_framebuffer_sRGB extension
    return vec3(
            (rgb.r <= 0.0031308 ? (rgb.r * 12.92) : (1.055 * pow(rgb.r, 1.0 / 2.4) - 0.055)),
            (rgb.g <= 0.0031308 ? (rgb.g * 12.92) : (1.055 * pow(rgb.g, 1.0 / 2.4) - 0.055)),
            (rgb.b <= 0.0031308 ? (rgb.b * 12.92) : (1.055 * pow(rgb.b, 1.0 / 2.4) - 0.055)));
# endif
# if 0
    // Faster variant
    return pow(rgb, 1.0 / 2.2);
# endif
# if 0
    // Even faster variant, assuming gamma = 2.0
    return sqrt(rgb);
# endif
#endif
}

void main()
{
    vec3 srgb;

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
     * Why use local variables in the if/else branches, and a local computation of the srgb value?
     *  - To work around driver bugs.
     */
    float m = texture2D(mask_tex, gl_TexCoord[1].xy).x;
# if defined(mode_even_odd_rows)
    if (m > 0.5)
    {
        vec3 rgb0 = texture2D(rgb_l, gl_TexCoord[0].xy - vec2(0.0, step_y)).rgb;
        vec3 rgb1 = texture2D(rgb_l, gl_TexCoord[0].xy).rgb;
        vec3 rgb2 = texture2D(rgb_l, gl_TexCoord[0].xy + vec2(0.0, step_y)).rgb;
        srgb = rgb_to_srgb((rgb0 + 2.0 * rgb1 + rgb2) / 4.0);
    }
    else
    {
        vec3 rgb0 = texture2D(rgb_r, gl_TexCoord[0].xy - vec2(0.0, step_y)).rgb;
        vec3 rgb1 = texture2D(rgb_r, gl_TexCoord[0].xy).rgb;
        vec3 rgb2 = texture2D(rgb_r, gl_TexCoord[0].xy + vec2(0.0, step_y)).rgb;
        srgb = rgb_to_srgb((rgb0 + 2.0 * rgb1 + rgb2) / 4.0);
    }
# elif defined(mode_even_odd_columns)
    if (m > 0.5)
    {
        vec3 rgb0 = texture2D(rgb_l, gl_TexCoord[0].xy - vec2(step_x, 0.0)).rgb;
        vec3 rgb1 = texture2D(rgb_l, gl_TexCoord[0].xy).rgb;
        vec3 rgb2 = texture2D(rgb_l, gl_TexCoord[0].xy + vec2(step_x, 0.0)).rgb;
        srgb = rgb_to_srgb((rgb0 + 2.0 * rgb1 + rgb2) / 4.0);
    }
    else
    {
        vec3 rgb0 = texture2D(rgb_r, gl_TexCoord[0].xy - vec2(step_x, 0.0)).rgb;
        vec3 rgb1 = texture2D(rgb_r, gl_TexCoord[0].xy).rgb;
        vec3 rgb2 = texture2D(rgb_r, gl_TexCoord[0].xy + vec2(step_x, 0.0)).rgb;
        srgb = rgb_to_srgb((rgb0 + 2.0 * rgb1 + rgb2) / 4.0);
    }
# elif defined(mode_checkerboard)
    if (m > 0.5)
    {
        vec3 rgb0 = texture2D(rgb_l, gl_TexCoord[0].xy - vec2(0.0, step_y)).rgb;
        vec3 rgb1 = texture2D(rgb_l, gl_TexCoord[0].xy - vec2(step_x, 0.0)).rgb;
        vec3 rgb2 = texture2D(rgb_l, gl_TexCoord[0].xy).rgb;
        vec3 rgb3 = texture2D(rgb_l, gl_TexCoord[0].xy + vec2(step_x, 0.0)).rgb;
        vec3 rgb4 = texture2D(rgb_l, gl_TexCoord[0].xy + vec2(0.0, step_y)).rgb;
        srgb = rgb_to_srgb((rgb0 + rgb1 + 4.0 * rgb2 + rgb3 + rgb4) / 8.0);
    }
    else
    {
        vec3 rgb0 = texture2D(rgb_r, gl_TexCoord[0].xy - vec2(0.0, step_y)).rgb;
        vec3 rgb1 = texture2D(rgb_r, gl_TexCoord[0].xy - vec2(step_x, 0.0)).rgb;
        vec3 rgb2 = texture2D(rgb_r, gl_TexCoord[0].xy).rgb;
        vec3 rgb3 = texture2D(rgb_r, gl_TexCoord[0].xy + vec2(step_x, 0.0)).rgb;
        vec3 rgb4 = texture2D(rgb_r, gl_TexCoord[0].xy + vec2(0.0, step_y)).rgb;
        srgb = rgb_to_srgb((rgb0 + rgb1 + 4.0 * rgb2 + rgb3 + rgb4) / 8.0);
    }
# endif

#else

    vec3 l = rgb_to_srgb(texture2D(rgb_l, gl_TexCoord[0].xy).rgb);
# if !defined(mode_onechannel)
    vec3 r = rgb_to_srgb(texture2D(rgb_r, gl_TexCoord[0].xy).rgb);
# endif

# if defined(mode_onechannel)
    srgb = l;
# elif defined(mode_anaglyph_monochrome)
    srgb = vec3(srgb_to_lum(l), srgb_to_lum(r), srgb_to_lum(r));
# elif defined(mode_anaglyph_full_color)
    srgb = vec3(l.r, r.gb);
# elif defined(mode_anaglyph_half_color)
    srgb = vec3(srgb_to_lum(l), r.gb);
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
    srgb = m0 * l + m1 * r;
# endif

#endif

    gl_FragColor = vec4(srgb, 1.0);
}
