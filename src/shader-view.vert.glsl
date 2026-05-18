/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022, 2023, 2024, 2025, 2026
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

uniform mat4 projectionModelViewMatrix;
uniform mat4 orientationMatrix;
uniform int rotation; // 0=none, 1=90°, 2=180°, 3=270° (all clockwise)

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texcoord;

smooth out vec2 vtexcoord;
smooth out vec3 vdirection;

void main(void)
{
    vec2 tc = texcoord;
    if (rotation == 1) {
        tc = vec2(1.0 - texcoord.y, texcoord.x);
    } else if (rotation == 2) {
        tc = vec2(1.0 - texcoord.x, 1.0 - texcoord.y);
    } else if (rotation == 3) {
        tc = vec2(texcoord.y, 1.0 - texcoord.x);
    }
    vtexcoord = tc;
    vdirection = (position * orientationMatrix).xyz;
    gl_Position = projectionModelViewMatrix * position;
}
