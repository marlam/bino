/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022
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

uniform sampler2D view0;
uniform sampler2D view1;

uniform float relativeWidth;
uniform float relativeHeight;
uniform float fragOffsetX;
uniform float fragOffsetY;

// This must be the same as OutputMode from modes.hpp:
const int Output_Left = 0;
const int Output_Right = 1;
const int Output_OpenGL_Stereo = 2;
const int Output_Alternating = 3;
const int Output_Red_Cyan_Dubois = 4;
const int Output_Red_Cyan_FullColor = 5;
const int Output_Red_Cyan_HalfColor = 6;
const int Output_Red_Cyan_Monochrome = 7;
const int Output_Green_Magenta_Dubois = 8;
const int Output_Green_Magenta_FullColor = 9;
const int Output_Green_Magenta_HalfColor = 10;
const int Output_Green_Magenta_Monochrome = 11;
const int Output_Amber_Blue_Dubois = 12;
const int Output_Amber_Blue_FullColor = 13;
const int Output_Amber_Blue_HalfColor = 14;
const int Output_Amber_Blue_Monochrome = 15;
const int Output_Red_Green_Monochrome = 16;
const int Output_Red_Blue_Monochrome = 17;
const int Output_Even_Odd_Rows = 18;
const int Output_Even_Odd_Columns = 19;
const int Output_Checkerboard = 20;
uniform int outputMode;

smooth in vec2 vtexcoord;

layout(location = 0) out vec4 fcolor;


// linear RGB to luminance, as used by Mitsuba2 and pbrt
float rgb_to_lum(vec3 rgb)
{
    return dot(rgb, vec3(0.212671, 0.715160, 0.072169));
}

// linear RGB to non-linear RGB
float to_nonlinear(float x)
{
    const float c0 = 0.416666666667; // 1.0 / 2.4
    return (x <= 0.0031308 ? (x * 12.92) : (1.055 * pow(x, c0) - 0.055));
}
vec3 rgb_to_nonlinear(vec3 rgb)
{
    return vec3(to_nonlinear(rgb.r), to_nonlinear(rgb.g), to_nonlinear(rgb.b));
}

