/*
 * This file is part of Bino, a 3D video player.
 *
 * Copyright (C) 2016, 2017 Computer Graphics Group, University of Siegen
 * Written by Martin Lambers <martin.lambers@uni-siegen.de>
 *
 * Copyright (C) 2022
 * Martin Lambers <marlam@marlam.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* This logging functionality is adapted from QVR */

#pragma once

#include <cstdio>

typedef enum {
    /*! Print only fatal errors */
    Log_Level_Fatal = 0,
    /*! Additionally print warnings (default) */
    Log_Level_Warning = 1,
    /*! Additionally print informational messages */
    Log_Level_Info = 2,
    /*! Additionally print debugging information */
    Log_Level_Debug = 3,
    /*! Additionally print super verbose debugging information */
    Log_Level_Firehose = 4
} LogLevel;

void SetLogLevel(LogLevel l);
LogLevel GetLogLevel();

void SetLogFile(const char* name, bool truncate); /* nullptr means stderr */
const char* GetLogFile(); /* nullptr means stderr */

/* Send one line to the log (\n will be appended) */
void Log(LogLevel level, const char* s);

#define LOG_BUFSIZE 1024

#define LOG_MSG(level, ...) { char buf[LOG_BUFSIZE]; snprintf(buf, LOG_BUFSIZE, __VA_ARGS__); Log(level, buf); }
#define LOG_REQUESTED(...)  { LOG_MSG(Log_Level_Info, __VA_ARGS__); }
#define LOG_FATAL(...)      { LOG_MSG(Log_Level_Fatal, __VA_ARGS__); }
#define LOG_WARNING(...)    { if (GetLogLevel() >= Log_Level_Warning)  { LOG_MSG(Log_Level_Warning, __VA_ARGS__); } }
#define LOG_INFO(...)       { if (GetLogLevel() >= Log_Level_Info)     { LOG_MSG(Log_Level_Info, __VA_ARGS__); } }
#define LOG_DEBUG(...)      { if (GetLogLevel() >= Log_Level_Debug)    { LOG_MSG(Log_Level_Debug, __VA_ARGS__); } }
#define LOG_FIREHOSE(...)   { if (GetLogLevel() >= Log_Level_Firehose) { LOG_MSG(Log_Level_Firehose, __VA_ARGS__); } }
