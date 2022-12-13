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

uniform sampler2D frameTex;
uniform sampler2D subtitleTex;
uniform float relative_width;
uniform float relative_height;
uniform float view_offset_x;
uniform float view_factor_x;
uniform float view_offset_y;
uniform float view_factor_y;
int surroundDegrees = $SURROUND_DEGREES;
const bool nonlinear_output = $NONLINEAR_OUTPUT;

smooth in vec2 vtexcoord;
smooth in vec3 vdirection;

const float pi = 3.14159265358979323846;

layout(location = 0) out vec4 fcolor;

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
    vec3 rgb;
    if (surroundDegrees > 0) {
        vec3 dir = normalize(vdirection);
        float theta = asin(clamp(-dir.y, -1.0, 1.0));
        float phi = atan(dir.x, -dir.z);
	float tmp = (surroundDegrees == 360 ? 2.0f * pi : pi);
        float u = phi / tmp + 0.5;
        float v = theta / pi + 0.5;
        float vtx = view_offset_x + view_factor_x * u;
        float vty = view_offset_y + view_factor_y * v;
        rgb = texture(frameTex, vec2(vtx, vty)).rgb;
    } else {
        float vtx = view_offset_x + view_factor_x * vtexcoord.x;
        float vty = view_offset_y + view_factor_y * vtexcoord.y;
        float tx = (      vtx - 0.5 * (1.0 - relative_width )) / relative_width;
        float ty = (1.0 - vty - 0.5 * (1.0 - relative_height)) / relative_height;
        rgb = texture(frameTex, vec2(tx, ty)).rgb;
        vec4 sub = texture(subtitleTex, vec2(vtexcoord.x, 1.0 - vtexcoord.y)).rgba;
        rgb = mix(rgb, sub.rgb, sub.a);
    }
    if (nonlinear_output) {
        rgb = rgb_to_nonlinear(rgb);
    }
    fcolor = vec4(rgb, 1.0);
}
