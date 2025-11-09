/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2022, 2023, 2024, 2025
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

#include <QCoreApplication>

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

QString inputModeToStringUI(InputMode mode)
{
    switch (mode) {
    case Input_Unknown:
        return QCoreApplication::translate("Mode", "Input autodetection");
        break;
    case Input_Mono:
        return QCoreApplication::translate("Mode", "Input 2D");
        break;
    case Input_Top_Bottom:
        return QCoreApplication::translate("Mode", "Input top/bottom");
        break;
    case Input_Top_Bottom_Half:
        return QCoreApplication::translate("Mode", "Input top/bottom half height");
        break;
    case Input_Bottom_Top:
        return QCoreApplication::translate("Mode", "Input bottom/top");
        break;
    case Input_Bottom_Top_Half:
        return QCoreApplication::translate("Mode", "Input bottom/top half height");
        break;
    case Input_Left_Right:
        return QCoreApplication::translate("Mode", "Input left/right");
        break;
    case Input_Left_Right_Half:
        return QCoreApplication::translate("Mode", "Input left/right half width");
        break;
    case Input_Right_Left:
        return QCoreApplication::translate("Mode", "Input right/left");
        break;
    case Input_Right_Left_Half:
        return QCoreApplication::translate("Mode", "Input right/left half width");
        break;
    case Input_Alternating_LR:
        return QCoreApplication::translate("Mode", "Input alternating left/right");
        break;
    case Input_Alternating_RL:
        return QCoreApplication::translate("Mode", "Input alternating right/left");
        break;
    };
    return QString();
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

const char* surroundModeToString(SurroundMode mode)
{
    switch (mode) {
    case Surround_Unknown:
        return "unknown";
        break;
    case Surround_Off:
        return "off";
        break;
    case Surround_180:
        return "180";
        break;
    case Surround_360:
        return "360";
        break;
    }
    return nullptr;
}

QString surroundModeToStringUI(SurroundMode mode)
{
    switch (mode) {
    case Surround_Unknown:
        return QCoreApplication::translate("Mode", "Surround autodetection");
        break;
    case Surround_Off:
        return QCoreApplication::translate("Mode", "Surround off");
        break;
    case Surround_180:
        return QCoreApplication::translate("Mode", "Surround 180°");
        break;
    case Surround_360:
        return QCoreApplication::translate("Mode", "Surround 360°");
        break;
    }
    return QString();
}

SurroundMode surroundModeFromString(const QString& s, bool* ok)
{
    SurroundMode mode = Surround_Unknown;
    bool r = true;
    if (s == "off")
        mode = Surround_Off;
    else if (s == "180")
        mode = Surround_180;
    else if (s == "360")
        mode = Surround_360;
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
    case Output_HDMI_Frame_Pack:
        return "hdmi-frame-pack";
        break;
    case Output_Left_Right:
        return "left-right";
        break;
    case Output_Left_Right_Half:
        return "left-right-half";
        break;
    case Output_Right_Left:
        return "right-left";
        break;
    case Output_Right_Left_Half:
        return "right-left-half";
        break;
    case Output_Top_Bottom:
        return "top-bottom";
        break;
    case Output_Top_Bottom_Half:
        return "top-bottom-half";
        break;
    case Output_Bottom_Top:
        return "bottom-top";
        break;
    case Output_Bottom_Top_Half:
        return "bottom-top-half";
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
    }
    return nullptr;
}

