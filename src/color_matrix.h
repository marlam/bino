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

#ifndef COLOR_MATRIX_H
#define COLOR_MATRIX_H

/*
 * Returns a 4x4 row-major matrix that modifies linear RGB colors.
 *
 * The brightness, contrast, hue, saturation parameters are expected to
 * be between -1 and +1, where 0 means "no change".
 */
void get_color_matrix(float brightness, float contrast, float hue, float saturation, float matrix[16]);

#endif
