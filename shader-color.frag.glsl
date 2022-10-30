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

uniform sampler2D plane0;
uniform sampler2D plane1;
uniform sampler2D plane2;
uniform sampler2D plane3;

const int Format_RGB = 1;
const int Format_YUVp = 2;
const int Format_YVUp = 3;
const int Format_YUVsp = 4;
uniform int planeFormat;

uniform bool yuvValueRangeSmall;

const int YUV_BT601 = 1;
const int YUV_BT709 = 2;
const int YUV_AdobeRGB = 3;
const int YUV_BT2020 = 4;
uniform int yuvSpace;

smooth in vec2 vtexcoord;

layout(location = 0) out vec4 fcolor;

float to_linear(float x)
{
    const float c0 = 0.077399380805; // 1.0 / 12.92
    const float c1 = 0.947867298578; // 1.0 / 1.055;
    return (x <= 0.04045 ? (x * c0) : pow((x + 0.055) * c1, 2.4));
}

vec3 rgb_to_linear(vec3 rgb)
{
    return vec3(to_linear(rgb.r), to_linear(rgb.g), to_linear(rgb.b));
}

void main(void)
{
    vec3 rgb = vec3(0.0, 1.0, 0.0);
    if (planeFormat == Format_RGB) {
        rgb = texture(plane0, vtexcoord).rgb;
    } else {
        vec3 yuv;
        if (planeFormat == Format_YUVp) {
            yuv = vec3(
                    texture(plane0, vtexcoord).r,
                    texture(plane1, vtexcoord).r,
                    texture(plane2, vtexcoord).r);
        } else if (planeFormat == Format_YVUp) {
            yuv = vec3(
                    texture(plane0, vtexcoord).r,
                    texture(plane2, vtexcoord).r,
                    texture(plane1, vtexcoord).r);
        } else if (planeFormat == Format_YUVsp) {
            yuv = vec3(
                    texture(plane0, vtexcoord).r,
                    texture(plane1, vtexcoord).rg);
        }
        mat4 m;
        // The following matrices are the same as used by Qt,
        // see qtmultimedia/src/multimedia/video/qvideotexturehelper.cpp
        if (yuvSpace == YUV_AdobeRGB) {
            m = mat4(
                    1.0, 1.0, 1.0, 0.0,
                    0.0, -0.344, 1.772, 0.0,
                    1.402, -0.714, 0.0, 0.0,
                    -0.701, 0.529, -0.886, 1.0);
        } else if (yuvSpace == YUV_BT709) {
            if (yuvValueRangeSmall) {
                m = mat4(
                        1.1644, 1.1644, 1.1644, 0.0,
                        0.0, -0.5329, 2.1124, 0.0,
                        1.7928, -0.2132, 0.0, 0.0,
                        -0.9731, 0.3015, -1.1335, 1.0);
            } else {
                m = mat4(
                        1.0, 1.0, 1.0, 0.0,
                        0.0, -0.187324, 1.8556, 0.0,
                        1.5748, -0.468124, 0.0, 0.0,
                        -0.8774, 0.327724, -0.9278, 1.0);
            }
        } else if (yuvSpace == YUV_BT2020) {
            if (yuvValueRangeSmall) {
                m = mat4(
                        1.1644, 1.1644, 1.1644, 0.0,
                        0.0, -0.1874, 2.1418, 0.0,
                        1.6787, -0.6511, 0.0, 0.0,
                        -0.9158, 0.3478, -1.1483, 1.0);
            } else {
                m = mat4(
                        1.0, 1.0, 1.0, 0.0,
                        0.0, -0.2801, 1.8814, 0.0,
                        1.4746, -0.91666, 0.0, 0.0,
                        -0.7373, 0.5984, -0.9407, 1.0);
            }
        } else {
            if (yuvValueRangeSmall) {
                m = mat4(
                        1.164, 1.164, 1.164, 0.0,
                        0.0, -0.392, 2.017, 0.0,
                        1.596, -0.813, 0.0, 0.0,
                        -0.8708, 0.5296, -1.081, 1.0);
            } else {
                m = mat4(
                        1.0, 1.0, 1.0, 0.0,
                        0.0, -0.1646, 1.42, 0.0,
                        1.772, -0.57135, 0.0, 0.0,
                        -0.886, 0.36795, -0.71, 1.0);
            }
        }
        rgb = (m * vec4(yuv, 1.0)).rgb;
    }

    rgb = rgb_to_linear(rgb);
    fcolor = vec4(rgb, 1.0);
}
