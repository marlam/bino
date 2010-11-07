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

#include "config.h"

#include "video_output.h"


video_output_state::video_output_state() throw ()
    : contrast(0.0f), brightness(0.0f), hue(0.0f), saturation(0.0f),
    fullscreen(false), swap_eyes(false)
{
}

video_output_state::~video_output_state()
{
}


video_output::video_output(bool receiving_controller) throw ()
    : controller(receiving_controller)
{
}

video_output::~video_output()
{
}
