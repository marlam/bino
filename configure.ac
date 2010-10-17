dnl Process this file with autoconf to produce a configure script.

dnl configure.ac
dnl
dnl This file is part of bino, a program to play stereoscopic videos.
dnl
dnl Copyright (C) 2010  Martin Lambers <marlam@marlam.de>
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 3 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see <http://www.gnu.org/licenses/>.

dnl Autotools init stuff
AC_PREREQ([2.65])
AC_INIT([bino], [0.2], [marlam@marlam.de], [bino], [TODO])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_TARGET
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AM_INIT_AUTOMAKE([1.11.1 color-tests silent-rules tar-pax -Wall -Wno-portability])
AM_SILENT_RULES([yes])
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_LANG([C++])
AC_C_BIGENDIAN

dnl Useful definitions
AC_DEFINE_UNQUOTED([PLATFORM], ["${target}"], [Platform triplet])

dnl Feature checks needed by the base module
AC_CHECK_DECLS([optreset], [#include <getopt.h>])
AC_SEARCH_LIBS([iconv_open], [iconv])
AC_SEARCH_LIBS([clock_gettime], [rt])
AC_CHECK_FUNCS([backtrace clock_gettime])

dnl ffmpeg
PKG_CHECK_MODULES([libavformat], [libavformat >= 52.78.5], [HAVE_LIBAVFORMAT=1], [HAVE_LIBAVFORMAT=0])
if test "$HAVE_LIBAVFORMAT" != "1"; then
    AC_MSG_WARN([libavformat >= 52.78.5 not found:])
    AC_MSG_WARN([$libavformat_PKG_ERRORS])
    AC_MSG_WARN([libavformat is provided by ffmpeg; Debian package: libavformat-dev])
fi
PKG_CHECK_MODULES([libswscale], [libswscale >= 0.11.0], [HAVE_LIBSWSCALE=1], [HAVE_LIBSWSCALE=0])
if test "$HAVE_LIBSWSCALE" != "1"; then
    AC_MSG_WARN([libswscale >= 0.11.0 not found:])
    AC_MSG_WARN([$libswscale_PKG_ERRORS])
    AC_MSG_WARN([libswscale is provided by ffmpeg; Debian package: libswscale-dev])
fi

dnl glew
PKG_CHECK_MODULES([libglew], [glew >= 1.5.0], [HAVE_LIBGLEW=1], [HAVE_LIBGLEW=0])
if test "$HAVE_LIBGLEW" != "1"; then
    AC_MSG_WARN([libglew >= 1.5.0 not found:])
    AC_MSG_WARN([$libglew_PKG_ERRORS])
    AC_MSG_WARN([libglew is provided by glew; Debian package: libglew-dev])
fi

dnl glut
AC_CHECK_LIB([glut], [glutMainLoopEvent], [HAVE_LIBGLUT=1], [HAVE_LIBGLUT=0])
if test "$HAVE_LIBGLUT" != "1"; then
    AC_MSG_WARN([libglut (freeglut version) not found])
    AC_MSG_WARN([libglut (freeglut version) is provided by freeglut; Debian package: freeglut3-dev])
else
    AC_SUBST([libglut_CFLAGS], [])
    AC_SUBST([libglut_LIBS], [-lglut])
fi

dnl glu
PKG_CHECK_MODULES([libglu], [glu >= 0.0], [HAVE_LIBGLU=1], [HAVE_LIBGLU=0])
if test "$HAVE_LIBGLU" != "1"; then
    AC_MSG_WARN([libglu not found:])
    AC_MSG_WARN([$libglu_PKG_ERRORS])
    AC_MSG_WARN([libglu is provided by Mesa; Debian package: libglu-dev])
fi

dnl freealut
PKG_CHECK_MODULES([libalut], [freealut >= 0.0], [HAVE_LIBALUT=1], [HAVE_LIBALUT=0])
if test "$HAVE_LIBALUT" != "1"; then
    AC_MSG_WARN([libalut not found:])
    AC_MSG_WARN([$libalut_PKG_ERRORS])
    AC_MSG_WARN([libalut is provided by freealut; Debian package: libalut-dev])
fi

dnl Check if all required libraries were found
if test "$HAVE_LIBAVFORMAT" != "1" \
    -o "$HAVE_LIBSWSCALE" != "1" \
    -o "$HAVE_LIBGLEW" != "1" \
    -o "$HAVE_LIBGLUT" != "1" \
    -o "$HAVE_LIBGLU" != "1" \
    -o "$HAVE_LIBALUT" != "1"; then
    AC_MSG_ERROR([Required libraries were not found. See messages above.])
fi

dnl Output
AC_CONFIG_FILES([Makefile	\
	src/base/Makefile	\
	src/Makefile		\
	doc/Makefile		\
	])
AC_OUTPUT
