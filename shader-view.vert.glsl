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

uniform mat4 projection_matrix;
uniform mat4 model_view_matrix;
uniform bool three_sixty;

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texcoord;

smooth out vec2 vtexcoord;
smooth out vec3 vdirection;

void main(void)
{
    vtexcoord = texcoord;
    vdirection = (position * model_view_matrix).xyz;
    vec4 out_pos;
    if (three_sixty)
        out_pos = projection_matrix * position;
    else
        out_pos = projection_matrix * model_view_matrix * position;
    gl_Position = out_pos;
}
