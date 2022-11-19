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

uniform bool hasDiffTex;
uniform sampler2D diffTex;

smooth in vec3 vnormal;
smooth in vec3 vlight;
smooth in vec3 vview;
smooth in vec2 vtexcoord;

layout(location = 0) out vec4 fcolor;

void main(void)
{
    const vec3 lightAmbient = vec3(0.0);
    const vec3 lightColor = vec3(1.0);
    vec3 materialColor = vec3(1.0);
    if (hasDiffTex)
        materialColor = texture(diffTex, vtexcoord).rgb;

    vec3 normal = normalize(vnormal);
    vec3 light = normalize(vlight);
    vec3 view = normalize(vview);
    vec3 halfv = normalize(light + view);
    vec3 diffuse = materialColor * lightColor * max(dot(light, normal), 0.0);
    vec3 color = lightAmbient + diffuse;

    fcolor = vec4(color, 1.0);
}