QString outputModeToStringUI(OutputMode mode)
{
    switch (mode) {
    case Output_Left:
        return QCoreApplication::translate("Mode", "Output 2D left");
        break;
    case Output_Right:
        return QCoreApplication::translate("Mode", "Output 2D right");
        break;
    case Output_OpenGL_Stereo:
        return QCoreApplication::translate("Mode", "Output 3D OpenGL Stereo");
        break;
    case Output_Alternating:
        return QCoreApplication::translate("Mode", "Output 3D alternating");
        break;
    case Output_HDMI_Frame_Pack:
        return QCoreApplication::translate("Mode", "Output HDMI frame pack");
        break;
    case Output_Left_Right:
        return QCoreApplication::translate("Mode", "Output left/right");
        break;
    case Output_Left_Right_Half:
        return QCoreApplication::translate("Mode", "Output left/right half width");
        break;
    case Output_Right_Left:
        return QCoreApplication::translate("Mode", "Output right/left");
        break;
    case Output_Right_Left_Half:
        return QCoreApplication::translate("Mode", "Output right/left half width");
        break;
    case Output_Top_Bottom:
        return QCoreApplication::translate("Mode", "Output top/bottom");
        break;
    case Output_Top_Bottom_Half:
        return QCoreApplication::translate("Mode", "Output top/bottom half height");
        break;
    case Output_Bottom_Top:
        return QCoreApplication::translate("Mode", "Output bottom/top");
        break;
    case Output_Bottom_Top_Half:
        return QCoreApplication::translate("Mode", "Output bottom/top half height");
        break;
    case Output_Even_Odd_Rows:
        return QCoreApplication::translate("Mode", "Output even/odd rows");
        break;
    case Output_Even_Odd_Columns:
        return QCoreApplication::translate("Mode", "Output even/odd columns");
        break;
    case Output_Checkerboard:
        return QCoreApplication::translate("Mode", "Output checkerboard");
        break;
    case Output_Red_Cyan_Dubois:
        return QCoreApplication::translate("Mode", "Output red/cyan high quality");
        break;
    case Output_Red_Cyan_FullColor:
        return QCoreApplication::translate("Mode", "Output red/cyan full color");
        break;
    case Output_Red_Cyan_HalfColor:
        return QCoreApplication::translate("Mode", "Output red/cyan half color");
        break;
    case Output_Red_Cyan_Monochrome:
        return QCoreApplication::translate("Mode", "Output red/cyan monochrome");
        break;
    case Output_Green_Magenta_Dubois:
        return QCoreApplication::translate("Mode", "Output green/magenta high quality");
        break;
    case Output_Green_Magenta_FullColor:
        return QCoreApplication::translate("Mode", "Output green/magenta full color");
        break;
    case Output_Green_Magenta_HalfColor:
        return QCoreApplication::translate("Mode", "Output green/magenta half color");
        break;
    case Output_Green_Magenta_Monochrome:
        return QCoreApplication::translate("Mode", "Output green/magenta monochrome");
        break;
    case Output_Amber_Blue_Dubois:
        return QCoreApplication::translate("Mode", "Output amber/blue high quality");
        break;
    case Output_Amber_Blue_FullColor:
        return QCoreApplication::translate("Mode", "Output amber/blue full color");
        break;
    case Output_Amber_Blue_HalfColor:
        return QCoreApplication::translate("Mode", "Output amber/blue half color");
        break;
    case Output_Amber_Blue_Monochrome:
        return QCoreApplication::translate("Mode", "Output amber/blue monochrome");
        break;
    case Output_Red_Green_Monochrome:
        return QCoreApplication::translate("Mode", "Output red/green monochrome");
        break;
    case Output_Red_Blue_Monochrome:
        return QCoreApplication::translate("Mode", "Output red/blue monochrome");
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
    else if (s == "hdmi-frame-pack")
        mode = Output_HDMI_Frame_Pack;
    else if (s == "left-right")
        mode = Output_Left_Right;
    else if (s == "left-right-half")
        mode = Output_Left_Right_Half;
    else if (s == "right-left")
        mode = Output_Right_Left;
    else if (s == "right-left-half")
        mode = Output_Right_Left_Half;
    else if (s == "top-bottom")
        mode = Output_Top_Bottom;
    else if (s == "top-bottom-half")
        mode = Output_Top_Bottom_Half;
    else if (s == "bottom-top")
        mode = Output_Bottom_Top;
    else if (s == "bottom-top-half")
        mode = Output_Bottom_Top_Half;
    else if (s == "even-odd-rows")
        mode = Output_Even_Odd_Rows;
    else if (s == "even-odd-columns")
        mode = Output_Even_Odd_Columns;
    else if (s == "checkerboard")
        mode = Output_Checkerboard;
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
    else
        r = false;
    if (ok)
        *ok = r;
    return mode;
}

const char* loopModeToString(LoopMode mode)
{
    switch (mode) {
    case Loop_Off:
        return "off";
        break;
    case Loop_One:
        return "one";
        break;
    case Loop_All:
        return "all";
        break;
    }
    return nullptr;

}

QString loopModeToStringUI(LoopMode mode)
{
    switch (mode) {
    case Loop_Off:
        return QCoreApplication::translate("Mode", "Loop off");
        break;
    case Loop_One:
        return QCoreApplication::translate("Mode", "Loop one");
        break;
    case Loop_All:
        return QCoreApplication::translate("Mode", "Loop all");
        break;
    }
    return QString();
}

LoopMode loopModeFromString(const QString& s, bool* ok)
{
    LoopMode mode = Loop_Off;
    bool r = true;
    if (s == "off")
        mode = Loop_Off;
    else if (s == "one")
        mode = Loop_One;
    else if (s == "all")
        mode = Loop_All;
    else
        r = false;
    if (ok)
        *ok = r;
    return mode;
}

const char* waitModeToString(WaitMode mode)
{
    switch (mode) {
    case Wait_Off:
        return "off";
        break;
    case Wait_On:
        return "on";
        break;
    }
    return nullptr;

}

QString waitModeToStringUI(WaitMode mode)
{
    switch (mode) {
    case Wait_Off:
        return QCoreApplication::translate("Mode", "Wait off");
        break;
    case Wait_On:
        return QCoreApplication::translate("Mode", "Wait on");
        break;
    }
    return QString();
}

WaitMode waitModeFromString(const QString& s, bool* ok)
{
    WaitMode mode = Wait_Off;
    bool r = true;
    if (s == "off")
        mode = Wait_Off;
    else if (s == "on")
        mode = Wait_On;
    else
        r = false;
    if (ok)
        *ok = r;
    return mode;
}
