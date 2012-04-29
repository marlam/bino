/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2012
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

#include <cmath>

#include "color_matrix.h"

static void matmult(const float A[16], const float B[16], float result[16])
{
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            result[r * 4 + c] =
                  A[r * 4 + 0] * B[0 * 4 + c]
                + A[r * 4 + 1] * B[1 * 4 + c]
                + A[r * 4 + 2] * B[2 * 4 + c]
                + A[r * 4 + 3] * B[3 * 4 + c];
        }
    }
}

void get_color_matrix(float brightness, float contrast, float hue, float saturation, float matrix[16])
{
    /*
     * See http://www.graficaobscura.com/matrix/index.html for the basic ideas.
     * Note that the hue matrix is computed in a simpler way.
     */

    const float wr = 0.3086f;
    const float wg = 0.6094f;
    const float wb = 0.0820f;

    // Brightness and Contrast
    float b = (brightness < 0.0f ? brightness : 4.0f * brightness) + 1.0f;
    float c = -contrast;
    float BC[16] = {
           b, 0.0f, 0.0f, 0.0f,
        0.0f,    b, 0.0f, 0.0f,
        0.0f, 0.0f,    b, 0.0f,
           c,    c,    c, 1.0f
    };
    // Saturation
    float s = saturation + 1.0f;
    float S[16] = {
        (1.0f - s) * wr + s, (1.0f - s) * wg    , (1.0f - s) * wb    , 0.0f,
        (1.0f - s) * wr    , (1.0f - s) * wg + s, (1.0f - s) * wb    , 0.0f,
        (1.0f - s) * wr    , (1.0f - s) * wg    , (1.0f - s) * wb + s, 0.0f,
                       0.0f,                0.0f,                0.0f, 1.0f
    };
    // Hue
    float n = 1.0f / sqrtf(3.0f);       // normalized hue rotation axis: sqrt(3) * (1 1 1)
    float h = hue * M_PI;               // hue rotation angle
    float hc = cosf(h);
    float hs = sinf(h);
    float H[16] = {     // conversion of angle/axis representation to matrix representation
        n * n * (1.0f - hc) + hc    , n * n * (1.0f - hc) - n * hs, n * n * (1.0f - hc) + n * hs, 0.0f,
        n * n * (1.0f - hc) + n * hs, n * n * (1.0f - hc) + hc    , n * n * (1.0f - hc) - n * hs, 0.0f,
        n * n * (1.0f - hc) - n * hs, n * n * (1.0f - hc) + n * hs, n * n * (1.0f - hc) + hc    , 0.0f,
                                0.0f,                         0.0f,                         0.0f, 1.0f
    };

    /* I
    float I[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    */

    // matrix = B * C * S * H
    float T[16];
    matmult(BC, S, T);
    matmult(T, H, matrix);
}