void main(void)
{
    float tx = (vtexcoord.x - 0.5 * (1.0 - relativeWidth )) / relativeWidth;
    float ty = (vtexcoord.y - 0.5 * (1.0 - relativeHeight)) / relativeHeight;
    vec3 rgb = vec3(1.0, 0.0, 0.0);
    if (outputMode == Output_Left) {
        rgb = texture(view0, vec2(tx, ty)).rgb;
    } else if (outputMode == Output_Right) {
        rgb = texture(view1, vec2(tx, ty)).rgb;
    } else if (outputMode == Output_Even_Odd_Rows) {
        float fragmentY = gl_FragCoord.y - 0.5 + fragOffsetY;
        if (mod(fragmentY, 2.0) < 0.5) {
            rgb = texture(view0, vec2(tx, ty)).rgb;
        } else {
            rgb = texture(view1, vec2(tx, ty)).rgb;
        }
    } else if (outputMode == Output_Even_Odd_Columns) {
        float fragmentX = gl_FragCoord.x - 0.5 + fragOffsetX;
        if (mod(fragmentX, 2.0) < 0.5) {
            rgb = texture(view0, vec2(tx, ty)).rgb;
        } else {
            rgb = texture(view1, vec2(tx, ty)).rgb;
        }
    } else if (outputMode == Output_Checkerboard) {
        float fragmentX = gl_FragCoord.x - 0.5 + fragOffsetX;
        float fragmentY = gl_FragCoord.y - 0.5 + fragOffsetY;
        if (abs(mod(fragmentX, 2.0) - mod(fragmentY, 2.0)) < 0.5) {
            rgb = texture(view0, vec2(tx, ty)).rgb;
        } else {
            rgb = texture(view1, vec2(tx, ty)).rgb;
        }
    } else {
        vec3 rgb0 = texture(view0, vec2(tx, ty)).rgb;
        vec3 rgb1 = texture(view1, vec2(tx, ty)).rgb;
        if (outputMode == Output_Red_Cyan_Dubois) {
            // Source of this matrix: http://www.site.uottawa.ca/~edubois/anaglyph/LeastSquaresHowToPhotoshop.pdf
            mat3 m0 = mat3(
                    0.437, -0.062, -0.048,
                    0.449, -0.062, -0.050,
                    0.164, -0.024, -0.017);
            mat3 m1 = mat3(
                    -0.011,  0.377, -0.026,
                    -0.032,  0.761, -0.093,
                    -0.007,  0.009,  1.234);
            rgb = m0 * rgb0 + m1 * rgb1;
        } else if (outputMode == Output_Red_Cyan_FullColor) {
            rgb = vec3(rgb0.r, rgb1.g, rgb1.b);
        } else if (outputMode == Output_Red_Cyan_HalfColor) {
            rgb = vec3(rgb_to_lum(rgb0), rgb1.g, rgb1.b);
        } else if (outputMode == Output_Red_Cyan_Monochrome) {
            rgb = vec3(rgb_to_lum(rgb0), rgb_to_lum(rgb1), rgb_to_lum(rgb1));
        } else if (outputMode == Output_Green_Magenta_Dubois) {
            // Source of this matrix: http://www.flickr.com/photos/e_dubois/5132528166/
            mat3 m0 = mat3(
                    -0.062,  0.284, -0.015,
                    -0.158,  0.668, -0.027,
                    -0.039,  0.143,  0.021);
            mat3 m1 = mat3(
                    0.529, -0.016,  0.009,
                    0.705, -0.015,  0.075,
                    0.024, -0.065,  0.937);
            rgb = m0 * rgb0 + m1 * rgb1;
        } else if (outputMode == Output_Green_Magenta_FullColor) {
            rgb = vec3(rgb1.r, rgb0.g, rgb1.b);
        } else if (outputMode == Output_Green_Magenta_HalfColor) {
            rgb = vec3(rgb1.r, rgb_to_lum(rgb0), rgb1.b);
        } else if (outputMode == Output_Green_Magenta_Monochrome) {
            rgb = vec3(rgb_to_lum(rgb1), rgb_to_lum(rgb0), rgb_to_lum(rgb1));
        } else if (outputMode == Output_Amber_Blue_Dubois) {
            // Source of this matrix: http://www.flickr.com/photos/e_dubois/5230654930/
            mat3 m0 = mat3(
                    1.062, -0.026, -0.038,
                    -0.205,  0.908, -0.173,
                    0.299,  0.068,  0.022);
            mat3 m1 = mat3(
                    -0.016,  0.006,  0.094,
                    -0.123,  0.062,  0.185,
                    -0.017, -0.017,  0.911);
            rgb = m0 * rgb0 + m1 * rgb1;
        } else if (outputMode == Output_Amber_Blue_FullColor) {
            rgb = vec3(rgb0.r, rgb0.g, rgb1.b);
        } else if (outputMode == Output_Amber_Blue_HalfColor) {
            rgb = vec3(rgb_to_lum(rgb0), rgb_to_lum(rgb0), rgb1.b);
        } else if (outputMode == Output_Amber_Blue_Monochrome) {
            rgb = vec3(rgb_to_lum(rgb0), rgb_to_lum(rgb0), rgb_to_lum(rgb1));
        } else if (outputMode == Output_Red_Green_Monochrome) {
            rgb = vec3(rgb_to_lum(rgb0), rgb_to_lum(rgb1), 0.0);
        } else if (outputMode == Output_Red_Blue_Monochrome) {
            rgb = vec3(rgb_to_lum(rgb0), 0.0, rgb_to_lum(rgb1));
        }
    }
    fcolor = vec4(rgb_to_nonlinear(rgb), 1.0);
}
