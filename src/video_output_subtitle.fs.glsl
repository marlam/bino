/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2011
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

uniform sampler2D subtitle_tex;
uniform float parallax;

float linear_to_nonlinear(float x)
{
    return (x <= 0.0031308 ? (x * 12.92) : (1.055 * pow(x, 1.0 / 2.4) - 0.055));
}

vec4 rgba_to_srgba(vec4 rgba)
{
#if $srgb_broken
    return rgba;
#else
    // See GL_ARB_framebuffer_sRGB extension
    return vec4(
            linear_to_nonlinear(rgba.r),
            linear_to_nonlinear(rgba.g),
            linear_to_nonlinear(rgba.b),
            rgba.a);
#endif
}

void main()
{
    vec4 rgba = texture2D(subtitle_tex, gl_TexCoord[0].xy + vec2(parallax, 0.0));
    gl_FragColor = rgba_to_srgba(rgba);
}
