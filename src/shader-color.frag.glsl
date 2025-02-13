/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022, 2023, 2024, 2025
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
const int Format_Y = 5;
const int planeFormat = $PLANE_FORMAT;

const bool colorRangeSmall = $COLOR_RANGE_SMALL;

const int CS_BT601 = 1;
const int CS_BT709 = 2;
const int CS_AdobeRGB = 3;
const int CS_BT2020 = 4;
const int colorSpace = $COLOR_SPACE;

const int CT_NOOP = 1;
const int CT_ST2084 = 2;
const int CT_STD_B67 = 3;
const int colorTransfer = $COLOR_TRANSFER;
uniform float masteringWhite;

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
    vec3 yuv = vec3(1.0, 0.0, 0.0);
    vec3 rgb = vec3(0.0, 1.0, 0.0);
    if (planeFormat == Format_RGB) {
        rgb = texture(plane0, vtexcoord).rgb;
    } else if (planeFormat == Format_Y) {
        rgb = texture(plane0, vtexcoord).rrr;
    } else {
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
        if (colorSpace == CS_AdobeRGB) {
            m = mat4(
                    1.0, 1.0, 1.0, 0.0,
                    0.0, -0.344, 1.772, 0.0,
                    1.402, -0.714, 0.0, 0.0,
                    -0.701, 0.529, -0.886, 1.0);
        } else if (colorSpace == CS_BT709) {
            if (colorRangeSmall) {
                m = mat4(
                        1.1644, 1.1644, 1.1644, 0.0,
                        0.0, -0.2132, 2.1124, 0.0,
                        1.7927, -0.5329, 0.0, 0.0,
                        -0.9729, 0.3015, -1.1334, 1.0);
            } else {
                m = mat4(
                        1.0, 1.0, 1.0, 0.0,
                        0.0, -0.187324, 1.8556, 0.0,
                        1.5748, -0.468124, 0.0, 0.0,
                        -0.790488, 0.329010, -0.931439, 1.0);
            }
        } else if (colorSpace == CS_BT2020) {
            if (colorRangeSmall) {
                m = mat4(
                        1.1644, 1.1644, 1.1644, 0.0,
                        0.0, -0.1874, 2.1418, 0.0,
                        1.6787, -0.6504, 0.0, 0.0,
                        -0.9157, 0.3475, -1.1483, 1.0);
            } else {
                m = mat4(
                        1.0, 1.0, 1.0, 0.0,
                        0.0, -0.1646, 1.8814, 0.0,
                        1.4746, -0.5714, 0.0, 0.0,
                        -0.7402, 0.3694, -0.9445, 1.0);
            }
        } else {
            if (colorRangeSmall) {
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
    if (colorTransfer == CT_ST2084 || colorTransfer == CT_STD_B67) {
        // This code was reconstructed from the mess in qtmultimedia/src/multimedia/video;
        // it is distributed there over various shaders and C++ files.
        // 1. scale
        const float maxLum = 1.0;
        float scale = 1.0;
        float y = (yuv.x - 16.0 / 256.0) * 256.0 / 219.0; // XXX This looks wrong!?
        float p = y / masteringWhite;
        float ks = 1.5 * maxLum - 0.5;
        if (p > ks) {
            float t = (p - ks) / (1.0 - ks);
            float t2 = t * t;
            float t3 = t * t2;
            p = (2.0 * t3 - 3.0 * t2 + 1.0) * ks + (t3 - 2.0 * t2 + t) * (1.0 - ks) + (-2.0 * t3 + 3.0 * t2) * maxLum;
            float newY = p * masteringWhite;
            scale = newY / y;
        }
        rgb *= scale;
        // 2. tonemap
        if (colorTransfer == CT_ST2084) {
            const vec3 one_over_m1 = vec3(8192.0 / 1305.0);
            const vec3 one_over_m2 = vec3(32.0 / 2523.0);
            const float c1 = 107.0 / 128.0;
            const float c2 = 2413.0 / 128.0;
            const float c3 = 2392.0 / 128.0;
            vec3 e = pow(rgb, one_over_m2);
            vec3 num = max(e - c1, 0.0);
            vec3 den = c2 - c3 * e;
            rgb = pow(num / den, one_over_m1) * 10000.0 / 100.0;
        } else if (colorTransfer == CT_STD_B67) {
            const float a = 0.17883277;
            const float b = 0.28466892; // = 1 - 4a
            const float c = 0.55991073; // = 0.5 - a ln(4a)
            bvec3 cutoff = lessThan(rgb, vec3(0.5));
            vec3 low = rgb * rgb / 3.0;
            vec3 high = (exp((rgb - c) / a) + b) / 12.0;
            rgb = mix(high, low, cutoff);
            float lum = dot(rgb, vec3(0.2627, 0.6780, 0.0593));
            float y = pow(lum, 0.2); // gamma-1 with gamma = 1.2
            rgb *= y;
        }
        // 3. convert rec2020 to sRGB
        rgb = rgb * mat3(
                1.6605, -0.5876, -0.0728,
                -0.1246,  1.1329, -0.0083,
                -0.0182, -0.1006,  1.1187);
    } else {
        rgb = rgb_to_linear(rgb);
    }
    fcolor = vec4(rgb, 1.0);
}

