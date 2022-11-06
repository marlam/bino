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

#include "modes.hpp"

const char* inputModeToString(InputMode mode)
{
    switch (mode) {
    case Input_Unknown:
        return "unknown";
        break;
    case Input_Mono:
        return "mono";
        break;
    case Input_Top_Bottom:
        return "top-bottom";
        break;
    case Input_Top_Bottom_Half:
        return "top-bottom-half";
        break;
    case Input_Bottom_Top:
        return "bottom-top";
        break;
    case Input_Bottom_Top_Half:
        return "bottom-top-half";
        break;
    case Input_Left_Right:
        return "left-right";
        break;
    case Input_Left_Right_Half:
        return "left-right-half";
        break;
    case Input_Right_Left:
        return "right-left";
        break;
    case Input_Right_Left_Half:
        return "right-left-half";
        break;
    case Input_Alternating_LR:
        return "alternating-left-right";
        break;
    case Input_Alternating_RL:
        return "alternating-right-left";
        break;
    };
    return nullptr;
}

InputMode inputModeFromString(const QString& s, bool* ok)
{
    InputMode mode = Input_Unknown;
    bool r = true;
    if (s == "mono")
        mode = Input_Mono;
    else if (s == "top-bottom")
        mode = Input_Top_Bottom;
    else if (s == "top-bottom-half")
        mode = Input_Top_Bottom_Half;
    else if (s == "bottom-top")
        mode = Input_Bottom_Top;
    else if (s == "bottom-top-half")
        mode = Input_Bottom_Top_Half;
    else if (s == "left-right")
        mode = Input_Left_Right;
    else if (s == "left-right-half")
        mode = Input_Left_Right_Half;
    else if (s == "right-left")
        mode = Input_Right_Left;
    else if (s == "right-left-half")
        mode = Input_Right_Left_Half;
    else if (s == "alternating-left-right")
        mode = Input_Alternating_LR;
    else if (s == "alternating-right-left")
        mode = Input_Alternating_RL;
    else
        r = false;
    if (ok)
        *ok = r;
    return mode;
}

const char* threeSixtyModeToString(ThreeSixtyMode mode)
{
    switch (mode) {
    case ThreeSixty_Unknown:
        return "unknown";
        break;
    case ThreeSixty_On:
        return "on";
        break;
    case ThreeSixty_Off:
        return "Off";
        break;
    }
    return nullptr;
}

ThreeSixtyMode threeSixtyModeFromString(const QString& s, bool* ok)
{
    ThreeSixtyMode mode = ThreeSixty_Unknown;
    bool r = true;
    if (s == "on")
        mode = ThreeSixty_On;
    else if (s == "off")
        mode = ThreeSixty_Off;
    else
        r = false;
    if (ok)
        *ok = r;
    return mode;
}

const char* outputModeToString(OutputMode mode)
{
    switch (mode) {
    case Output_Left:
        return "left";
        break;
    case Output_Right:
        return "right";
        break;
    case Output_OpenGL_Stereo:
        return "stereo";
        break;
    case Output_Alternating:
        return "alternating";
        break;
    case Output_Red_Cyan_Dubois:
        return "red-cyan-dubois";
        break;
    case Output_Red_Cyan_FullColor:
        return "red-cyan-fullcolor";
        break;
    case Output_Red_Cyan_HalfColor:
        return "red-cyan-halfcolor";
        break;
    case Output_Red_Cyan_Monochrome:
        return "red-cyan-monochrome";
        break;
    case Output_Green_Magenta_Dubois:
        return "green-magenta-dubois";
        break;
    case Output_Green_Magenta_FullColor:
        return "green-magenta-fullcolor";
        break;
    case Output_Green_Magenta_HalfColor:
        return "green-magenta-halfcolor";
        break;
    case Output_Green_Magenta_Monochrome:
        return "green-magenta-monochrome";
        break;
    case Output_Amber_Blue_Dubois:
        return "amber-blue-dubois";
        break;
    case Output_Amber_Blue_FullColor:
        return "amber-blue-fullcolor";
        break;
    case Output_Amber_Blue_HalfColor:
        return "amber-blue-halfcolor";
        break;
    case Output_Amber_Blue_Monochrome:
        return "amber-blue-monochrome";
        break;
    case Output_Red_Green_Monochrome:
        return "red-green-monochrome";
        break;
    case Output_Red_Blue_Monochrome:
        return "red-blue-monochrome";
        break;
    case Output_Even_Odd_Rows:
        return "even-odd-rows";
        break;
    case Output_Even_Odd_Columns:
        return "even-odd-columns";
        break;
    case Output_Checkerboard:
        return "checkeroard";
        break;
    }
    return nullptr;
}

OutputMode outputModeFromString(const QString& s, bool* ok)
{
    OutputMode mode = Output_Left;
    bool r = true;
    if (s == "left")
        mode = Output_Left;
    else if (s == "right")
        mode = Output_Right;
    else if (s == "stereo")
        mode = Output_OpenGL_Stereo;
    else if (s == "alternating")
        mode = Output_Alternating;
    else if (s == "red-cyan-dubois")
        mode = Output_Red_Cyan_Dubois;
    else if (s == "red-cyan-fullcolor")
        mode = Output_Red_Cyan_FullColor;
    else if (s == "red-cyan-halfcolor")
        mode = Output_Red_Cyan_HalfColor;
    else if (s == "red-cyan-monochrome")
        mode = Output_Red_Cyan_Monochrome;
    else if (s == "green-magenta-dubois")
        mode = Output_Green_Magenta_Dubois;
    else if (s == "green-magenta-fullcolor")
        mode = Output_Green_Magenta_FullColor;
    else if (s == "green-magenta-halfcolor")
        mode = Output_Green_Magenta_HalfColor;
    else if (s == "green-magenta-monochrome")
        mode = Output_Green_Magenta_Monochrome;
    else if (s == "amber-blue-dubois")
        mode = Output_Amber_Blue_Dubois;
    else if (s == "amber-blue-fullcolor")
        mode = Output_Amber_Blue_FullColor;
    else if (s == "amber-blue-halfcolor")
        mode = Output_Amber_Blue_HalfColor;
    else if (s == "amber-blue-monochrome")
        mode = Output_Amber_Blue_Monochrome;
    else if (s == "red-green-monochrome")
        mode = Output_Red_Green_Monochrome;
    else if (s == "red-blue-monochrome")
        mode = Output_Red_Blue_Monochrome;
    else if (s == "even-odd-rows")
        mode = Output_Even_Odd_Rows;
    else if (s == "even-odd-columns")
        mode = Output_Even_Odd_Columns;
    else if (s == "checkerboard")
        mode = Output_Checkerboard;
    else
        r = false;
    if (ok)
        *ok = r;
    return mode;
}
