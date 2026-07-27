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

uniform sampler2D frameTex;
uniform sampler2D overlayTex0; // audio
uniform sampler2D overlayTex1; // subtitle
uniform sampler2D overlayTex2; // ui
uniform bool showOverlayAudio;
uniform bool showOverlaySubtitle;
uniform bool showOverlayUI;
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
    vec3 rgb = vec3(0.0, 0.0, 0.0);
    float overlay_y = 1.0 - vtexcoord.y;
    float overlay_x = vtexcoord.x;
    if (surroundDegrees > 0) {
        overlay_x = 1.0 - overlay_x;
        vec3 dir = normalize(vdirection);
        float theta = asin(clamp(-dir.y, -1.0, 1.0));
        float phi = atan(dir.x, -dir.z);
	float tmp = (surroundDegrees == 360 ? 2.0 * pi : pi);
        float u = phi / tmp + 0.5;
        float v = theta / pi + 0.5;
        float vtx = view_offset_x + view_factor_x * u;
        float vty = view_offset_y + view_factor_y * v;
        rgb = texture(frameTex, vec2(vtx, vty)).rgb;
    } else {
        float vtx = (      vtexcoord.x - 0.5) / relative_width  + 0.5;
        float vty = (1.0 - vtexcoord.y - 0.5) / relative_height + 0.5;
        float x_inside = step(0.0, vtx) * step(0.0, 1.0 - vtx);
        float y_inside = step(0.0, vty) * step(0.0, 1.0 - vty);
        float tx = view_offset_x + view_factor_x * vtx;
        float ty = view_offset_y + view_factor_y * vty;
        rgb = x_inside * y_inside * texture(frameTex, vec2(tx, ty)).rgb;
    }
    // Only show overlays for the cube side that is directly in front
    // of the viewer. In our cube VAO, this cube side is rendered via
    // triangles 2 and 3.
    if (surroundDegrees == 0 || gl_PrimitiveID == 2 || gl_PrimitiveID == 3) {
        if (showOverlayAudio) {
            vec4 ovl0 = texture(overlayTex0, vec2(overlay_x, overlay_y)).rgba;
            rgb = mix(rgb, ovl0.rgb, ovl0.a);
        }
        if (showOverlaySubtitle) {
            vec4 ovl1 = texture(overlayTex1, vec2(overlay_x, overlay_y)).rgba;
            rgb = mix(rgb, ovl1.rgb, ovl1.a);
        }
        if (showOverlayUI) {
            vec4 ovl2 = texture(overlayTex2, vec2(overlay_x, overlay_y)).rgba;
            rgb = mix(rgb, ovl2.rgb, ovl2.a);
        }
    }
    if (nonlinear_output) {
        rgb = rgb_to_nonlinear(rgb);
    }
    fcolor = vec4(rgb, 1.0);
}
