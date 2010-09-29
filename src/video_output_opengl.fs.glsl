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

#version 110

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform float gamma;
uniform float hue;
uniform float saturation;
uniform float brightness;
uniform float contrast;
uniform int mode;
const int mode_normal = 0;
const int mode_anaglyph_monochrome = 1;
const int mode_anaglyph_full_color = 2;
const int mode_anaglyph_half_color = 3;
const int mode_anaglyph_dubois = 4;

const float pi = 3.14159265358979323846;

vec3 rgb_to_hsl(vec3 rgb)
{
    vec3 hsl;

    float minval = min(min(rgb.r, rgb.g), rgb.b);
    float maxval = max(max(rgb.r, rgb.g), rgb.b);
    float delta = maxval - minval;

    hsl.z = (maxval + minval) / 2.0;
    if (maxval == minval)
    {
	hsl.x = 0.0;
	hsl.y = 0.0;
    }
    else
    {
	hsl.y = delta / ((hsl.z <= 0.5) ? (maxval + minval) : (2.0 - maxval - minval));
	if (maxval == rgb.r)
	{
	    hsl.x = (60.0 / 360.0) * (rgb.g - rgb.b) / (maxval - minval);
	    if (rgb.g < rgb.b)
		hsl.x += 360.0 / 360.0;
	}
	else if (maxval == rgb.g)
	{
	    hsl.x = (60.0 / 360.0) * (rgb.b - rgb.r) / (maxval - minval) + (120.0 / 360.0);
	}
	else
	{
	    hsl.x = (60.0 / 360.0) * (rgb.r - rgb.g) / (maxval - minval) + (240.0 / 360.0);
	}
    }
    return hsl;
}

float hsl_to_rgb_helper(float tmp2, float tmp1, float H)
{
    float ret;
    
    if (H < 0.0)
	H += 1.0;
    else if (H > 1.0)
	H -= 1.0;

    if (H < 1.0 / 6.0)
	ret = (tmp2 + (tmp1 - tmp2) * (360.0 / 60.0) * H);
    else if (H < 1.0 / 2.0)
	ret = tmp1;
    else if (H < 2.0 / 3.0)
	ret = (tmp2 + (tmp1 - tmp2) * ((2.0 / 3.0) - H) * (360.0 / 60.0));
    else
	ret = tmp2;
    return ret;
}

vec3 hsl_to_rgb(vec3 hsl)
{
    vec3 rgb;
    float tmp1, tmp2;

    if (hsl.z < 0.5) 
	tmp1 = hsl.z * (1.0 + hsl.y);
    else
	tmp1 = (hsl.z + hsl.y) - (hsl.z * hsl.y);
    tmp2 = 2.0 * hsl.z - tmp1;
    rgb.r = hsl_to_rgb_helper(tmp2, tmp1, hsl.x + (1.0 / 3.0));
    rgb.g = hsl_to_rgb_helper(tmp2, tmp1, hsl.x);
    rgb.b = hsl_to_rgb_helper(tmp2, tmp1, hsl.x - (1.0 / 3.0));
    return rgb;
}

vec3 color_adjust(vec3 hsl)
{
    // uses uniforms hue, saturation, brightness, contrast
    hsl.x = clamp(hsl.x + hue / (2.0 * pi), 0.0, 1.0);
    hsl.y = clamp(hsl.y + saturation * hsl.y, 0.0, 1.0);
    hsl.z = clamp(hsl.z + brightness * hsl.z, 0.0, 1.0);
    hsl.z = clamp((hsl.z - 0.5) * (contrast + 1.0) + 0.5, 0.0, 1.0);
    return hsl;
}

void main()
{
    vec3 rgb0 = texture2D(tex0, gl_TexCoord[0].xy).xyz;
    vec3 hsl0 = rgb_to_hsl(clamp(pow(rgb0, vec3(1.0 / gamma)), 0.0, 1.0));
    vec3 adjusted_hsl0 = color_adjust(hsl0);
    vec3 adjusted_rgb0 = hsl_to_rgb(adjusted_hsl0);

    vec3 result = adjusted_rgb0;
    if (mode != mode_normal)
    {
        vec3 rgb1 = texture2D(tex1, gl_TexCoord[0].xy).xyz;
        vec3 hsl1 = rgb_to_hsl(clamp(pow(rgb1, vec3(1.0 / gamma)), 0.0, 1.0));
        vec3 adjusted_hsl1 = color_adjust(hsl1);
        vec3 adjusted_rgb1 = hsl_to_rgb(adjusted_hsl1);
        if (mode == mode_anaglyph_monochrome)
        {
            // left: luminance in red channel, right: luminance in green + blue channels
            float lum_l = 0.299 * adjusted_rgb0.r + 0.587 * adjusted_rgb0.g + 0.114 * adjusted_rgb0.b;
            float lum_r = 0.299 * adjusted_rgb1.r + 0.587 * adjusted_rgb1.g + 0.114 * adjusted_rgb1.b;
            result = vec3(lum_l, lum_r, lum_r);
        }
        else if (mode == mode_anaglyph_full_color)
        {
            // left: red, right: green + blue
            result = vec3(adjusted_rgb0.r, adjusted_rgb1.gb);
        }
        else if (mode == mode_anaglyph_half_color)
        {
            // left: luminance in red channel, right: green + blue
            float lum_l = 0.299 * adjusted_rgb0.r + 0.587 * adjusted_rgb0.g + 0.114 * adjusted_rgb0.b;
            result = vec3(lum_l, adjusted_rgb1.gb);
        }
        else if (mode == mode_anaglyph_dubois)
        {
            // Dubois anaglyph method.
            //
            // Authors page: http://www.site.uottawa.ca/~edubois/anaglyph/
            //
            // This method depends on the characteristics of the display device
            // and the anaglyph glasses.
            //
            // The matrices below are taken from "A Uniform Metric for Anaglyph Calculation"
            // by Zhe Zhang and David F. McAllister, Proc. Electronic Imaging 2006, available
            // here: http://research.csc.ncsu.edu/stereographics/ei06.pdf
            //
            // These matrices are supposed to work fine for LCD monitors and most red-cyan
            // glasses. See also the remarks in http://research.csc.ncsu.edu/stereographics/LS.pdf
            // (where slightly different values are used).

            /*
            mat3 m0 = transpose(mat3(
                     0.4155,  0.4710,  0.1670,
                    -0.0458, -0.0484, -0.0258,
                    -0.0545, -0.0614,  0.0128));
            mat3 m1 = transpose(mat3(
                    -0.0109, -0.0365, -0.0060,
                     0.3756,  0.7333,  0.0111,
                    -0.0651, -0.1286,  1.2968));
            */
            mat3 m0 = mat3(
                     0.4155, -0.0458, -0.0545,
                     0.4710, -0.0484, -0.0614,
                     0.1670, -0.0258,  0.0128);
            mat3 m1 = mat3(
                    -0.0109,  0.3756, -0.0651,
                    -0.0365,  0.7333, -0.1286,
                    -0.0060,  0.0111,  1.2968);

            result = clamp(m0 * adjusted_rgb0 + m1 * adjusted_rgb1, 0.0, 1.0);
        }
    }

    gl_FragColor = vec4(result, 1.0);
}
