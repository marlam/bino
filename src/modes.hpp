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

#pragma once

#include <QString>


/* Input mode: Is this 2D or 3D, and in the latter case, how are the
 * left and right view arranged in one frame? */
enum InputMode {
    Input_Unknown,         // unknown; needs to be guessed
    Input_Mono,            // monoscopic video (2D)
    Input_Top_Bottom,      // stereoscopic video, left eye top, right eye bottom
    Input_Top_Bottom_Half, // stereoscopic video, left eye top, right eye bottom, both half height
    Input_Bottom_Top,      // stereoscopic video, left eye bottom, right eye top
    Input_Bottom_Top_Half, // stereoscopic video, left eye bottom, right eye top, both half height
    Input_Left_Right,      // stereoscopic video, left eye left, right eye right
    Input_Left_Right_Half, // stereoscopic video, left eye left, right eye right, both half width
    Input_Right_Left,      // stereoscopic video, left eye right, right eye left
    Input_Right_Left_Half, // stereoscopic video, left eye right, right eye left, both half width
    Input_Alternating_LR,  // stereoscopic video, alternating frames, left first
    Input_Alternating_RL,  // stereoscopic video, alternating frames, right first
};

const char* inputModeToString(InputMode mode);
InputMode inputModeFromString(const QString& s, bool* ok = nullptr);

/* Surround mode (180° / 360°) */
enum SurroundMode {
    Surround_Unknown,     // unknown; needs to be guessed
    Surround_Off,         // conventional video
    Surround_180,         // 180° video
    Surround_360,         // 360° video
};

const char* surroundModeToString(SurroundMode mode);
SurroundMode surroundModeFromString(const QString& s, bool* ok = nullptr);

/* Output mode: Is the output 2D or 3D, and in the latter case, how should
 * the left and right view be arranged on screen? */
// Note that this is mirrored in shader-display-frag.glsl!
enum OutputMode {
    Output_Left = 0,
    Output_Right = 1,
    Output_OpenGL_Stereo = 2,
    Output_Alternating = 3,
    Output_Left_Right = 4,
    Output_Left_Right_Half = 5,
    Output_Right_Left = 6,
    Output_Right_Left_Half = 7,
    Output_Top_Bottom = 8,
    Output_Top_Bottom_Half = 9,
    Output_Bottom_Top = 10,
    Output_Bottom_Top_Half = 11,
    Output_Even_Odd_Rows = 12,
    Output_Even_Odd_Columns = 13,
    Output_Checkerboard = 14,
    Output_Red_Cyan_Dubois = 15,
    Output_Red_Cyan_FullColor = 16,
    Output_Red_Cyan_HalfColor = 17,
    Output_Red_Cyan_Monochrome = 18,
    Output_Green_Magenta_Dubois = 19,
    Output_Green_Magenta_FullColor = 20,
    Output_Green_Magenta_HalfColor = 21,
    Output_Green_Magenta_Monochrome = 22,
    Output_Amber_Blue_Dubois = 23,
    Output_Amber_Blue_FullColor = 24,
    Output_Amber_Blue_HalfColor = 25,
    Output_Amber_Blue_Monochrome = 26,
    Output_Red_Green_Monochrome = 27,
    Output_Red_Blue_Monochrome = 28
};

const char* outputModeToString(OutputMode mode);
OutputMode outputModeFromString(const QString& s, bool* ok = nullptr);

/* Loop mode for the playlist */

enum LoopMode
{
    Loop_Off,
    Loop_One,
    Loop_All
};

const char* loopModeToString(LoopMode mode);
LoopMode loopModeFromString(const QString& s, bool* ok = nullptr);
