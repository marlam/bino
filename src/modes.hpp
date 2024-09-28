/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022, 2023, 2024
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
    Input_Unknown = 0,         // unknown; needs to be guessed
    Input_Mono = 1,            // monoscopic video (2D)
    Input_Top_Bottom = 2,      // stereoscopic video, left eye top, right eye bottom
    Input_Top_Bottom_Half = 3, // stereoscopic video, left eye top, right eye bottom, both half height
    Input_Bottom_Top = 4,      // stereoscopic video, left eye bottom, right eye top
    Input_Bottom_Top_Half = 5, // stereoscopic video, left eye bottom, right eye top, both half height
    Input_Left_Right = 6,      // stereoscopic video, left eye left, right eye right
    Input_Left_Right_Half = 7, // stereoscopic video, left eye left, right eye right, both half width
    Input_Right_Left = 8,      // stereoscopic video, left eye right, right eye left
    Input_Right_Left_Half = 9, // stereoscopic video, left eye right, right eye left, both half width
    Input_Alternating_LR = 10, // stereoscopic video, alternating frames, left first
    Input_Alternating_RL = 11, // stereoscopic video, alternating frames, right first
};

const char* inputModeToString(InputMode mode);
QString inputModeToStringUI(InputMode mode);
InputMode inputModeFromString(const QString& s, bool* ok = nullptr);

/* Surround mode (180° / 360°) */
enum SurroundMode {
    Surround_Unknown = 0,     // unknown; needs to be guessed
    Surround_Off = 1,         // conventional video
    Surround_180 = 2,         // 180° video
    Surround_360 = 3,         // 360° video
};

const char* surroundModeToString(SurroundMode mode);
QString surroundModeToStringUI(SurroundMode mode);
SurroundMode surroundModeFromString(const QString& s, bool* ok = nullptr);

/* Output mode: Is the output 2D or 3D, and in the latter case, how should
 * the left and right view be arranged on screen? */
// Note that this is mirrored in shader-display-frag.glsl!
enum OutputMode {
    Output_Left = 0,
    Output_Right = 1,
    Output_OpenGL_Stereo = 2,
    Output_Alternating = 3,
    Output_HDMI_Frame_Pack = 4,
    Output_Left_Right = 5,
    Output_Left_Right_Half = 6,
    Output_Right_Left = 7,
    Output_Right_Left_Half = 8,
    Output_Top_Bottom = 9,
    Output_Top_Bottom_Half = 10,
    Output_Bottom_Top = 11,
    Output_Bottom_Top_Half = 12,
    Output_Even_Odd_Rows = 13,
    Output_Even_Odd_Columns = 14,
    Output_Checkerboard = 15,
    Output_Red_Cyan_Dubois = 16,
    Output_Red_Cyan_FullColor = 17,
    Output_Red_Cyan_HalfColor = 18,
    Output_Red_Cyan_Monochrome = 19,
    Output_Green_Magenta_Dubois = 20,
    Output_Green_Magenta_FullColor = 21,
    Output_Green_Magenta_HalfColor = 22,
    Output_Green_Magenta_Monochrome = 23,
    Output_Amber_Blue_Dubois = 24,
    Output_Amber_Blue_FullColor = 25,
    Output_Amber_Blue_HalfColor = 26,
    Output_Amber_Blue_Monochrome = 27,
    Output_Red_Green_Monochrome = 28,
    Output_Red_Blue_Monochrome = 29
};

const char* outputModeToString(OutputMode mode);
QString outputModeToStringUI(OutputMode mode);
OutputMode outputModeFromString(const QString& s, bool* ok = nullptr);

/* Loop mode for the playlist */

enum LoopMode
{
    Loop_Off,
    Loop_One,
    Loop_All
};

const char* loopModeToString(LoopMode mode);
QString loopModeToStringUI(LoopMode mode);
LoopMode loopModeFromString(const QString& s, bool* ok = nullptr);

/* Wait mode for the playlist */

enum WaitMode
{
    Wait_Off,
    Wait_On
};

const char* waitModeToString(WaitMode mode);
QString waitModeToStringUI(WaitMode mode);
WaitMode waitModeFromString(const QString& s, bool* ok = nullptr);
